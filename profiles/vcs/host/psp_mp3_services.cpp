#include "psp_mp3_services.hpp"
#include "psprecomp/runtime.hpp"
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#include "../third_party/minimp3/minimp3.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <vector>

namespace vcs {
namespace {
constexpr std::uint32_t invalid_handle = 0x80671001u, unreserved = 0x80671102u;
constexpr std::uint32_t not_initialized = 0x80671103u, no_resources = 0x80671201u;
constexpr std::uint32_t bad_address = 0x80671002u, bad_size = 0x80671003u, invalid_data = 0x807F00FDu;
constexpr std::uint32_t work_size = 0x5C0u;
struct Stream {
    mp3dec_t decoder{};
    std::uint64_t start{}, end{}, position{}, consumed{};
    std::uint32_t input{}, input_size{}, pcm{}, pcm_size{}, last_request{}, samples{};
    int channels{}, rate{}, bitrate{}, version{}, frame_samples{1152}, loops{-1};
    bool initialized{}, low_level{};
    std::vector<std::uint8_t> pending;
    Stream() { mp3dec_init(&decoder); }
    std::uint32_t capacity() const { return ((input_size - work_size) / 2u) * 2u; }
    std::uint32_t write_offset() const { return static_cast<std::uint32_t>((position - start) % capacity()); }
    std::uint32_t needed() const {
        if (low_level || position >= end) return 0u;
        const auto half = capacity() / 2u;
        const auto window_end = (consumed / half + 2u) * half;
        const auto written = position - start;
        if (written >= window_end) return 0u;
        return static_cast<std::uint32_t>(std::min({window_end - written,
            std::uint64_t(capacity() - write_offset()), end - position}));
    }
    void reset() {
        position = start; consumed = 0; samples = 0; pending.clear(); last_request = 0;
        mp3dec_init(&decoder);
    }
};
struct Pool { bool initialized{}; std::array<std::unique_ptr<Stream>, 2> streams; };
bool pointers(psprecomp::Runtime &rt, std::initializer_list<std::uint32_t> addresses) {
    return std::all_of(addresses.begin(), addresses.end(), [&](auto address) { return rt.memory().raw_pointer(address, 4u); });
}
std::uint64_t word64(psprecomp::Runtime &rt, std::uint32_t address) {
    return rt.memory().load32(address) | (std::uint64_t(rt.memory().load32(address + 4u)) << 32u);
}
}

void install_mp3_services(psprecomp::Runtime &runtime) {
    auto pool = std::make_shared<Pool>();
    const auto bind = [&](std::uint32_t nid, const char *name, auto handler) {
        runtime.nids().add("sceMp3", nid, name); runtime.register_hle("sceMp3", nid, std::move(handler));
    };
    const auto lookup = [pool](psprecomp::AllegrexContext &ctx, bool require_init) -> Stream * {
        const auto handle = ctx.gpr[4];
        if (handle >= pool->streams.size()) { ctx.set_gpr(2, invalid_handle); return nullptr; }
        auto *stream = pool->streams[handle].get();
        if (!stream) { ctx.set_gpr(2, require_init ? not_initialized : unreserved); return nullptr; }
        if (require_init && !stream->initialized) { ctx.set_gpr(2, not_initialized); return nullptr; }
        return stream;
    };
    bind(0x35750070u, "sceMp3InitResource", [pool](auto &, auto &ctx) { pool->initialized = true; ctx.set_gpr(2, 0u); });
    bind(0x3C2FA058u, "sceMp3TermResource", [pool](auto &, auto &ctx) {
        for (auto &stream : pool->streams) stream.reset();
        pool->initialized = false; ctx.set_gpr(2, 0u);
    });
    bind(0xF5478233u, "sceMp3ReleaseMp3Handle", [pool](auto &, auto &ctx) {
        if (ctx.gpr[4] >= pool->streams.size()) { ctx.set_gpr(2, invalid_handle); return; }
        pool->streams[ctx.gpr[4]].reset(); ctx.set_gpr(2, 0u);
    });
    bind(0x07EC321Au, "sceMp3ReserveMp3Handle", [pool](auto &rt, auto &ctx) {
        if (!pool->initialized) { ctx.set_gpr(2, no_resources); return; }
        auto slot = std::find_if(pool->streams.begin(), pool->streams.end(), [](const auto &item) { return !item; });
        if (slot == pool->streams.end()) { ctx.set_gpr(2, no_resources); return; }
        auto stream = std::make_unique<Stream>();
        const auto parameter = ctx.gpr[4];
        if (parameter) {
            if (!rt.memory().raw_pointer(parameter, 32u)) { ctx.set_gpr(2, bad_address); return; }
            stream->start = word64(rt, parameter); stream->end = word64(rt, parameter + 8u);
            stream->input = rt.memory().load32(parameter + 16u); stream->input_size = rt.memory().load32(parameter + 20u);
            stream->pcm = rt.memory().load32(parameter + 24u); stream->pcm_size = rt.memory().load32(parameter + 28u);
            if (stream->start >= stream->end || stream->input_size < 8192u || stream->pcm_size < 9216u) {
                ctx.set_gpr(2, bad_size); return;
            }
            if (!rt.memory().raw_pointer(stream->input, stream->input_size) || !rt.memory().raw_pointer(stream->pcm, stream->pcm_size)) {
                ctx.set_gpr(2, bad_address); return;
            }
            stream->position = stream->start;
        } else stream->low_level = true;
        const auto handle = static_cast<std::uint32_t>(slot - pool->streams.begin());
        *slot = std::move(stream); ctx.set_gpr(2, handle);
    });
    bind(0xA703FE0Fu, "sceMp3GetInfoToAddStreamData", [lookup](auto &rt, auto &ctx) {
        auto *s = lookup(ctx, false); if (!s) return;
        if (s->low_level) { ctx.set_gpr(2, unreserved); return; }
        if (!pointers(rt, {ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]})) { ctx.set_gpr(2, bad_address); return; }
        if (s->position > 0xFFFFFFFFull) { ctx.set_gpr(2, bad_size); return; }
        s->last_request = s->needed();
        rt.memory().store32(ctx.gpr[5], s->input + work_size + s->write_offset());
        rt.memory().store32(ctx.gpr[6], s->last_request); rt.memory().store32(ctx.gpr[7], static_cast<std::uint32_t>(s->position));
        ctx.set_gpr(2, 0u);
    });
    bind(0x0DB149F4u, "sceMp3NotifyAddStreamData", [lookup](auto &rt, auto &ctx) {
        auto *s = lookup(ctx, false); if (!s) return;
        if (s->low_level) { ctx.set_gpr(2, unreserved); return; }
        const auto count = ctx.gpr[5];
        if (count > s->needed()) { ctx.set_gpr(2, bad_size); return; }
        const auto *bytes = rt.memory().raw_pointer(s->input + work_size + s->write_offset(), count);
        if (!bytes) { ctx.set_gpr(2, bad_address); return; }
        s->pending.insert(s->pending.end(), bytes, bytes + count); s->position += count; s->last_request = 0;
        ctx.set_gpr(2, 0u);
    });
    bind(0xD0A56296u, "sceMp3CheckStreamDataNeeded", [lookup](auto &, auto &ctx) {
        auto *s = lookup(ctx, false); if (s) ctx.set_gpr(2, s->needed() ? 1u : 0u);
    });
    bind(0x44E07129u, "sceMp3Init", [lookup](auto &, auto &ctx) {
        auto *s = lookup(ctx, false); if (!s) return;
        if (s->low_level || s->pending.size() < 4u) { ctx.set_gpr(2, invalid_data); return; }
        mp3dec_t probe{}; mp3dec_init(&probe);
        std::array<mp3d_sample_t, MINIMP3_MAX_SAMPLES_PER_FRAME> scratch{}; mp3dec_frame_info_t info{};
        const auto samples = mp3dec_decode_frame(&probe, s->pending.data(), static_cast<int>(s->pending.size()), scratch.data(), &info);
        if (!samples || info.layer != 3 || info.channels < 1 || info.channels > 2) { ctx.set_gpr(2, invalid_data); return; }
        s->channels = info.channels; s->rate = info.hz; s->bitrate = info.bitrate_kbps; s->frame_samples = samples;
        s->version = (s->pending[static_cast<std::size_t>(info.frame_offset) + 1u] >> 3u) & 3u;
        s->initialized = true; ctx.set_gpr(2, 0u);
    });
    bind(0xD021C0FBu, "sceMp3Decode", [lookup](auto &rt, auto &ctx) {
        auto *s = lookup(ctx, true); if (!s) return;
        if (s->low_level) { ctx.set_gpr(2, unreserved); return; }
        if (!pointers(rt, {ctx.gpr[5]})) { ctx.set_gpr(2, bad_address); return; }
        if (s->pending.empty()) {
            if (s->position >= s->end && s->loops != 0) { if (s->loops > 0) --s->loops; s->reset(); }
            rt.memory().store32(ctx.gpr[5], s->pcm); ctx.set_gpr(2, 0u); return;
        }
        std::array<mp3d_sample_t, MINIMP3_MAX_SAMPLES_PER_FRAME> pcm{}; mp3dec_frame_info_t info{};
        const auto samples = mp3dec_decode_frame(&s->decoder, s->pending.data(), static_cast<int>(s->pending.size()), pcm.data(), &info);
        const auto consumed = std::min(s->pending.size(), static_cast<std::size_t>(std::max(0, info.frame_bytes)));
        s->pending.erase(s->pending.begin(), s->pending.begin() + consumed); s->consumed += consumed;
        if (!consumed && s->position >= s->end) { s->consumed += s->pending.size(); s->pending.clear(); }
        const auto bytes = static_cast<std::uint32_t>(samples * info.channels * sizeof(mp3d_sample_t));
        if (bytes > s->pcm_size) { ctx.set_gpr(2, bad_size); return; }
        rt.memory().copy_in(s->pcm, std::span(reinterpret_cast<const std::uint8_t *>(pcm.data()), bytes));
        rt.memory().store32(ctx.gpr[5], s->pcm); s->samples += samples; ctx.set_gpr(2, bytes);
    });
    const auto query = [&](std::uint32_t nid, const char *name, auto field) {
        bind(nid, name, [lookup, field](auto &, auto &ctx) { if (auto *s = lookup(ctx, true)) ctx.set_gpr(2, field(*s)); });
    };
    query(0x354D27EAu, "sceMp3GetSumDecodedSample", [](const auto &s) { return s.samples; });
    query(0x7F696782u, "sceMp3GetMp3ChannelNum", [](const auto &s) { return s.channels; });
    query(0x87677E40u, "sceMp3GetBitRate", [](const auto &s) { return s.bitrate; });
    query(0x87C263D1u, "sceMp3GetMaxOutputSample", [](const auto &s) { return s.frame_samples; });
    query(0x8F450998u, "sceMp3GetSamplingRate", [](const auto &s) { return s.rate; });
    query(0xAE6D2027u, "sceMp3GetMPEGVersion", [](const auto &s) { return s.version; });
    query(0xD8F54A51u, "sceMp3GetLoopNum", [](const auto &s) { return s.loops; });
    bind(0x3CEF484Fu, "sceMp3SetLoopNum", [lookup](auto &, auto &ctx) {
        if (auto *s = lookup(ctx, false)) { s->loops = static_cast<std::int32_t>(ctx.gpr[5]); ctx.set_gpr(2, 0u); }
    });
    bind(0x2A368661u, "sceMp3ResetPlayPosition", [lookup](auto &, auto &ctx) {
        if (auto *s = lookup(ctx, true)) { s->reset(); ctx.set_gpr(2, 0u); }
    });
    bind(0x1B839B83u, "sceMp3LowLevelInit", [lookup](auto &, auto &ctx) {
        if (auto *s = lookup(ctx, false)) { mp3dec_init(&s->decoder); s->initialized = true; ctx.set_gpr(2, 0u); }
    });
    bind(0xE3EE2C81u, "sceMp3LowLevelDecode", [lookup](auto &rt, auto &ctx) {
        auto *s = lookup(ctx, true); if (!s) return;
        const auto *input = rt.memory().raw_pointer(ctx.gpr[5], 4096u);
        if (!input || !pointers(rt, {ctx.gpr[6], ctx.gpr[8]}) || !rt.memory().raw_pointer(ctx.gpr[7], 4608u)) {
            ctx.set_gpr(2, bad_address); return;
        }
        std::array<mp3d_sample_t, MINIMP3_MAX_SAMPLES_PER_FRAME> pcm{}; mp3dec_frame_info_t info{};
        const auto samples = mp3dec_decode_frame(&s->decoder, input, 4096, pcm.data(), &info);
        const auto bytes = static_cast<std::uint32_t>(samples * info.channels * sizeof(mp3d_sample_t));
        rt.memory().copy_in(ctx.gpr[7], std::span(reinterpret_cast<const std::uint8_t *>(pcm.data()), bytes));
        rt.memory().store32(ctx.gpr[6], std::max(0, info.frame_bytes)); rt.memory().store32(ctx.gpr[8], bytes);
        s->samples += samples; ctx.set_gpr(2, 0u);
    });
}
}
