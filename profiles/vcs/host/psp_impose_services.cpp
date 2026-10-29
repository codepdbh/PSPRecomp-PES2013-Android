#include "psp_impose_services.hpp"
#include "psprecomp/runtime.hpp"
#include <memory>

namespace vcs {
namespace {
struct LanguageMode {
    std::uint32_t language{1u}; // English; matches the host's system parameters.
    std::uint32_t confirm_button{1u}; // Cross.
};
}
void install_impose_language_services(psprecomp::Runtime &runtime) {
    auto mode = std::make_shared<LanguageMode>();
    runtime.nids().add("sceImpose", 0x36AA6E91u, "sceImposeSetLanguageMode");
    runtime.nids().add("sceImpose", 0x24FD7BCFu, "sceImposeGetLanguageMode");
    runtime.register_hle("sceImpose", 0x36AA6E91u,
        [mode](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
            mode->language = ctx.gpr[4u];
            mode->confirm_button = ctx.gpr[5u];
            ctx.set_gpr(2u, 0u);
        });
    runtime.register_hle("sceImpose", 0x24FD7BCFu,
        [mode](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
            // Stop on malformed output buffers rather than corrupting guest
            // memory. This diagnostic guard is not a firmware error code.
            if (!rt.memory().raw_pointer(ctx.gpr[4u], 4u) ||
                !rt.memory().raw_pointer(ctx.gpr[5u], 4u)) {
                rt.stop("sceImposeGetLanguageMode output buffer is outside guest memory");
                return;
            }
            rt.memory().store32(ctx.gpr[4u], mode->language);
            rt.memory().store32(ctx.gpr[5u], mode->confirm_button);
            ctx.set_gpr(2u, 0u);
        });
}
}
