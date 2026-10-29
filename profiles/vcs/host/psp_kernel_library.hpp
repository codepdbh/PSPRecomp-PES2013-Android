#pragma once

namespace psprecomp { class Runtime; }

namespace vcs {
// PSP CPU interrupt state is global to the single guest CPU, not a nesting
// counter. Suspend returns the old state, which the caller must later restore.
void install_kernel_library(psprecomp::Runtime &runtime);
bool kernel_cpu_interrupts_enabled() noexcept;
}
