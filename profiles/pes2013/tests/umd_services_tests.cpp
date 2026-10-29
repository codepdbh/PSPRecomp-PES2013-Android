#include "psp_umd_services.hpp"
#include "psprecomp/runtime.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
void call(psprecomp::Runtime &rt, std::uint32_t nid) { rt.invoke_import("sceUmdUser", nid, rt.cpu()); }
}
int main() {
    try {
        std::vector<std::uint32_t> notifications;
        unsigned checkpoints = 0u;
        psprecomp::Runtime rt;
        vcs::install_umd_services(rt, true, {
            [](std::int32_t uid) { return uid == 7; },
            [&](std::int32_t uid, std::uint32_t state) { require(uid == 7, "Wrong callback recipient"); notifications.push_back(state); },
            [&](psprecomp::AllegrexContext &ctx, std::uint32_t result) { ++checkpoints; ctx.pc = 0x08820000u; ctx.set_gpr(2u, result); }
        });
        auto &ctx = rt.cpu();
        call(rt, 0x46EBB729u); require(ctx.gpr[2u] == 1u, "Mounted media not reported");
        call(rt, 0x6B4A146Cu); require(ctx.gpr[2u] == 0x32u, "Initial drive state is not ready");
        ctx.set_gpr(4u, 99u); call(rt, 0xAEE7404Du);
        require(ctx.gpr[2u] == 0x80010016u, "Nonexistent callback was accepted");
        ctx.set_gpr(4u, 7u); call(rt, 0xAEE7404Du); require(ctx.gpr[2u] == 0u, "Valid callback rejected");
        constexpr std::uint32_t name = 0x08810000u;
        constexpr char drive[] = "disc0:";
        for (unsigned i = 0u; i < sizeof(drive); ++i) rt.memory().store8(name + i, drive[i]);
        ctx.set_gpr(4u, 1u); ctx.set_gpr(5u, 0u); call(rt, 0xC6183D47u);
        require(ctx.gpr[2u] == 0x80010016u && notifications.empty(), "Invalid mount name produced notification");
        ctx.set_gpr(5u, name); call(rt, 0xC6183D47u);
        require(ctx.gpr[2u] == 0u && notifications == std::vector<std::uint32_t>{0x32u}, "Activate did not notify ready state");
        ctx.set_gpr(4u, 0x20u); ctx.set_gpr(5u, 1000u); call(rt, 0x4A9E5E29u);
        require(!rt.stopped() && ctx.gpr[2u] == 0u && checkpoints == 1u && ctx.pc == 0x08820000u,
                "Callback-enabled ready wait did not hand off callback context");
        ctx.set_gpr(4u, 0x20u); call(rt, 0x8EF08FCEu);
        require(checkpoints == 1u, "Ordinary wait unexpectedly dispatched callbacks");
        ctx.set_gpr(4u, 4u); call(rt, 0x4A9E5E29u);
        require(ctx.gpr[2u] == 0x80010016u, "CHANGED-only wait mask was accepted");
        ctx.set_gpr(4u, 1u); ctx.set_gpr(5u, name); call(rt, 0xE83742BAu);
        call(rt, 0x6B4A146Cu); require(ctx.gpr[2u] == 0x12u, "Deactivate still reports readable media");
        require(notifications.size() == 2u && notifications.back() == 0x12u, "Deactivate did not notify state change");
        ctx.set_gpr(4u, 0x22u); call(rt, 0x8EF08FCEu);
        require(!rt.stopped() && ctx.gpr[2u] == 0u, "Wait must match any requested state bit");

        psprecomp::Runtime empty;
        vcs::install_umd_services(empty, false);
        call(empty, 0x46EBB729u); require(empty.cpu().gpr[2u] == 0u, "Empty drive reports inserted media");
        call(empty, 0x6B4A146Cu); require(empty.cpu().gpr[2u] == 1u, "Empty drive reports ready state");
        empty.cpu().set_gpr(4u, 0x20u); call(empty, 0x4A9E5E29u);
        require(empty.stopped(), "Unsatisfiable media wait must not report success");
        std::cout << "UMD media, callback and wait-mask tests passed\n";
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
