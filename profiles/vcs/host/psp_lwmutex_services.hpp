#pragma once
#include "psprecomp/allegrex_context.hpp"
#include <functional>
namespace psprecomp { class Runtime; }
namespace vcs {
struct LwMutexHooks {
    std::function<std::int32_t()> current_thread;
    std::function<std::uint32_t(std::int32_t)> priority;
    std::function<void(psprecomp::Runtime &, psprecomp::AllegrexContext &,
                       const psprecomp::AllegrexContext &)> block;
    std::function<void(std::int32_t, const psprecomp::AllegrexContext &)> ready;
};
void install_lwmutex_services(psprecomp::Runtime &runtime, LwMutexHooks hooks);
}
