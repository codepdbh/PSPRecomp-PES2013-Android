#pragma once
#include <cstdint>
#include <functional>

namespace psprecomp { class Runtime; struct AllegrexContext; }
namespace vcs {
struct UmdCallbackHooks {
    std::function<bool(std::int32_t)> exists;
    std::function<void(std::int32_t, std::uint32_t)> notify;
    std::function<void(psprecomp::AllegrexContext &, std::uint32_t)> checkpoint;
};
// Fixed, locally mounted media model. Unsatisfied waits stop diagnostically;
// disc swapping and asynchronously pending drive-state waits are not implemented.
void install_umd_services(psprecomp::Runtime &runtime, bool medium_present,
                          UmdCallbackHooks callbacks = {});
}
