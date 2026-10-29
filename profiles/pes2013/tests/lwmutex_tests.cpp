#include "psp_lwmutex_services.hpp"
#include "psprecomp/runtime.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
}
int main() {
    try {
        psprecomp::Runtime rt;
        std::int32_t current = 1;
        unsigned blocked = 0;
        std::vector<std::pair<std::int32_t, psprecomp::AllegrexContext>> ready;
        std::uint32_t timeout_seen{};
        auto remove_waiter = vcs::install_lwmutex_services(rt, {
            [&] { return current; },
            [](std::int32_t uid) { return uid == 3 ? 10u : 30u; },
            [&](auto &, auto &, const auto &, std::uint32_t timeout) { ++blocked; timeout_seen = timeout; },
            [&](std::int32_t uid, const auto &ctx) { ready.emplace_back(uid, ctx); }
        });
        constexpr std::uint32_t area = 0x08810000u;
        rt.memory().store8(area + 64, 'm'); rt.memory().store8(area + 65, 0);
        auto &ctx = rt.cpu();
        auto call = [&](const char *lib, std::uint32_t nid, std::uint32_t count = 1u) {
            ctx.set_gpr(4, area); ctx.set_gpr(5, count); ctx.set_gpr(6, 0u); ctx.set_gpr(31, 0x08804000u);
            rt.invoke_import(lib, nid, ctx); require(!rt.stopped(), "Unexpected diagnostic stop");
            return ctx.gpr[2];
        };
        auto create = [&](std::uint32_t attr, std::uint32_t count) {
            ctx.set_gpr(4, area); ctx.set_gpr(5, area + 64); ctx.set_gpr(6, attr); ctx.set_gpr(7, count);
            rt.invoke_import("ThreadManForUser", 0x19CFF145u, ctx); return ctx.gpr[2];
        };
        require(create(0u, 2u) == 0x800201BDu, "Nonrecursive mutex accepted multiple initial locks");
        require(create(0x300u, 0u) == 0u, "Create failed");
        require(rt.memory().load32(area) == 0u && rt.memory().load32(area + 4) == 0u, "New mutex has an owner");
        require(call("Kernel_Library", 0xBEA46419u, 2u) == 0u, "Recursive lock failed");
        require(call("Kernel_Library", 0xBEA46419u) == 0u && rt.memory().load32(area) == 3u, "Recursive count lost");
        current = 2;
        require(call("Kernel_Library", 0x15B6446Bu) == 0x800201CCu, "Nonowner unlocked mutex");
        require(call("Kernel_Library", 0x37431849u) == 0x800201CBu, "Busy trylock reported success");
        require(call("Kernel_Library", 0xDC692EE3u) == 0x800201C4u, "Legacy trylock returned wrong error");
        constexpr std::uint32_t timeout = area + 128u;
        rt.memory().store32(timeout, 100u);
        ctx.set_gpr(4u, area); ctx.set_gpr(5u, 1u); ctx.set_gpr(6u, timeout);
        rt.invoke_import("Kernel_Library", 0xBEA46419u, ctx);
        require(timeout_seen == timeout && rt.memory().load32(area + 12u) == 1u,
                "Timed contention did not pass its timeout to the scheduler");
        remove_waiter(current);
        require(rt.memory().load32(area + 12u) == 0u && ready.empty(),
                "Expired waiter stayed queued or incorrectly acquired mutex");
        call("Kernel_Library", 0xBEA46419u);
        current = 3; call("Kernel_Library", 0xBEA46419u);
        require(blocked == 3 && rt.memory().load32(area + 12) == 2u, "Contended threads were not queued");
        current = 1;
        require(call("Kernel_Library", 0x15B6446Bu, 4u) == 0x800201CEu, "Unlock underflow accepted");
        require(call("Kernel_Library", 0x15B6446Bu, 3u) == 0u, "Owner unlock failed");
        require(ready.size() == 1u && ready[0].first == 3 && ready[0].second.pc == 0x08804000u,
                "Priority waiter did not receive saved return context");
        require(rt.memory().load32(area) == 1u && rt.memory().load32(area + 4) == 3u,
                "Ownership was not transferred before waking waiter");
        call("ThreadManForUser", 0x60107536u);
        require(ready.size() == 2u && ready[1].first == 2 && ready[1].second.gpr[2] == 0x800201A7u,
                "Delete did not release blocked thread with WAIT_DELETE");
        require(call("Kernel_Library", 0x37431849u) == 0x800201CAu, "Deleted mutex remained usable");
        std::cout << "LwMutex ownership, recursion, contention, priority and deletion tests passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
