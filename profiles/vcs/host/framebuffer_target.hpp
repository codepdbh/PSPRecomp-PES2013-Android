#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace vcs {
// Choose the most frequently used normalized framebuffer. Ties go to the
// target that first appeared, and an empty list preserves the caller's default.
template<class Range, class Address>
std::uint32_t most_used_framebuffer(const Range &items, std::uint32_t fallback, Address address_of) {
    std::vector<std::pair<std::uint32_t, std::size_t>> counts;
    for (const auto &item : items) {
        const std::uint32_t address = address_of(item) & 0x001FFFF0u;
        const auto found = std::find_if(counts.begin(), counts.end(),
            [address](const auto &entry) { return entry.first == address; });
        if (found == counts.end()) counts.emplace_back(address, 1u);
        else ++found->second;
    }
    std::size_t best_count = 0;
    for (const auto &[address, count] : counts)
        if (count > best_count) { fallback = address; best_count = count; }
    return fallback;
}
}
