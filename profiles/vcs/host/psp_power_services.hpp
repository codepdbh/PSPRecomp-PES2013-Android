#pragma once
namespace psprecomp { class Runtime; }
namespace vcs {
// Models the PSP clock configuration requested by the guest. This does not
// change the physical CPU clocks of the phone or promise cycle-accurate timing.
void install_power_clock_services(psprecomp::Runtime &runtime);
}
