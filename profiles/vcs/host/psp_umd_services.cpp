#include "psp_umd_services.hpp"
#include "psprecomp/runtime.hpp"
#include <cstring>
#include <memory>
#include <utility>

namespace vcs {
namespace {
constexpr std::uint32_t invalid_argument = 0x80010016u;
struct Drive {
    bool present{};
    bool active{true};
    std::int32_t callback{};
    UmdCallbackHooks hooks;
    std::uint32_t status() const { return present ? (active ? 0x32u : 0x12u) : 0x01u; }
    void notify() const {
        if (callback != 0 && hooks.notify && hooks.exists && hooks.exists(callback))
            hooks.notify(callback, status());
    }
};
bool valid_name(psprecomp::Runtime &rt, std::uint32_t address) {
    const auto *name = rt.memory().raw_pointer(address, 7u);
    return name != nullptr && std::memcmp(name, "disc0:", 7u) == 0;
}
}

void install_umd_services(psprecomp::Runtime &runtime, bool medium_present, UmdCallbackHooks callbacks) {
    auto drive = std::make_shared<Drive>();
    drive->present = medium_present;
    drive->hooks = std::move(callbacks);
    const auto bind = [&](std::uint32_t nid, const char *name, auto handler) {
        runtime.nids().add("sceUmdUser", nid, name);
        runtime.register_hle("sceUmdUser", nid, std::move(handler));
    };
    bind(0x46EBB729u, "sceUmdCheckMedium", [drive](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
        ctx.set_gpr(2u, drive->present ? 1u : 0u);
    });
    bind(0x6B4A146Cu, "sceUmdGetDriveStat", [drive](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
        ctx.set_gpr(2u, drive->status());
    });
    bind(0xAEE7404Du, "sceUmdRegisterUMDCallBack", [drive](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
        const auto uid = static_cast<std::int32_t>(ctx.gpr[4u]);
        if (uid <= 0 || !drive->hooks.exists || !drive->hooks.exists(uid)) {
            ctx.set_gpr(2u, invalid_argument);
            return;
        }
        drive->callback = uid;
        ctx.set_gpr(2u, 0u);
    });
    bind(0xBD2BDE07u, "sceUmdUnRegisterUMDCallBack", [drive](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
        if (drive->callback == 0 || ctx.gpr[4u] != static_cast<std::uint32_t>(drive->callback)) {
            ctx.set_gpr(2u, invalid_argument);
            return;
        }
        drive->callback = 0;
        ctx.set_gpr(2u, 0u);
    });
    bind(0xC6183D47u, "sceUmdActivate", [drive](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        if ((ctx.gpr[4u] != 1u && ctx.gpr[4u] != 2u) || !valid_name(rt, ctx.gpr[5u])) {
            ctx.set_gpr(2u, invalid_argument);
            return;
        }
        drive->active = true;
        drive->notify();
        ctx.set_gpr(2u, 0u);
    });
    bind(0xE83742BAu, "sceUmdDeactivate", [drive](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        if ((ctx.gpr[4u] != 1u && ctx.gpr[4u] != 2u) || !valid_name(rt, ctx.gpr[5u])) {
            ctx.set_gpr(2u, invalid_argument);
            return;
        }
        drive->active = false;
        drive->notify();
        ctx.set_gpr(2u, 0u);
    });
    const auto wait = [drive](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, bool with_callbacks) {
        const auto requested = ctx.gpr[4u];
        // CHANGED alone is not a valid wait condition; state matching is OR.
        if ((requested & 0x3Bu) == 0u) {
            ctx.set_gpr(2u, invalid_argument);
            return;
        }
        if ((requested & drive->status()) == 0u) {
            rt.stop("UMD drive-state wait needs asynchronous media events (fixed-disc host)");
            return;
        }
        ctx.set_gpr(2u, 0u);
        if (with_callbacks && drive->hooks.checkpoint) drive->hooks.checkpoint(ctx, 0u);
    };
    bind(0x8EF08FCEu, "sceUmdWaitDriveStat", [wait](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        wait(rt, ctx, false);
    });
    bind(0x56202973u, "sceUmdWaitDriveStatWithTimer", [wait](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        wait(rt, ctx, false);
    });
    bind(0x4A9E5E29u, "sceUmdWaitDriveStatCB", [wait](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        wait(rt, ctx, true);
    });
    bind(0x6AF9B50Au, "sceUmdCancelWaitDriveStat", [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
        // The fixed-disc model has no outstanding asynchronous drive waits.
        ctx.set_gpr(2u, 0u);
    });
    bind(0x20628E6Fu, "sceUmdGetErrorStat", [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
        ctx.set_gpr(2u, 0u);
    });
}
}
