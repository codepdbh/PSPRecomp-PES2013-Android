#include "display_window_android.hpp"
#include "audio_output.hpp"
#include "ge_gpu_backend.hpp"
#include "overlay_bank.hpp"
#include "vcs_config.hpp"
#include "vcs_profile.hpp"
#include "psprecomp/common.hpp"
#include "psprecomp/elf32.hpp"
#include "psprecomp/sha256.hpp"
#include <android/log.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <pthread.h>
namespace {
std::mutex state_mutex;
std::string status = "Esperando los datos de PES 2013.";
psprecomp::Runtime *running{};
pthread_t worker{};
bool worker_joinable{};
std::atomic_bool active{};
std::atomic_bool cancel{};
void set_status(std::string value) {
    std::lock_guard lock(state_mutex); status = std::move(value);
    __android_log_print(ANDROID_LOG_INFO, "PES2013", "%s", status.c_str());
}
std::string java_string(JNIEnv *env, jstring value) {
    if (!value) return {};
    const auto *data = env->GetStringUTFChars(value, nullptr);
    if (!data) return {};
    std::string result(data); env->ReleaseStringUTFChars(value, data); return result;
}
void stop_game() {
    cancel = true;
    // Also cover a stop requested while the worker is still loading the ELF:
    // Runtime::run resets its stop flag at entry, so keep the lifecycle request
    // asserted until the worker acknowledges it.
    while (active) {
        { std::lock_guard lock(state_mutex); if (running) running->request_stop(); }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (worker_joinable) { pthread_join(worker, nullptr); worker_joinable = false; }
}
void play(std::filesystem::path root, std::filesystem::path settings_dir) {
    std::unique_ptr<psprecomp::Runtime> runtime;
    try {
        const auto executable = root / "PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF";
        if (!std::filesystem::is_regular_file(executable))
            throw psprecomp::Error("Falta PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF en " + root.string());
        if (psprecomp::sha256_file(executable) != "5ab84864354b3ee32cf4dbb1a7e0bb0fc914d63b0f25cddfa387af35e09a14db")
            throw psprecomp::Error("El ejecutable no corresponde a PES 2013 ULUS10597.");
        std::filesystem::create_directories(settings_dir);
        const auto ini = settings_dir / "VCSNative.ini";
        if (!std::filesystem::exists(ini)) {
            std::ofstream config(ini);
            config << "[Rendering]\nBackend=Vulkan\n[Timing]\nFrameRate=60\n[Audio]\nEnabled=true\n";
        }
        vcs::initialize_vcs_configuration(settings_dir);
        auto image = psprecomp::Elf32Image::from_file(executable);
        // Bound nesting while the diagnostic corpus is built without optimization.
        setenv("PSPRECOMP_CHAIN_DEPTH", "8", 0);
        runtime = std::make_unique<psprecomp::Runtime>(32u * 1024u * 1024u);
        runtime->set_game_root(root);
        const auto relocations = image.load_and_relocate(runtime->memory(), psprecomp::kDefaultPspUserLoadBase);
        if (relocations.invalid || relocations.unsupported) throw psprecomp::Error("Relocaciones ELF no soportadas.");
        const auto module = image.find_module_info(runtime->memory(), psprecomp::kDefaultPspUserLoadBase);
        if (!module) throw psprecomp::Error("No se encontro el modulo de inicio.");
        std::uint64_t end{};
        for (unsigned i = 0; i < image.segments().size(); ++i) {
            const auto &segment = image.segments()[i];
            if (segment.type == 1u) end = std::max(end, std::uint64_t(image.segment_runtime_address(i,
                psprecomp::kDefaultPspUserLoadBase)) + segment.memory_size);
        }
        if (!end || end >= 0x09FF0000ull) throw psprecomp::Error("Mapa de memoria ELF invalido.");
        psprecomp::register_generated_functions(*runtime);
        pes2013::install_overlay_bank(*runtime);
        vcs::install_profile(*runtime, static_cast<std::uint32_t>((end + 255u) & ~255ull));
        vcs::install_starvation_preemption();
        runtime->cpu().set_gpr(28, module->gp);
        std::string gpu_error;
        if (!vcs::initialize_ge_gpu_backend(gpu_error)) throw psprecomp::Error(gpu_error);
        vcs::display_window_start();
        { std::lock_guard lock(state_mutex); running = runtime.get(); }
        set_status("Iniciando PES 2013...");
        if (!cancel) runtime->run(image.runtime_entry(), 4'000'000'000ull);
        { std::lock_guard lock(state_mutex); running = nullptr; }
        vcs::report_present_stats();
        vcs::audio_output_shutdown(); vcs::shutdown_ge_gpu_backend();
        set_status("PES se detuvo: " + runtime->stop_reason());
    } catch (const std::exception &error) {
        { std::lock_guard lock(state_mutex); running = nullptr; }
        if (runtime) vcs::report_present_stats();
        vcs::audio_output_shutdown(); vcs::shutdown_ge_gpu_backend();
        set_status(error.what());
    }
    active = false;
}
}
void android_game_runtime_set_status(const char *value) {
    const std::string message = value ? value : "";
    set_status(message.starts_with("Juego en marcha") ? "Imagen recibida." : message);
}
extern "C" JNIEXPORT jstring JNICALL Java_com_psprecomp_pes2013_MainActivity_nativeStatus(JNIEnv *env, jclass) {
    std::lock_guard lock(state_mutex); return env->NewStringUTF(status.c_str());
}
extern "C" JNIEXPORT void JNICALL Java_com_psprecomp_pes2013_MainActivity_nativeSurface(JNIEnv *env, jclass, jobject surface) {
    auto *window = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
    vcs::display_window_set_surface(window);
    if (window) ANativeWindow_release(window);
}
extern "C" JNIEXPORT void JNICALL Java_com_psprecomp_pes2013_MainActivity_nativeStart(JNIEnv *env, jclass, jstring root, jstring settings) {
    if (active.exchange(true)) return;
    cancel = false;
    if (worker_joinable) { pthread_join(worker, nullptr); worker_joinable = false; }
    struct Paths { std::string root, settings; };
    auto paths = std::make_unique<Paths>(Paths{java_string(env, root), java_string(env, settings)});
    // The unoptimized diagnostic AOT units have large native frames. Bionic's
    // default pthread stack is smaller than the standalone probe's main stack.
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, 16u * 1024u * 1024u);
    const int error = pthread_create(&worker, &attributes, +[](void *raw) -> void * {
        std::unique_ptr<Paths> args(static_cast<Paths *>(raw));
        play(args->root, args->settings); return nullptr;
    }, paths.get());
    pthread_attr_destroy(&attributes);
    if (error) { active = false; set_status("No se pudo crear el hilo del juego."); return; }
    paths.release(); worker_joinable = true;
}
extern "C" JNIEXPORT void JNICALL Java_com_psprecomp_pes2013_MainActivity_nativeStop(JNIEnv *, jclass) { stop_game(); }
extern "C" JNIEXPORT void JNICALL Java_com_psprecomp_pes2013_MainActivity_nativeInput(JNIEnv *, jclass, jint buttons, jint x, jint y) {
    vcs::HostInputState input{};
    input.buttons = buttons; input.analog_x = std::clamp(x, 0, 255); input.analog_y = std::clamp(y, 0, 255);
    vcs::display_window_set_input(input);
}
