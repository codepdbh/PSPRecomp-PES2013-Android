#include "psp_mp3_services.hpp"
#include "psprecomp/runtime.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace { void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); } }
int main() {
    try {
        {
            psprecomp::Runtime rt; vcs::install_mp3_services(rt);
            auto call = [&](std::uint32_t nid, std::uint32_t argument = 0u) {
                rt.cpu().set_gpr(4u, argument); rt.invoke_import("sceMp3", nid, rt.cpu()); return rt.cpu().gpr[2];
            };
            require(call(0x07EC321Au) == 0x80671201u, "MP3 handle reserved without resources");
            call(0x35750070u);
            require(call(0x07EC321Au) == 0u && call(0x07EC321Au) == 1u, "MP3 pool did not allocate distinct handles");
            require(call(0x07EC321Au) == 0x80671201u, "MP3 pool exceeded its two handles");
            call(0xF5478233u, 0u);
            require(call(0x07EC321Au) == 0u, "Released low handle was not reused");
            require(call(0xF5478233u, 0xFFFFFFFFu) == 0x80671001u, "Invalid MP3 release returned success");
            call(0x3C2FA058u);
            require(call(0x07EC321Au) == 0x80671201u, "Terminated MP3 pool stayed initialized");
        }
        std::ifstream file(PES2013_MP3_TEST_ASSET, std::ios::binary);
        std::vector<std::uint8_t> encoded((std::istreambuf_iterator<char>(file)), {});
        require(encoded.size() > 1000u && encoded.size() < 6720u, "Missing MP3 test tone");
        psprecomp::Runtime rt; vcs::install_mp3_services(rt);
        auto &ctx = rt.cpu();
        auto call = [&](std::uint32_t nid) {
            rt.invoke_import("sceMp3", nid, ctx); require(!rt.stopped(), "MP3 service stopped unexpectedly"); return ctx.gpr[2];
        };
        call(0x35750070u);
        constexpr std::uint32_t parameter = 0x08810000u, input = 0x08820000u, pcm = 0x08830000u, results = 0x08810100u;
        rt.memory().store32(parameter + 8u, static_cast<std::uint32_t>(encoded.size()));
        rt.memory().store32(parameter + 16u, input); rt.memory().store32(parameter + 20u, 8192u);
        rt.memory().store32(parameter + 24u, pcm); rt.memory().store32(parameter + 28u, 9216u);
        ctx.set_gpr(4u, parameter); const auto handle = call(0x07EC321Au);
        require(handle == 0u, "Valid stream could not reserve MP3 handle");
        auto info = [&] {
            ctx.set_gpr(4u, handle); ctx.set_gpr(5u, results); ctx.set_gpr(6u, results + 4u); ctx.set_gpr(7u, results + 8u);
            require(call(0xA703FE0Fu) == 0u, "MP3 stream request failed");
        };
        info();
        const auto destination = rt.memory().load32(results), requested = rt.memory().load32(results + 4u);
        require(destination == input + 0x5C0u && requested == encoded.size() && rt.memory().load32(results + 8u) == 0u,
                "MP3 stream request did not respect workarea and file position");
        ctx.set_gpr(4u, handle); ctx.set_gpr(5u, requested + 1u);
        require(call(0x0DB149F4u) == 0x80671003u, "Oversized MP3 notification was accepted");
        info(); require(rt.memory().load32(results + 8u) == 0u, "Rejected MP3 data moved file position");
        rt.memory().copy_in(destination, encoded);
        ctx.set_gpr(4u, handle); ctx.set_gpr(5u, requested); require(call(0x0DB149F4u) == 0u, "MP3 stream notification failed");
        ctx.set_gpr(4u, handle); require(call(0x44E07129u) == 0u, "MP3 header initialization failed");
        auto query = [&](std::uint32_t nid) { ctx.set_gpr(4u, handle); return call(nid); };
        require(query(0x8F450998u) == 44100u && query(0x7F696782u) == 2u && query(0x87677E40u) == 128u,
                "MP3 metadata differs from test tone");
        ctx.set_gpr(4u, handle); ctx.set_gpr(5u, 0u); call(0x3CEF484Fu);
        std::vector<std::int16_t> left;
        for (unsigned frame = 0; frame < 64u; ++frame) {
            ctx.set_gpr(4u, handle); ctx.set_gpr(5u, results);
            const auto bytes = call(0xD021C0FBu);
            require(bytes <= 4608u && bytes % 4u == 0u && rt.memory().load32(results) == pcm, "MP3 output range is invalid");
            if (!bytes) break;
            for (unsigned i = 0; i < bytes; i += 4u) left.push_back(static_cast<std::int16_t>(rt.memory().load16(pcm + i)));
        }
        std::uint64_t energy = 0; unsigned crossings = 0;
        for (std::size_t i = 0; i < left.size(); ++i) {
            energy += static_cast<std::int64_t>(left[i]) * left[i];
            if (i && left[i - 1u] < 0 && left[i] >= 0) ++crossings;
        }
        require(left.size() > 8000u && energy > 100000000u && crossings > 150u && crossings < 400u,
                "MP3 decoder did not recover the synthetic tone");
        require(query(0x354D27EAu) == left.size(), "MP3 decoded-sample counter is inconsistent");
        ctx.set_gpr(4u, handle); call(0x2A368661u); info();
        require(rt.memory().load32(results + 8u) == 0u, "MP3 reset did not rewind file position");
        std::cout << "MP3 handle lifecycle, stream bounds, metadata and real PCM decoding passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
