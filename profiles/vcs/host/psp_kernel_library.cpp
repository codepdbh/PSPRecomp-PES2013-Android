#include "psp_kernel_library.hpp"
#include "psprecomp/runtime.hpp"

#include <atomic>
#include <cstring>
#include <vector>

namespace vcs {
namespace {
bool interrupts_enabled = true;
}

bool kernel_cpu_interrupts_enabled() noexcept { return interrupts_enabled; }

void install_kernel_library(psprecomp::Runtime &runtime) {
    interrupts_enabled = true;
    runtime.register_hle("Kernel_Library", 0x092968F4u,
        [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
            ctx.set_gpr(2u, interrupts_enabled ? 1u : 0u);
            interrupts_enabled = false;
        });
    const auto resume = [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
        interrupts_enabled = ctx.gpr[4u] != 0u;
    };
    runtime.register_hle("Kernel_Library", 0x5F10D406u, resume);
    runtime.register_hle("Kernel_Library", 0x3B84732Du,
        [resume](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
            std::atomic_thread_fence(std::memory_order_seq_cst);
            resume(rt, ctx);
        });
    runtime.register_hle("Kernel_Library", 0x47A0B729u,
        [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
            ctx.set_gpr(2u, ctx.gpr[4u] == 0u ? 1u : 0u);
        });
    runtime.register_hle("Kernel_Library", 0xB55249D2u,
        [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
            ctx.set_gpr(2u, interrupts_enabled ? 1u : 0u);
        });
    runtime.register_hle("Kernel_Library", 0xA089ECA4u,
        [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
            const auto destination = ctx.gpr[4u];
            const auto size = ctx.gpr[6u];
            if (size != 0u) {
                auto *bytes = rt.memory().raw_pointer(destination, size);
                if (bytes == nullptr) {
                    rt.stop("Kernel_Library::sceKernelMemset destination outside guest memory");
                    return;
                }
                std::memset(bytes, static_cast<int>(ctx.gpr[5u] & 0xFFu), size);
            }
            ctx.set_gpr(2u, destination);
        });
    runtime.register_hle("Kernel_Library", 0x1839852Au,
        [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
            const auto destination = ctx.gpr[4u];
            const auto size = ctx.gpr[6u];
            if (size != 0u) {
                auto *target = rt.memory().raw_pointer(destination, size);
                const auto *source = rt.memory().raw_pointer(ctx.gpr[5u], size);
                if (target == nullptr || source == nullptr) {
                    rt.stop("Kernel_Library::sceKernelMemcpy range outside guest memory");
                    return;
                }
                std::memmove(target, source, size);
            }
            ctx.set_gpr(2u, destination);
        });
    runtime.register_hle("Kernel_Library", 0x293B45B8u,
        [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
            rt.invoke_import("ThreadManForUser", 0x293B45B8u, ctx);
        });
    const auto dma_copy = [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        const auto destination = ctx.gpr[4], source = ctx.gpr[5], size = ctx.gpr[6];
        if (!size) { ctx.set_gpr(2, 0x80000104u); return; }
        if (static_cast<std::uint64_t>(destination) + size >= 0x80000000ull ||
            static_cast<std::uint64_t>(source) + size >= 0x80000000ull || size >= 0x80000000u) {
            ctx.set_gpr(2, 0x80000023u); return;
        }
        if (!rt.memory().contains(destination, size) || !rt.memory().contains(source, size)) {
            ctx.set_gpr(2, 0x80000103u); return;
        }
        auto *output = rt.memory().raw_pointer(destination, size);
        const auto *input = rt.memory().raw_pointer(source, size);
        if (output && input) std::memmove(output, input, size);
        else {
            // VRAM mirrors are valid guest ranges but may wrap in host storage.
            std::vector<std::uint8_t> temporary(size);
            rt.memory().copy_out(source, temporary);
            rt.memory().copy_in(destination, temporary);
        }
        // Transfers complete synchronously; no DMA timing/busy model yet.
        ctx.set_gpr(2, 0u);
    };
    runtime.nids().add("sceDmac", 0x617F3FE6u, "sceDmacMemcpy");
    runtime.nids().add("sceDmac", 0xD97F94D8u, "sceDmacTryMemcpy");
    runtime.register_hle("sceDmac", 0x617F3FE6u, dma_copy);
    runtime.register_hle("sceDmac", 0xD97F94D8u, dma_copy);
}
}
