#include "psp_kernel_library.hpp"
#include "psp_power_services.hpp"
#include "psp_impose_services.hpp"
#include "psp_utility_modules.hpp"
#include "psp_alarm_services.hpp"
#include "psprecomp/runtime.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
void call(psprecomp::Runtime &rt, std::uint32_t nid) {
    rt.invoke_import("Kernel_Library", nid, rt.cpu());
    require(!rt.stopped(), "Unexpected stop in Kernel_Library service");
}
}
int main() {
    try {
        psprecomp::Runtime rt;
        vcs::install_kernel_library(rt);
        auto &ctx = rt.cpu();
        call(rt, 0x092968F4u);
        const auto outer = ctx.gpr[2];
        require(outer == 1u && !vcs::kernel_cpu_interrupts_enabled(), "First suspend must return enabled state");
        call(rt, 0x092968F4u);
        const auto inner = ctx.gpr[2];
        require(inner == 0u, "Nested suspend must return disabled state");
        ctx.set_gpr(4u, inner);
        call(rt, 0x5F10D406u);
        require(!vcs::kernel_cpu_interrupts_enabled(), "Restoring inner state must keep interrupts masked");
        ctx.set_gpr(4u, outer);
        call(rt, 0x3B84732Du);
        require(vcs::kernel_cpu_interrupts_enabled(), "Restoring outer state must reenable interrupts");

        constexpr std::uint32_t target = 0x08820000u;
        rt.memory().store8(target - 1u, 0x11u);
        rt.memory().store8(target + 8u, 0x22u);
        ctx.set_gpr(4u, target); ctx.set_gpr(5u, 0x1234u); ctx.set_gpr(6u, 8u);
        call(rt, 0xA089ECA4u);
        require(ctx.gpr[2u] == target, "Memset must return destination");
        for (unsigned n = 0u; n < 8u; ++n)
            require(rt.memory().load8(target + n) == 0x34u, "Memset must use the low fill byte");
        require(rt.memory().load8(target - 1u) == 0x11u && rt.memory().load8(target + 8u) == 0x22u,
                "Memset overwrote surrounding bytes");

        ctx.set_gpr(4u, 0x44000100u); ctx.set_gpr(5u, target); ctx.set_gpr(6u, 8u);
        call(rt, 0x1839852Au);
        require(rt.memory().load32(0x04000100u) == 0x34343434u, "Memcpy must resolve the uncached VRAM alias");
        ctx.set_gpr(4u, 0u); ctx.set_gpr(5u, 0u); ctx.set_gpr(6u, 0u);
        call(rt, 0x1839852Au);
        require(ctx.gpr[2u] == 0u, "Zero-length copy must not dereference null addresses");

        rt.register_hle("ThreadManForUser", 0x293B45B8u,
            [](psprecomp::Runtime &, psprecomp::AllegrexContext &cpu) { cpu.set_gpr(2u, 17u); });
        call(rt, 0x293B45B8u);
        require(ctx.gpr[2u] == 17u, "Kernel library thread ID must use the scheduler's service");

        vcs::install_power_clock_services(rt);
        ctx.set_gpr(4u, 333u); ctx.set_gpr(5u, 266u); ctx.set_gpr(6u, 133u);
        rt.invoke_import("scePower", 0x469989ADu, ctx);
        require(ctx.gpr[2u] == 0u, "Firmware 6.30 clock setter rejected valid clocks");
        rt.invoke_import("scePower", 0x34F9C463u, ctx);
        require(ctx.gpr[2u] == 333u, "PLL query did not preserve requested frequency");
        rt.invoke_import("scePower", 0xFEE03A2Fu, ctx);
        require(ctx.gpr[2u] == 266u, "CPU clock query did not preserve requested frequency");
        rt.invoke_import("scePower", 0x9BADB3EBu, ctx);
        require(ctx.fpr[0u] == 133.0f, "Float bus clock query returned the wrong value");
        ctx.set_gpr(4u, 222u); ctx.set_gpr(5u, 333u); ctx.set_gpr(6u, 111u);
        rt.invoke_import("scePower", 0x737486F2u, ctx);
        require(ctx.gpr[2u] == 0x800001FEu, "Clock setter accepted CPU frequency above PLL");
        rt.invoke_import("scePower", 0xFEE03A2Fu, ctx);
        require(ctx.gpr[2u] == 266u, "Invalid clock request changed the previous CPU frequency");

        std::uint64_t time_us = 100u;
        auto alarms = vcs::install_alarm_services(rt, [&] { return time_us; });
        ctx.set_gpr(4u, 50u); ctx.set_gpr(5u, 0x08804000u); ctx.set_gpr(6u, 123u);
        rt.invoke_import("ThreadManForUser", 0x6652B8CAu, ctx);
        const auto alarm_uid = static_cast<std::int32_t>(ctx.gpr[2u]);
        require(alarm_uid > 0 && !alarms->take_due(), "Alarm fired before its deadline");
        time_us = 150u;
        const auto alarm = alarms->take_due();
        require(alarm && alarm->uid == alarm_uid && alarm->argument == 123u && !alarms->take_due(),
                "Alarm arguments were lost or callback was dispatched twice");
        alarms->complete(alarm_uid, 25);
        time_us = 174u; require(!alarms->take_due(), "Repeated alarm fired early");
        time_us = 175u; require(alarms->take_due().has_value(), "Positive callback result did not rearm alarm");
        ctx.set_gpr(4u, alarm_uid);
        rt.invoke_import("ThreadManForUser", 0x7E65B999u, ctx);
        alarms->complete(alarm_uid, 10);
        time_us = 200u; require(!alarms->take_due(), "Self-cancelled alarm was resurrected");

        vcs::install_utility_av_modules(rt);
        auto module = [&](std::uint32_t id, bool unload = false) {
            ctx.set_gpr(4u, id);
            rt.invoke_import("sceUtility", unload ? 0xE49BFE92u : 0x2A2B3DE0u, ctx);
            return ctx.gpr[2u];
        };
        require(module(0x302u) == 0x8002013Cu, "ATRAC module ignored codec dependency");
        require(module(0x300u) == 0u && module(0x302u) == 0u, "Available codec modules failed to load");
        require(module(0x302u) == 0x80111102u, "Duplicate module load was accepted");
        require(module(0x302u, true) == 0u && module(0x302u, true) == 0x80111103u,
                "Module unload state was not preserved");
        vcs::install_impose_language_services(rt);
        ctx.set_gpr(4u, 3u); ctx.set_gpr(5u, 0u);
        rt.invoke_import("sceImpose", 0x36AA6E91u, ctx);
        require(ctx.gpr[2u] == 0u, "Language setter failed");
        ctx.set_gpr(4u, target); ctx.set_gpr(5u, target + 4u);
        rt.invoke_import("sceImpose", 0x24FD7BCFu, ctx);
        require(rt.memory().load32(target) == 3u && rt.memory().load32(target + 4u) == 0u,
                "Language query did not preserve guest language and confirm button");
        psprecomp::Runtime separate;
        vcs::install_impose_language_services(separate);
        separate.cpu().set_gpr(4u, target); separate.cpu().set_gpr(5u, target + 4u);
        separate.invoke_import("sceImpose", 0x24FD7BCFu, separate.cpu());
        require(separate.memory().load32(target) == 1u && separate.memory().load32(target + 4u) == 1u,
                "Language state leaked between runtimes");
        separate.memory().store32(target, 0xAABBCCDDu);
        separate.cpu().set_gpr(5u, 0x09FFFFFFu);
        separate.invoke_import("sceImpose", 0x24FD7BCFu, separate.cpu());
        require(separate.stopped() && separate.memory().load32(target) == 0xAABBCCDDu,
                "Invalid language output buffer caused a partial write");

        ctx.set_gpr(4u, 0x09FFFFFFu); ctx.set_gpr(5u, target); ctx.set_gpr(6u, 8u);
        rt.invoke_import("Kernel_Library", 0x1839852Au, ctx);
        require(rt.stopped(), "Out-of-bounds copy must stop the diagnostic host");
        std::cout << "Kernel library interrupt, memory and alias tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
