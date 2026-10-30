#include "framebuffer_target.hpp"
#include "clear_frame_policy.hpp"
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

int main() {
    vcs::ClearFramePolicy policy;
    if (policy.defer_collected(true, false) ||
        !policy.defer_collected(true, true) || policy.release_without_submission(true) ||
        policy.defer_collected(false, true) || policy.release_without_submission(false)) {
        std::cerr << "Clear followed by drawing did not preserve the drawing\n";
        return 1;
    }
    if (!policy.defer_collected(true, true) || !policy.release_without_submission(false) ||
        policy.release_without_submission(false)) {
        std::cerr << "An isolated clear was not presented\n";
        return 1;
    }
    if (!policy.defer_collected(true, true) || policy.defer_collected(true, true) ||
        policy.release_without_submission(false)) {
        std::cerr << "Repeated clears were not presented\n";
        return 1;
    }
    if (!policy.defer_collected(true, true) || policy.release_without_submission(true) ||
        !policy.release_without_submission(false)) {
        std::cerr << "Clear was lost after a world-only submission\n";
        return 1;
    }
    std::mt19937 generator(12345);
    for (unsigned trial = 0; trial != 1000; ++trial) {
        std::vector<std::uint32_t> targets;
        for (unsigned i = 0, n = generator() % 100; i != n; ++i)
            targets.push_back((generator() % 8u) * 16u + (generator() % 16u));
        std::uint32_t expected = 0x1234u;
        std::size_t best = 0;
        for (const auto address : targets) {
            std::size_t count = 0;
            for (const auto other : targets)
                if ((other & 0x001FFFF0u) == (address & 0x001FFFF0u)) ++count;
            if (count > best) { best = count; expected = address & 0x001FFFF0u; }
        }
        if (vcs::most_used_framebuffer(targets, 0x1234u, [](auto value) { return value; }) != expected) {
            std::cerr << "Framebuffer selection changed for trial " << trial << '\n';
            return 1;
        }
    }
    return 0;
}
