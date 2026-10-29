#include "psprecomp/common.hpp"
#include "psprecomp/elf32.hpp"
#include "psprecomp/runtime.hpp"
#include "psprecomp/sha256.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#if defined(PSPRECOMP_PES2013_OVERLAYS)
#include "overlay_bank.hpp"
#endif

#if defined(PSPRECOMP_PES2013_SHARED_HOST)
#include "vcs_config.hpp"
#include "vcs_profile.hpp"
#include "audio_output.hpp"
#include "ge_gpu_backend.hpp"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <thread>

void android_game_runtime_set_status(const char *status) {
    std::cout << "[host-status] " << status << '\n';
}

namespace {
// NDK 28's libc++ does not provide std::jthread on this target.
class RuntimeWatchdog {
    psprecomp::Runtime &runtime_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool finished_{};
    bool expired_{};
    std::thread worker_;
public:
    explicit RuntimeWatchdog(psprecomp::Runtime &runtime) : runtime_(runtime), worker_([this] {
        std::unique_lock lock(mutex_);
        if (!condition_.wait_for(lock, std::chrono::seconds(20), [this] { return finished_; })) {
            expired_ = true;
            runtime_.request_stop();
        }
    }) {}
    ~RuntimeWatchdog() {
        { std::lock_guard lock(mutex_); finished_ = true; }
        condition_.notify_one();
        worker_.join();
    }
    bool expired() {
        std::lock_guard lock(mutex_);
        return expired_;
    }
};
}
#endif

// This is a diagnostic bootstrap, not the game host. In particular, missing
// PSP services must stop execution instead of pretending to succeed.
int main(int argc, char **argv) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: pes2013_probe <PES game root containing PSP_GAME>\n";
            return 2;
        }
        const auto root = std::filesystem::canonical(argv[1]);
        const auto executable = root / "PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF";
        const auto hash = psprecomp::sha256_file(executable);
        if (hash != PES2013_EXPECTED_SHA256)
            throw psprecomp::Error("Executable does not match the ULUS10597 PES profile");
        auto image = psprecomp::Elf32Image::from_file(executable);
        if (!std::getenv("PSPRECOMP_CHAIN_DEPTH")) {
#if defined(_WIN32)
            _putenv_s("PSPRECOMP_CHAIN_DEPTH", "8");
#else
            setenv("PSPRECOMP_CHAIN_DEPTH", "8", 0);
#endif
        }
        auto runtime = std::make_unique<psprecomp::Runtime>(32u * 1024u * 1024u);
        runtime->set_game_root(root);
        const auto relocations = image.load_and_relocate(
            runtime->memory(), psprecomp::kDefaultPspUserLoadBase);
        if (relocations.invalid != 0u || relocations.unsupported != 0u)
            throw psprecomp::Error("ELF contains unsupported or invalid relocations");
        const auto module = image.find_module_info(
            runtime->memory(), psprecomp::kDefaultPspUserLoadBase);
        if (!module) throw psprecomp::Error("PES module information was not found");
        psprecomp::register_generated_functions(*runtime);
#if defined(PSPRECOMP_PES2013_OVERLAYS)
        pes2013::install_overlay_bank(*runtime);
#endif
#if defined(PSPRECOMP_PES2013_SHARED_HOST)
        const auto app_data = root / "probe-state";
        std::filesystem::create_directories(app_data);
        // Headless service test: no Android window or GPU is initialized.
        const auto ini = app_data / "VCSNative.ini";
        // This directory belongs solely to the diagnostic process, so each
        // run sets a reproducible, silent configuration without a window.
        {
            std::ofstream settings(ini);
            if (!settings) throw psprecomp::Error("Cannot create headless probe configuration");
            settings << "[Rendering]\nBackend=Software\n[Timing]\nFrameRate=60\n[Audio]\nEnabled=false\n";
        }
        vcs::initialize_vcs_configuration(app_data);
        std::uint64_t image_end = 0u;
        for (std::size_t index = 0u; index < image.segments().size(); ++index) {
            const auto &segment = image.segments()[index];
            if (segment.type != 1u) continue;
            image_end = std::max(image_end,
                static_cast<std::uint64_t>(image.segment_runtime_address(
                    index, psprecomp::kDefaultPspUserLoadBase)) + segment.memory_size);
        }
        if (image_end == 0u || image_end >= 0x09FF0000ull)
            throw psprecomp::Error("PES image leaves no room for the loader stack");
        vcs::install_profile(*runtime, static_cast<std::uint32_t>((image_end + 255u) & ~255ull));
        vcs::install_starvation_preemption();
#endif
        auto &cpu = runtime->cpu();
        cpu.set_gpr(28u, module->gp);
#if !defined(PSPRECOMP_PES2013_SHARED_HOST)
        cpu.set_gpr(29u, 0x09FFF000u);
#endif
        cpu.set_gpr(31u, 0u);
        cpu.set_gpr(4u, 0u);
        cpu.set_gpr(5u, 0u);
        std::cout << "PES 2013 ULUS10597 diagnostic bootstrap\n"
                  << "SHA-256: " << hash << '\n'
                  << "Module: " << module->name << '\n'
                  << "Entry: " << psprecomp::hex32(image.runtime_entry()) << '\n'
                  << "Registered entries: " << runtime->function_count() << std::endl;
#if defined(PSPRECOMP_PES2013_SHARED_HOST)
        RuntimeWatchdog watchdog(*runtime);
#endif
        bool execution_failed = false;
        try {
            runtime->run(image.runtime_entry(), 100000u);
        } catch (const std::exception &error) {
            execution_failed = true;
            runtime->stop(std::string("Execution error: ") + error.what());
        }
#if defined(PSPRECOMP_PES2013_SHARED_HOST)
        if (watchdog.expired()) runtime->stop("Host bootstrap watchdog expired after 20 seconds");
#endif
        std::cout << "Stop: " << runtime->stop_reason() << '\n'
                  << "Guest PC: " << psprecomp::hex32(cpu.pc) << '\n';
#if defined(PSPRECOMP_PES2013_SHARED_HOST)
        runtime->report_hle_histogram();
        vcs::report_disc_read_stats();
        vcs::report_present_stats();
        vcs::audio_output_shutdown();
        vcs::shutdown_ge_gpu_backend();
        if (std::getenv("PSPRECOMP_DUMP_GUEST") != nullptr) {
            const auto dump = app_data / "ram.bin";
            const auto size = runtime->memory().size();
            const auto *bytes = runtime->memory().raw_pointer(psprecomp::GuestMemory::kPhysicalBase, size);
            if (!bytes) throw psprecomp::Error("Cannot access guest RAM for diagnostic dump");
            std::ofstream output(dump, std::ios::binary | std::ios::trunc);
            output.write(reinterpret_cast<const char *>(bytes), size);
            if (!output) throw psprecomp::Error("Cannot write diagnostic RAM dump");
            std::cout << "Guest RAM dump: " << dump.string() << '\n';
            for (unsigned index = 0; index < 32; ++index)
                std::cout << "r" << index << "=" << psprecomp::hex32(cpu.gpr[index]) << (index % 4 == 3 ? '\n' : ' ');
        }
#endif
        // An unimplemented service is the expected diagnostic result, not a
        // successful game boot. Preserve that distinction in the exit status.
        return execution_failed ? 1 : 3;
    } catch (const std::exception &error) {
        std::cerr << "PES bootstrap error: " << error.what() << '\n';
        return 1;
    }
}
