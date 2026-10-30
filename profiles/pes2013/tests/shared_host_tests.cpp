#include "vcs_profile.hpp"
#include "psprecomp/runtime.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

void android_game_runtime_set_status(const char *) {}

int main() {
    psprecomp::Runtime runtime;
    vcs::install_profile(runtime, 0x08E8AC00u);
    constexpr std::uint32_t buffer = 0x08818000u, output = 0x08819000u;
    std::array<std::uint8_t, 256> data{};
    auto put16 = [&](unsigned i, std::uint16_t n) { data[i] = n & 255u; data[i + 1] = n >> 8u; };
    auto put32 = [&](unsigned i, std::uint32_t n) { for (unsigned j = 0; j < 4; ++j) data[i + j] = (n >> (j * 8u)) & 255u; };
    std::memcpy(data.data(), "RIFF", 4); put32(4, 0x1000u - 8u);
    std::memcpy(data.data() + 8, "WAVEfmt ", 8); put32(16, 0x34u);
    put16(20, 0xFFFEu); put16(22, 2); put32(24, 44100); put32(28, 12058);
    put16(32, 560); put16(34, 0); std::memcpy(data.data() + 72, "fact", 4);
    put32(76, 8); put32(80, 4096); put32(84, 0x800); std::memcpy(data.data() + 88, "data", 4);
    put32(92, 0x1000u - 96u); runtime.memory().copy_in(buffer, data);

    psprecomp::AllegrexContext cpu{};
    cpu.set_gpr(4, buffer); cpu.set_gpr(5, data.size()); cpu.set_gpr(6, 0);
    runtime.invoke_import("sceAtrac3plus", 0x7A20E7AFu, cpu);
    if (runtime.stopped() || cpu.gpr[2] != 0u) { std::cerr << "Whole-buffer ATRAC initialization failed\n"; return 1; }
    cpu = {}; cpu.set_gpr(4, 0); cpu.set_gpr(5, output);
    runtime.invoke_import("sceAtrac3plus", 0xA554A158u, cpu);
    if (cpu.gpr[2] != 0u || runtime.memory().load32(output) != 96u) { std::cerr << "ATRAC metadata is wrong\n"; return 1; }
    cpu = {}; cpu.set_gpr(4, 0);
    runtime.invoke_import("sceAtrac3plus", 0x61EB33F5u, cpu);
    if (cpu.gpr[2] != 0u) { std::cerr << "ATRAC release failed\n"; return 1; }
    cpu = {}; cpu.set_gpr(4, buffer); cpu.set_gpr(5, 4);
    runtime.invoke_import("sceAtrac3plus", 0x7A20E7AFu, cpu);
    if (cpu.gpr[2] != 0x80630006u) { std::cerr << "Truncated ATRAC header was accepted\n"; return 1; }
    std::cout << "Whole-buffer ATRAC initialization and errors passed\n";
}
