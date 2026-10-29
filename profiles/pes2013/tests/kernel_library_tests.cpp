#include "psp_kernel_library.hpp"
#include "psp_power_services.hpp"
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
