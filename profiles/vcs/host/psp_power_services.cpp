#include "psp_power_services.hpp"
#include "psprecomp/runtime.hpp"
#include <memory>

namespace vcs {
namespace {
struct Clocks {
    std::uint32_t pll{333u};
    std::uint32_t cpu{333u};
    std::uint32_t bus{166u};
};
}
void install_power_clock_services(psprecomp::Runtime &runtime) {
    auto clocks = std::make_shared<Clocks>();
    const auto configure = [clocks](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
        const Clocks requested{ctx.gpr[4u], ctx.gpr[5u], ctx.gpr[6u]};
        const bool valid_pll = requested.pll >= 19u && requested.pll <= 333u;
        const bool valid_cpu = requested.cpu >= 1u && requested.cpu <= requested.pll;
        const bool valid_bus = requested.bus >= 1u && requested.bus <= 166u;
        if (!valid_pll || !valid_cpu || !valid_bus) {
            ctx.set_gpr(2u, 0x800001FEu);
            return;
        }
        *clocks = requested;
        ctx.set_gpr(2u, 0u);
    };
    for (const auto nid : {0x737486F2u, 0xEBD177D6u, 0x469989ADu})
        runtime.register_hle("scePower", nid, configure);
    const auto query = [&](std::uint32_t nid, std::uint32_t Clocks::*field, bool floating) {
        runtime.register_hle("scePower", nid,
            [clocks, field, floating](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
                const auto value = (*clocks).*field;
                if (floating) ctx.fpr[0] = static_cast<float>(value);
                else ctx.set_gpr(2u, value);
            });
    };
    query(0x34F9C463u, &Clocks::pll, false);
    query(0xEA382A27u, &Clocks::pll, true);
    for (const auto nid : {0xFEE03A2Fu, 0xFDB5BFE9u}) query(nid, &Clocks::cpu, false);
    for (const auto nid : {0x478FE6F5u, 0xBD681969u}) query(nid, &Clocks::bus, false);
    query(0xB1A52C83u, &Clocks::cpu, true);
    query(0x9BADB3EBu, &Clocks::bus, true);
}
}
