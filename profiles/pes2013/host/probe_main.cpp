#include "psprecomp/common.hpp"
#include "psprecomp/elf32.hpp"
#include "psprecomp/runtime.hpp"
#include "psprecomp/sha256.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>

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
        auto &cpu = runtime->cpu();
        cpu.set_gpr(28u, module->gp);
        cpu.set_gpr(29u, 0x09FFF000u);
        cpu.set_gpr(31u, 0u);
        cpu.set_gpr(4u, 0u);
        cpu.set_gpr(5u, 0u);
        std::cout << "PES 2013 ULUS10597 diagnostic bootstrap\n"
                  << "SHA-256: " << hash << '\n'
                  << "Module: " << module->name << '\n'
                  << "Entry: " << psprecomp::hex32(image.runtime_entry()) << '\n'
                  << "Registered entries: " << runtime->function_count() << std::endl;
        runtime->run(image.runtime_entry(), 100000u);
        std::cout << "Stop: " << runtime->stop_reason() << '\n'
                  << "Guest PC: " << psprecomp::hex32(cpu.pc) << '\n';
        // An unimplemented service is the expected diagnostic result, not a
        // successful game boot. Preserve that distinction in the exit status.
        return 3;
    } catch (const std::exception &error) {
        std::cerr << "PES bootstrap error: " << error.what() << '\n';
        return 1;
    }
}
