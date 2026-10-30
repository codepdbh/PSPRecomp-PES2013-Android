#include "framebuffer_target.hpp"
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

int main() {
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
