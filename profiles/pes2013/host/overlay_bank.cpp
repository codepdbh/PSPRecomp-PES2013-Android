#include "overlay_bank.hpp"
#include "psprecomp/common.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>
namespace pes2013 {
void install_overlay_bank(psprecomp::Runtime &runtime) {
    auto active = std::make_shared<std::vector<const OverlayModule *>>();
    const auto catalog = generated_overlay_modules();
    const auto first = std::min_element(catalog.begin(), catalog.end(), [](const auto &a, const auto &b) { return a.base < b.base; });
    const auto lowest_base = first == catalog.end() ? 0xFFFFFFFFu : first->base;
    runtime.set_code_resolver([active, lowest_base](psprecomp::Runtime &rt, std::uint32_t pc) {
        pc = rt.memory().canonical(pc);
        if (pc < lowest_base) return;
        // Most dispatches in an overlay stay in the already active module.
        // Check those few headers first, rather than scanning the full catalog
        // for every generated block, while still detecting a replaced slot.
        for (const auto *module : *active) {
            if (pc < module->base + 64u || pc >= module->base + 64u + module->text_size) continue;
            const auto *header = rt.memory().raw_pointer(module->base, 64u);
            if (header && std::memcmp(header, module->header.data(), 64u) == 0) return;
        }
        const auto modules = generated_overlay_modules();
        for (const auto &module : modules) {
            if (pc < module.base + 64u || pc >= module.base + 64u + module.text_size) continue;
            const auto *header = rt.memory().raw_pointer(module.base, 64u);
            if (!header || std::memcmp(header, module.header.data(), 64u)) continue;
            if (std::find(active->begin(), active->end(), &module) != active->end()) return;
            if (!module.register_code) { rt.stop(std::string("Overlay has no linked AOT code: ") + module.name); return; }
            const auto *text = rt.memory().raw_pointer(module.base + 64u, module.text_size);
            if (!text) { rt.stop("Overlay code range outside RAM"); return; }
            std::uint64_t hash = 14695981039346656037ull;
            for (unsigned i = 0; i < module.text_size; ++i) { hash ^= text[i]; hash *= 1099511628211ull; }
            if (hash != module.text_hash) { rt.stop(std::string("Overlay text differs from AOT input: ") + module.name); return; }
            for (auto it = active->begin(); it != active->end();) {
                const auto *old = *it;
                if (old->base < module.base + module.memory_size && module.base < old->base + old->memory_size) {
                    rt.unregister_functions(old->base + 64u, old->base + 64u + old->text_size);
                    it = active->erase(it);
                } else ++it;
            }
            module.register_code(rt); active->push_back(&module);
            std::cerr << "[overlay] activated " << module.name << " at " << psprecomp::hex32(module.base) << '\n';
            return;
        }
        for (const auto *module : *active)
            if (pc >= module->base + 64u && pc < module->base + 64u + module->text_size) {
                rt.stop("Active overlay slot was overwritten by an unidentified module"); return;
            }
    });
}
}
