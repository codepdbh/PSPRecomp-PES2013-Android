#include "psp_utility_modules.hpp"
#include "psprecomp/runtime.hpp"
#include "psprecomp/common.hpp"
#include <memory>
#include <set>

namespace vcs {
void install_utility_av_modules(psprecomp::Runtime &runtime) {
    // These libraries are supplied by the shared HLE host, not loaded as PRX.
    // Loading a library does not imply every one of its imports is implemented.
    auto loaded = std::make_shared<std::set<std::uint32_t>>();
    // MP3 resource lifetime is independent of utility-module lifetime. Handles
    // and decoding remain explicit missing imports until their HLE is installed.
    auto mp3_resources = std::make_shared<bool>(false);
    runtime.nids().add("sceMp3", 0x35750070u, "sceMp3InitResource");
    runtime.nids().add("sceMp3", 0x3C2FA058u, "sceMp3TermResource");
    runtime.register_hle("sceMp3", 0x35750070u, [mp3_resources](auto &, auto &ctx) {
        *mp3_resources = true; ctx.set_gpr(2, 0u);
    });
    runtime.register_hle("sceMp3", 0x3C2FA058u, [mp3_resources](auto &, auto &ctx) {
        *mp3_resources = false; ctx.set_gpr(2, 0u);
    });
    const auto operation = [loaded](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, bool unload) {
        const auto id = ctx.gpr[4];
        if (id != 0x200u && (id < 0x300u || id > 0x304u)) {
            rt.stop("Utility module not implemented by PES host: " + psprecomp::hex32(id)); return;
        }
        if (unload) {
            ctx.set_gpr(2, loaded->erase(id) ? 0u : 0x80111103u); return;
        }
        if (loaded->contains(id)) { ctx.set_gpr(2, 0x80111102u); return; }
        if ((id == 0x302u || id == 0x303u) && !loaded->contains(0x300u)) {
            ctx.set_gpr(2, 0x8002013Cu); return;
        }
        loaded->insert(id); ctx.set_gpr(2, 0u);
    };
    runtime.nids().add("sceUtility", 0x2A2B3DE0u, "sceUtilityLoadModule");
    runtime.nids().add("sceUtility", 0xE49BFE92u, "sceUtilityUnloadModule");
    runtime.register_hle("sceUtility", 0x2A2B3DE0u, [operation](auto &rt, auto &ctx) { operation(rt, ctx, false); });
    runtime.register_hle("sceUtility", 0xE49BFE92u, [operation](auto &rt, auto &ctx) { operation(rt, ctx, true); });
}
}
