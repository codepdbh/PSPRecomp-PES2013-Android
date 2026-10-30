#pragma once

namespace vcs {
// Delay a single clear-only list when a frame is already on screen. A later
// drawing list replaces it; an isolated or repeated clear remains visible.
class ClearFramePolicy {
public:
    bool defer_collected(bool clear_only, bool has_previous_frame) noexcept {
        if (clear_only && !pending_ && has_previous_frame) {
            pending_ = true;
            return true;
        }
        pending_ = false;
        return false;
    }
    bool release_without_submission(bool submitted) noexcept {
        if (!pending_ || submitted) return false;
        pending_ = false;
        return true;
    }
private:
    bool pending_{};
};
}
