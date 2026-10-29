#pragma once
#include "psprecomp/runtime.hpp"
#include <array>
#include <span>
namespace pes2013 {
struct OverlayModule {
    const char *name;
    std::uint32_t base, text_size, memory_size;
    std::array<std::uint8_t, 64> header;
    std::uint64_t text_hash;
    void (*register_code)(psprecomp::Runtime &);
};
std::span<const OverlayModule> generated_overlay_modules();
void install_overlay_bank(psprecomp::Runtime &runtime);
}
