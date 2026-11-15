#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include <array>

namespace ac3::sim {
void Game::model_draw_callback(u32 callback, u32 work) {
    if (callback == 0x8008D4E8u) projected_glow(work);
    else call_by_address(callback, "model draw callback (exe_e)", work);
}

void Game::model_project_vertices(u32 stream, u32 work, bool prefetch) {
    s32 remaining = m.r32s(stream);
    u32 source = stream + 4, screens = work + 16, depths = work + 676;
    model_exact_on_ = gte.precise && player_settings;
    u32 vertex = 0;
    auto readGroup = [&](u32 address) {
        return std::array<u32, 5>{m.r32(address), m.r32(address + 4), m.r32(address + 8),
            m.r32(address + 12), m.r32(address + 16)};
    };
    auto group = readGroup(source);
    do {
        gte.mtc2(0, group[0]); gte.mtc2(2, group[1]); gte.mtc2(4, group[2]);
        if (prefetch) {
            gte.mtc2(1, group[3]); gte.mtc2(3, group[3] >> 16); gte.mtc2(5, group[4]);
        } else {
            gte.mtc2(5, group[4]); gte.mtc2(1, group[3]);
            gte.mtc2(3, static_cast<u32>(static_cast<s32>(group[3]) >> 16));
        }
        gte.op(0x280030);
        source += 20;
        remaining = wrap::sub(remaining, 3);
        if (prefetch) group = readGroup(source);
        for (u32 corner = 0; corner < 3; ++corner) m.w32(screens + 4 * corner, gte.mfc2(12 + corner));
        for (u32 corner = 0; corner < 3; ++corner) m.w16(depths + 2 * corner, gte.mfc2(17 + corner));
        if (model_exact_on_)
            for (u32 corner = 0; corner < 3; ++corner)
                if (vertex + corner < 256) model_exact_[vertex + corner] = gte.exact_fifo[corner];
        screens += 12; depths += 6; vertex += 3;
        if (!prefetch && remaining > 0) group = readGroup(source);
    } while (remaining > 0);
}

u32 Game::draw_submeshes(u32 entries, u32 work) {
    cover(0x80062BD4u);
    Frame frame(*this, 0x20);
    for (u32 entry = entries;; entry += 56) {
        const u32 part = m.r32(entry + 8);
        if (static_cast<s32>(part) < 0) return part;
        m.w32(work + 12, part);
        for (u32 control = 0; control < 8; ++control) gte.ctc2(control, m.r32(entry + 20 + 4 * control));
        const u32 callback = m.r32(entry + 52);
        if (callback) model_draw_callback(callback, work);
        model_project_vertices(m.r32(entry + 12), work, true);
        m.w32(work + 8, m.r32(m.r32(entry + 16) + 20)); model_stored_triangles(work);
        m.w32(work + 8, m.r32(m.r32(entry + 16) + 24)); model_stored_quads(work);
    }
}

u32 Game::draw_aircraft_models(u32 entries, u32 work) {
    cover(0x800637B0u);
    Frame frame(*this, 0x38);
    for (u32 entry = entries;; entry += 160) {
        const u32 scale = m.r32(entry);
        if (static_cast<s32>(scale) < 0) return scale;
        const u32 localTable = m.r32(work + 4);
        m.w32(work + 4, localTable + 156);
        clear_otag_r(localTable, 39);
        const u32 depth = m.r32(entry + 84);
        m.w16(work + 1014, depth);
        if (depth - 31u < 0x77D9u) {
            const u32 worldEntry = m.r32(work) + 4 * (depth >> 3);
            m.w32(localTable, (m.r32(localTable) & 0xFF000000u) | (m.r32(worldEntry) & 0xFFFFFFu));
            m.w32(worldEntry, (m.r32(worldEntry) & 0xFF000000u) | ((localTable + 152) & 0xFFFFFFu));
        }
        m.w32(work + 1016, localTable + 76);
        m.w32(work + 12, m.r32(entry));
        for (u32 control = 0; control < 8; ++control) gte.ctc2(control, m.r32(entry + 88 + 4 * control));
        for (u32 control = 0; control < 5; ++control) gte.ctc2(8 + control, m.r32(entry + 120 + 4 * control));
        const u32 state = m.r32(entry + 152);
        m.w32(work + 1020, state);
        if (state) {
            const u32 callback = m.r32(entry + 156);
            if (callback) model_draw_callback(callback, work);
        }
        model_depth_ = true;
        for (u32 part = 0; part < 5; ++part) {
            const u32 model = m.r32(entry + 24 + 4 * part);
            model_project_vertices(m.r32(entry + 4 + 4 * part), work, false);
            if (part) {
                m.w32(work + 1008, m.r32(model + 16));
                m.w32(work + 8, m.r32(model + 20)); model_lit_triangles(work);
                m.w32(work + 8, m.r32(model + 24)); model_lit_quads(work);
            } else {
                for (u32 colour = 0; colour < 3; ++colour)
                    gte.ctc2(21 + colour, static_cast<u32>(static_cast<s32>(gte.cfc2(13 + colour)) >> 2));
                s32 fog = 3584;
                const u32 thrustState = m.r32(work + 1020);
                if (thrustState) {
                    const s32 thrust = m.r16s(thrustState + 6);
                    if (thrust < 2048) fog = 4096 - (thrust >> 2);
                    else {
                        const s32 extra = thrust - 2048;
                        fog = 3584 - (2 * extra - (extra >> 2));
                    }
                    if (m.r16(work + 1012) & 1u) fog += 8 + (fog >> 5);
                }
                gte.mtc2(8, static_cast<u32>(fog));
                model_fx_ = true;
                m.w32(work + 8, m.r32(model + 20)); model_fog_triangles(work);
                m.w32(work + 8, m.r32(model + 24)); model_fog_quads(work);
                model_fx_ = false;
            }
        }
        model_depth_ = false;
    }
}
}
