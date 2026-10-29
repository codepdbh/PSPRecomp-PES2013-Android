#pragma once
namespace psprecomp { class Runtime; }
namespace vcs {
// Guest system-overlay settings, separate from host Android preferences.
void install_impose_language_services(psprecomp::Runtime &runtime);
}
