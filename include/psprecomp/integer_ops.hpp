#pragma once
#include "psprecomp/allegrex_context.hpp"
namespace psprecomp {
// Allegrex HI:LO accumulation wraps modulo 2^64, including signed products.
PSPRECOMP_CONTEXT_FORCEINLINE void accumulate_product(AllegrexContext &ctx,
        std::uint32_t lhs, std::uint32_t rhs, bool unsigned_product, bool subtract) {
    const std::uint64_t product = unsigned_product
        ? static_cast<std::uint64_t>(lhs) * rhs
        : static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(lhs)) *
                                     static_cast<std::int64_t>(static_cast<std::int32_t>(rhs)));
    const std::uint64_t accumulator = (static_cast<std::uint64_t>(ctx.hi) << 32u) | ctx.lo;
    const std::uint64_t result = subtract ? accumulator - product : accumulator + product;
    ctx.lo = static_cast<std::uint32_t>(result);
    ctx.hi = static_cast<std::uint32_t>(result >> 32u);
}
}
