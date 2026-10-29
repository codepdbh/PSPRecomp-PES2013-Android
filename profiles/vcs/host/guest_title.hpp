#pragma once

// Which game the recompiled corpus is. Liberty City Stories runs on the VCS
// host code (same engine, same PSP services), so the host is shared and only
// the patches keyed to VCS executable addresses are switched off for LCS:
// installed there, they would overwrite unrelated LCS code.
//
// Set by CMake (PSPRECOMP_GUEST_TITLE=lcs defines PSPRECOMP_TITLE_LCS).
namespace vcs {
#if defined(PSPRECOMP_TITLE_LCS)
inline constexpr bool kTitleLcs = true;
#else
inline constexpr bool kTitleLcs = false;
#endif
#if defined(PSPRECOMP_TITLE_PES2013)
inline constexpr bool kTitlePes2013 = true;
#else
inline constexpr bool kTitlePes2013 = false;
#endif
inline constexpr bool kTitleVcs = !kTitleLcs && !kTitlePes2013;
} // namespace vcs
