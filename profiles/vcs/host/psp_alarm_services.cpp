#include "psp_alarm_services.hpp"
#include "psprecomp/runtime.hpp"
#include <algorithm>
#include <limits>
namespace vcs {
std::optional<AlarmInvocation> AlarmServices::take_due() {
    auto selected = records.end();
    const auto time = now();
    for (auto it = records.begin(); it != records.end(); ++it)
        if (!it->second.running && it->second.deadline <= time &&
            (selected == records.end() || it->second.deadline < selected->second.deadline)) selected = it;
    if (selected == records.end()) return {};
    selected->second.running = true;
    return AlarmInvocation{selected->first, selected->second.function, selected->second.argument};
}
void AlarmServices::complete(std::int32_t uid, std::int32_t delay) {
    const auto found = records.find(uid);
    if (found == records.end()) return; // Handler may cancel itself.
    if (delay <= 0) { records.erase(found); return; }
    found->second.deadline = now() + static_cast<std::uint32_t>(delay);
    found->second.running = false;
}
std::shared_ptr<AlarmServices> install_alarm_services(psprecomp::Runtime &rt, std::function<std::uint64_t()> now) {
    auto state = std::make_shared<AlarmServices>(); state->now = std::move(now);
    rt.nids().add("ThreadManForUser", 0x6652B8CAu, "sceKernelSetAlarm");
    rt.nids().add("ThreadManForUser", 0x7E65B999u, "sceKernelCancelAlarm");
    rt.register_hle("ThreadManForUser", 0x6652B8CAu, [state](auto &runtime, auto &ctx) {
        if ((ctx.gpr[5] & 3u) || !runtime.memory().raw_pointer(ctx.gpr[5], 4u)) {
            ctx.set_gpr(2, 0x800200D3u); return;
        }
        const auto uid = state->next_uid++;
        state->records.emplace(uid, AlarmServices::Record{state->now() + ctx.gpr[4], ctx.gpr[5], ctx.gpr[6]});
        ctx.set_gpr(2, uid);
    });
    rt.register_hle("ThreadManForUser", 0x7E65B999u, [state](auto &, auto &ctx) {
        ctx.set_gpr(2, state->records.erase(static_cast<std::int32_t>(ctx.gpr[4])) ? 0u : 0x8002019Fu);
    });
    return state;
}
}
