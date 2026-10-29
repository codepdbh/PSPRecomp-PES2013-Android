#include "psp_kernel_library.hpp"
#include "psprecomp/runtime.hpp"

#include <atomic>
#include <cstring>

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
}
}
