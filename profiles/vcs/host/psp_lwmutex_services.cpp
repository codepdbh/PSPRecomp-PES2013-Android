#include "psp_lwmutex_services.hpp"
#include "psprecomp/runtime.hpp"
#include <algorithm>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

namespace vcs {
namespace {
constexpr std::uint32_t bad_address = 0x800200D3u, bad_count = 0x800201BDu;
constexpr std::uint32_t missing = 0x800201CAu, busy = 0x800201CBu;
struct Waiter { std::int32_t thread; std::uint32_t count; psprecomp::AllegrexContext resume; };
struct Mutex { std::uint32_t uid; std::vector<Waiter> waiters; };
struct State {
    LwMutexHooks hooks;
    std::uint32_t next_uid{0x700u};
    std::unordered_map<std::uint32_t, Mutex> mutexes;
    std::int32_t current() const { return hooks.current_thread ? hooks.current_thread() : 1; }
};
}
void install_lwmutex_services(psprecomp::Runtime &runtime, LwMutexHooks hooks) {
    auto state = std::make_shared<State>(); state->hooks = std::move(hooks);
    const auto bind = [&](const char *library, std::uint32_t nid, const char *name, auto handler) {
        runtime.nids().add(library, nid, name); runtime.register_hle(library, nid, std::move(handler));
    };
    bind("ThreadManForUser", 0x94AA61EEu, "sceKernelGetThreadCurrentPriority", [state](auto &, auto &ctx) {
        if (!state->hooks.priority) { ctx.set_gpr(2, 0x80020198u); return; }
        ctx.set_gpr(2, state->hooks.priority(state->current()));
    });
    bind("ThreadManForUser", 0x19CFF145u, "sceKernelCreateLwMutex",
        [state](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
            const auto area = ctx.gpr[4], attr = ctx.gpr[6], count = ctx.gpr[7];
            if ((area & 3u) || !rt.memory().raw_pointer(area, 32u) || !rt.memory().raw_pointer(ctx.gpr[5], 1u)) {
                ctx.set_gpr(2, bad_address); return;
            }
            if (attr >= 0x400u) { ctx.set_gpr(2, 0x80020191u); return; }
            if (count > 0x7FFFFFFFu || (!(attr & 0x200u) && count > 1u)) {
                ctx.set_gpr(2, bad_count); return;
            }
            const auto uid = state->next_uid++;
            state->mutexes.insert_or_assign(area, Mutex{uid, {}});
            for (unsigned i = 0; i < 32; i += 4) rt.memory().store32(area + i, 0u);
            rt.memory().store32(area, count);
            rt.memory().store32(area + 4, count ? state->current() : 0u);
            rt.memory().store32(area + 8, attr);
            rt.memory().store32(area + 16, uid);
            ctx.set_gpr(2, 0u);
        });
    const auto lookup = [state](psprecomp::Runtime &rt, std::uint32_t area) -> Mutex * {
        if ((area & 3u) || !rt.memory().raw_pointer(area, 32u)) return nullptr;
        const auto it = state->mutexes.find(area);
        if (it == state->mutexes.end() || it->second.uid != rt.memory().load32(area + 16)) return nullptr;
        return &it->second;
    };
    const auto lock = [state, lookup](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx,
                                     bool attempt, bool legacy) {
        const auto area = ctx.gpr[4], count = ctx.gpr[5];
        auto *mutex = lookup(rt, area);
        if (!mutex) { ctx.set_gpr(2, missing); return; }
        const auto attr = rt.memory().load32(area + 8), level = rt.memory().load32(area);
        const auto owner = static_cast<std::int32_t>(rt.memory().load32(area + 4));
        std::uint32_t error = 0u;
        if (!count || count > 0x7FFFFFFFu || (!(attr & 0x200u) && count > 1u)) error = bad_count;
        else if (count > 0x7FFFFFFFu - level) error = 0x800201CDu;
        else if (level && owner == state->current() && !(attr & 0x200u)) error = 0x800201CFu;
        if (error) { ctx.set_gpr(2, attempt && legacy ? 0x800201C4u : error); return; }
        if (!level || owner == state->current()) {
            rt.memory().store32(area, level + count);
            rt.memory().store32(area + 4, state->current());
            ctx.set_gpr(2, 0u); return;
        }
        if (attempt) { ctx.set_gpr(2, legacy ? 0x800201C4u : busy); return; }
        if (ctx.gpr[6]) {
            if (!rt.memory().raw_pointer(ctx.gpr[6], 4u)) { ctx.set_gpr(2, bad_address); return; }
            if (rt.memory().load32(ctx.gpr[6]) == 0u) { ctx.set_gpr(2, 0x800201A8u); return; }
            rt.stop("Timed contended LwMutex wait needs scheduler integration"); return;
        }
        if (!state->hooks.block || !state->hooks.ready) { rt.stop("Contended LwMutex has no scheduler"); return; }
        auto resume = ctx; resume.pc = ctx.gpr[31]; resume.set_gpr(2, 0u);
        mutex->waiters.push_back({state->current(), count, resume});
        rt.memory().store32(area + 12, static_cast<std::uint32_t>(mutex->waiters.size()));
        state->hooks.block(rt, ctx, resume);
    };
    bind("Kernel_Library", 0xBEA46419u, "sceKernelLockLwMutex", [lock](auto &rt, auto &ctx) { lock(rt, ctx, false, false); });
    bind("Kernel_Library", 0x1FC64E09u, "sceKernelLockLwMutexCB", [lock](auto &rt, auto &ctx) { lock(rt, ctx, false, false); });
    bind("Kernel_Library", 0x37431849u, "sceKernelTryLockLwMutex_600", [lock](auto &rt, auto &ctx) { lock(rt, ctx, true, false); });
    bind("Kernel_Library", 0xDC692EE3u, "sceKernelTryLockLwMutex", [lock](auto &rt, auto &ctx) { lock(rt, ctx, true, true); });
    bind("Kernel_Library", 0x15B6446Bu, "sceKernelUnlockLwMutex", [state, lookup](auto &rt, auto &ctx) {
        const auto area = ctx.gpr[4], count = ctx.gpr[5]; auto *mutex = lookup(rt, area);
        if (!mutex) { ctx.set_gpr(2, missing); return; }
        const auto level = rt.memory().load32(area), attr = rt.memory().load32(area + 8);
        if (!count || count > 0x7FFFFFFFu || (!(attr & 0x200u) && count > 1u)) { ctx.set_gpr(2, bad_count); return; }
        if (!level || rt.memory().load32(area + 4) != static_cast<std::uint32_t>(state->current())) { ctx.set_gpr(2, 0x800201CCu); return; }
        if (count > level) { ctx.set_gpr(2, 0x800201CEu); return; }
        rt.memory().store32(area, level - count);
        if (level == count) {
            rt.memory().store32(area + 4, 0u);
            if (!mutex->waiters.empty()) {
                auto next = mutex->waiters.begin();
                if ((attr & 0x100u) && state->hooks.priority)
                    next = std::min_element(next, mutex->waiters.end(), [&](const auto &a, const auto &b) {
                        return state->hooks.priority(a.thread) < state->hooks.priority(b.thread);
                    });
                const auto waiter = *next; mutex->waiters.erase(next);
                rt.memory().store32(area, waiter.count); rt.memory().store32(area + 4, waiter.thread);
                state->hooks.ready(waiter.thread, waiter.resume);
            }
        }
        rt.memory().store32(area + 12, static_cast<std::uint32_t>(mutex->waiters.size()));
        ctx.set_gpr(2, 0u);
    });
    bind("ThreadManForUser", 0x60107536u, "sceKernelDeleteLwMutex", [state, lookup](auto &rt, auto &ctx) {
        const auto area = ctx.gpr[4]; auto *mutex = lookup(rt, area);
        if (!mutex) { ctx.set_gpr(2, missing); return; }
        for (auto &waiter : mutex->waiters) {
            waiter.resume.set_gpr(2, 0x800201A7u);
            state->hooks.ready(waiter.thread, waiter.resume);
        }
        state->mutexes.erase(area);
        rt.memory().store32(area, 0u); rt.memory().store32(area + 4, 0xFFFFFFFFu);
        rt.memory().store32(area + 12, 0u); rt.memory().store32(area + 16, 0xFFFFFFFFu);
        ctx.set_gpr(2, 0u);
    });
}
}
