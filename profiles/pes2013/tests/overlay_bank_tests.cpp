#include "overlay_bank.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
constexpr std::uint32_t base = 0x08E83000u;
constexpr std::array<std::uint8_t, 8> text_a{1,2,3,4,5,6,7,8}, text_b{8,7,6,5,4,3,2,1};
std::uint64_t hash(const auto &bytes) {
    std::uint64_t value = 14695981039346656037ull;
    for (auto byte : bytes) { value ^= byte; value *= 1099511628211ull; }
    return value;
}
void register_a(psprecomp::Runtime &rt) {
    rt.register_function(base + 64u, +[](psprecomp::Runtime &runtime, psprecomp::AllegrexContext &) {
        runtime.stop("module A executed");
    }, "overlay_a");
}
void register_b(psprecomp::Runtime &rt) {
    rt.register_function(base + 68u, +[](psprecomp::Runtime &runtime, psprecomp::AllegrexContext &) {
        runtime.stop("module B executed");
    }, "overlay_b");
}
const auto modules = [] {
    std::array<pes2013::OverlayModule, 2> result{};
    result[0] = {"a.ovl", base, 8u, 72u, {}, hash(text_a), &register_a};
    result[1] = {"b.ovl", base, 8u, 72u, {}, hash(text_b), &register_b};
    for (auto &module : result) {
        module.header[0] = 'M'; module.header[1] = 'W'; module.header[2] = 'o'; module.header[3] = '3';
    }
    result[0].header[32] = 'a'; result[1].header[32] = 'b';
    return result;
}();
void write(psprecomp::Runtime &rt, unsigned index, const auto &text) {
    for (unsigned i = 0; i < 64; ++i) rt.memory().store8(base + i, modules[index].header[i]);
    for (unsigned i = 0; i < text.size(); ++i) rt.memory().store8(base + 64 + i, text[i]);
}
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
}
namespace pes2013 {
std::span<const OverlayModule> generated_overlay_modules() { return modules; }
}
int main() {
    try {
        psprecomp::Runtime rt;
        pes2013::install_overlay_bank(rt);
        write(rt, 0, text_a);
        rt.run(base + 64u, 3u);
        require(rt.stop_reason() == "module A executed", "First overlay did not activate");
        write(rt, 1, text_b);
        rt.run(base + 68u, 3u);
        require(rt.stop_reason() == "module B executed", "Replacement overlay did not activate");
        require(!rt.has_function(base + 64u), "Replaced overlay left stale code registrations");
        rt.memory().store8(base, 0u);
        rt.run(base + 68u, 3u);
        require(rt.stop_reason().find("overwritten") != std::string::npos,
                "Unidentified replacement executed old native code");
        psprecomp::Runtime corrupt;
        pes2013::install_overlay_bank(corrupt);
        write(corrupt, 0, text_b);
        corrupt.run(base + 64u, 3u);
        require(corrupt.stop_reason().find("differs") != std::string::npos && !corrupt.has_function(base + 64u),
                "Mismatched text was allowed to activate");
        std::cout << "Overlay identity, replacement and stale-code rejection tests passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
