#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kO = 0x80112DC4u;
constexpr u32 kDisplay = 0x800BCE48u;
constexpr u32 kP2 = 0x80112DF0u, kBrief = 0x80112DC8u, kCards = 0x80112DECu;
constexpr u32 kLoader = 0x80112E0Cu, kHangar = 0x80112DA8u;
constexpr u32 kRecordLoaded = 0x8009B904u;

u32 O(Memory& m) { return m.r32(kO); }
u32 sc(Memory& m) { return m.r32(g::kScene); }
u32 req_list(Memory& m) { return m.r32(g::kExit) + 0x60; }
}

s32 Game::o2_dir_entries(u32 dirp, u32 out) {
    cover(0x800ECD4Cu);
    const u32 d = m.r32(dirp);
    const s32 n = m.r32s(d);
    for (s32 i = 0; i < n; ++i) m.w32(out + 4 * static_cast<u32>(i), d + 4 * static_cast<u32>(m.r32s(d + 4 + 4 * static_cast<u32>(i)) / 4));
    return n;
}

u32 Game::o2_upload_pack(u32 pack) {
    cover(0x800ECDA8u);
    Frame f(*this, 0x20);
    const u32 buf = mem_alloc(0x30000);
    u32 dir, n;
    if (m.r32(pack) == 0x1A7A6C55u) {
        ulz_decode(pack, buf);
        dir = ld_relocate_dir(buf);
        n = m.r32(buf);
    } else {
        dir = ld_relocate_dir(pack);
        n = m.r32(pack);
    }
    ld_upload_tims(dir, static_cast<s32>(n));
    heap_set(buf);
    return 1;
}

u32 Game::o2_upload_tim(u32 tim) {
    cover(0x800ECF88u);
    Frame f(*this, 0x20);
    const u32 buf = mem_alloc(0x9000);
    u32 t = tim;
    if (m.r32(tim) == 0x1A7A6C55u) {
        ulz_decode(tim, buf);
        t = buf;
    }
    load_tim(t);
    return heap_set(buf);
}

void Game::o2_pilots_alloc() {
    cover(0x800E37F8u);
    Frame f(*this, 0x20);
    m.w32(kP2, mem_alloc(0x360));
    m.w32(m.r32(kP2) + 0x2E8, mem_alloc(0x80));
    for (u32 i = 0; i < 4; ++i) {
        const u32 b = mem_alloc(0x480);
        if (i < 3) m.w32(m.r32(kP2) + 4 * i + 0x2EC, b);
        else m.w32(m.r32(kP2) + 0x2F8, b);
    }
    for (u32 i = 0; i < 7; ++i) {
        const u32 b = mem_alloc(0x180);
        if (i >= 6) m.w32(m.r32(kP2) + 0x314, b);
        else m.w32(m.r32(kP2) + 4 * i + 0x2FC, b);
    }
}

void Game::o2_cards_setup(u32 dirp, u32 pal) {
    cover(0x800E38DCu);
    Frame f(*this, 0x30);
    const u32 b = mem_alloc(0x280);
    m.w32(kCards, b);
    m.w16(b, m.r16(0x800CCD38u));
    m.w8(b + 2, m.r8(0x800CCD3Au));
    m.w8(m.r32(kCards) + 2, 19);
    m.w8(m.r32(kCards) + 3, 2);
    u32 d = m.r32(kCards) + 4, s = 0x800CCD3Cu;
    for (; s != 0x800CCD5Cu; s += 16, d += 16)
        for (u32 k = 0; k < 16; k += 4) m.w32(d + k, m.r32(s + k));
    m.w32(d, m.r32(s));
    m.w16(d + 4, m.r16(s + 4));
    m.w8(d + 6, m.r8(s + 6));
    for (u32 k = 0; k < 3; ++k) m.w32(m.r32(kCards) + 0x44 + 4 * k, 0);
    const u32 c = m.r32(kCards);
    m.w16(c + 0x50, 1);
    for (u32 k = 0; k < 4; ++k) m.w8(c + 0x52 + k, m.r8(0x800CCD64u + k));
    m.w8(c + 0x56, 0);
    m.w8(m.r32(kCards) + 0x57, 0);
    m.w32(m.r32(kCards) + 0x58, 0);
    m.w32(m.r32(kCards) + 0x5C, 0);
    const u32 ents = f.at(0x10);
    const s32 n = o2_dir_entries(dirp, ents);
    m.w8(m.r32(kCards) + 2, static_cast<u32>(n + 16));
    for (u32 k = 0; k < 8; ++k) m.w32(m.r32(kCards) + 0x60 + 4 * k, m.r32(m.r32(ents) + 0x14 + 4 * k));
    for (s32 j = 0; j < n; ++j)
        for (u32 k = 0; k < 32; ++k)
            m.w32(m.r32(kCards) + 0x80 + 128 * static_cast<u32>(j) + 4 * k, m.r32(m.r32(ents + 4 * static_cast<u32>(j)) + 0x40 + 4 * k));
    for (u32 k = 0; k < 32; ++k) m.w32(m.r32(kCards) + 0x200 + 4 * k, m.r32(pal + 4 * k));
}

void Game::o2_voice_alloc() {
    cover(0x800F74C8u);
    Frame frame(*this, 24);
    const u32 b = mem_alloc(0x101C);
    m.w32(b + 0xC, 0x3FFF);
    m.w32(b + 4, 0);
    m.w32(b + 8, 0);
    m.w32(b, 0);
    m.w32(b + 0x10, 255);
    m.w32(b + 0x14, 0);
    m.w32(kLoader, b);
}

void Game::o2_voice_script(u32 s) {
    cover(0x800F7514u);
    m.w32(m.r32(kLoader) + 0x1018, s);
}

u32 Game::o2_brief_kind(s32 mission) {
    cover(0x800FB2A0u);
    if (!mission) {
        m.w32(O(m) + 0x80, 1);
        return 1;
    }
    if (mission < 20) return 0;
    if (mission < 35) return 1;
    if (mission < 40) return 3;
    if (mission < 45) return 2;
    if (mission < 49) return 3;
    if (mission < 54) return 2;
    if (mission == 54) return 1;
    if (mission < 57) return 2;
    if (mission < 59) return 3;
    return 0;
}

u32 Game::o2_poly_g4(u32 d, u32 p, u32 ot) {
    cover(0x800F7244u);
    m.w8(p + 3, 8);
    m.w8(p + 7, 0x38);
    m.w8(p + 7, m.r8(d + 0x13) ? 0x3Au : 0x38u);
    for (u32 k = 0; k < 4; ++k) {
        m.w8(p + 4 + 8 * k, m.r8(d + 0x10 + 4 * k));
        m.w8(p + 5 + 8 * k, m.r8(d + 0x11 + 4 * k));
        m.w8(p + 6 + 8 * k, m.r8(d + 0x12 + 4 * k));
    }
    for (u32 k = 0; k < 4; ++k) {
        m.w16(p + 8 + 8 * k, m.r16(d + 4 * k));
        m.w16(p + 10 + 8 * k, m.r16(d + 2 + 4 * k));
    }
    ot_link(ot, p);
    return p + 36;
}

u32 Game::o2_load_bar(u32 packet) { return o2_loading_progress(packet); }

u32 Game::o2_ld_common() {
    cover(0x800FB4F0u);
    Frame f(*this, 0x18);
    const u32 list = req_list(m);
    m.w32(list, 0x71);
    const u32 buf = mem_alloc(records_size(list, 1));
    m.w32(sc(m) + 0x44, 1);
    request_records(req_list(m), 1, buf, kRecordLoaded);
    m.w32(O(m) + 0x1C8, 1);
    m.w32(O(m) + 0x1CC, 0);
    return 1;
}

u32 Game::o2_ld_wait() {
    cover(0x800FB574u);
    return m.r32(sc(m) + 0x44) == 0 ? 1u : 0u;
}

u32 Game::o2_ld_common_unpack() {
    cover(0x800FB58Cu);
    Frame f(*this, 0x20);
    const u32 d = ld_relocate_dir(m.r32(req_list(m)));
    o2_upload_pack(m.r32(d));
    if (!m.r32(O(m) + 0x80)) o2_upload_pack(m.r32(d + 8));
    if (m.r32(O(m) + 0x80) == 1) o2_upload_pack(m.r32(d + 0xC));
    if (m.r32(O(m) + 0x80) == 2) o2_upload_pack(m.r32(d + 0x10));
    if (m.r32(O(m) + 0x80) == 3) o2_upload_pack(m.r32(d + 0x14));
    snd_load_bank(m.r32(d + 4));
    heap_set(m.r32(d + 4));
    if (!m.r16(O(m) + 0x1C6)) o2_group_set(2, (1u << ((m.r32(O(m) + 0x80) + 2) & 31)) | 2u);
    const u32 v = sound_play(m.r32(0x80111700u + 4 * m.r32(O(m) + 0x80)));
    m.w32(O(m) + 0x130, v);
    return 1;
}

u32 Game::o2_ld_brief() {
    cover(0x800FB6E8u);
    Frame f(*this, 0x20);
    const u32 list = req_list(m);
    m.w32(list, static_cast<u32>(m.r16s(0x80111688u + 2 * static_cast<u32>(m.r16s(sc(m) + 0x38C)))));
    const u32 size = records_size(list, 1);
    m.w32(O(m) + 0x1C8, m.r32(O(m) + 0x1C8) + size);
    const u32 buf = mem_alloc(size);
    m.w32(O(m) + 0x1D0, buf);
    m.w32(O(m) + 0x1CC, 0);
    m.w32(sc(m) + 0x44, 1);
    request_records(req_list(m), 1, buf, kRecordLoaded);
    return 1;
}

u32 Game::o2_ld_progress() {
    cover(0x800FB7A4u);
    Frame f(*this, 0x20);
    const u32 at = f.at(0x10);
    load_progress(at);
    const u32 now = m.r32(at), last = m.r32(O(m) + 0x1D0);
    m.w32(O(m) + 0x1D0, now);
    m.w32(O(m) + 0x1CC, m.r32(O(m) + 0x1CC) + now - last);
    return m.r32(sc(m) + 0x44) == 0 ? 1u : 0u;
}

u32 Game::o2_ld_brief_unpack() {
    cover(0x800FB7F4u);
    Frame f(*this, 0x18);
    const u32 d = ld_relocate_dir(m.r32(req_list(m)));
    m.w32(O(m) + 0x20, d);
    m.w32(O(m) + 0x1C0, m.r32(d + 8));
    const u32 b = m.r32(O(m) + 0x20);
    m.w32(kBrief, m.r32(b));
    o2_upload_pack(m.r32(b + 0x1C));
    o2_upload_pack(m.r32(m.r32(O(m) + 0x20) + 0x20));
    const u32 k = 0x8010E674u + 8u * static_cast<u32>(m.r16s(sc(m) + 0x38C)) + 1 + 2u * m.r8(sc(m) + 0x956);
    u32 pic = 0;
    if (!m.r8(k)) pic = m.r32(m.r32(O(m) + 0x20) + 0x10);
    else if (m.r8(k) == 1) pic = m.r32(m.r32(O(m) + 0x20) + 0x14);
    if (m.r8(k) <= 1) {
        const u32 dd = ld_relocate_dir(pic);
        o2_upload_tim(m.r32(dd));
    }
    heap_set(m.r32(m.r32(O(m) + 0x20) + 0x1C));
    m.w32(O(m) + 0x120, ld_relocate_dir(m.r32(m.r32(O(m) + 0x20) + 4)));
    return 1;
}

u32 Game::o2_ld_hangar() {
    cover(0x800FB948u);
    Frame f(*this, 0x18);
    const u32 list = req_list(m);
    m.w32(list, 0x72);
    const u32 buf = mem_alloc(records_size(list, 1));
    m.w32(O(m) + 0x1D0, buf);
    m.w32(sc(m) + 0x44, 1);
    request_records(req_list(m), 1, buf, kRecordLoaded);
    return 1;
}

u32 Game::o2_ld_hangar_wait() {
    cover(0x800FB9C4u);
    Frame f(*this, 0x20);
    const u32 at = f.at(0x10);
    load_progress(at);
    const u32 now = m.r32(at), last = m.r32(O(m) + 0x1D0);
    m.w32(O(m) + 0x1D0, now);
    m.w32(O(m) + 0x1CC, m.r32(O(m) + 0x1CC) + now - last);
    if (m.r32(sc(m) + 0x44)) return 0;
    voice_release(m.r32(O(m) + 0x130));
    return 1;
}

u32 Game::o2_ld_hangar_unpack() {
    cover(0x800FBA30u);
    Frame f(*this, 0x18);
    const u32 d = ld_relocate_dir(m.r32(req_list(m)));
    m.w32(O(m) + 0x18, d);
    snd_load_bank(m.r32(d + 0x20));
    o2_upload_pack(m.r32(m.r32(O(m) + 0x18) + 0x1C));
    heap_set(m.r32(m.r32(O(m) + 0x18) + 0x1C));
    m.w32(O(m) + 0xAC, m.r32(m.r32(O(m) + 0x18) + 0x14));
    m.w32(O(m) + 0x154, mem_alloc(0x80));
    const u32 t = mem_alloc(0x10);
    const u32 d2 = m.r32(O(m) + 0x18);
    m.w32(O(m) + 0x1BC, t);
    o2_dir_entries(d2 + 0x10, t);
    m.w32(O(m) + 0x158, ld_relocate_dir(m.r32(m.r32(O(m) + 0x18) + 0xC)));
    const u32 s = ld_relocate_dir(m.r32(m.r32(O(m) + 0x18) + 8));
    m.w32(O(m) + 0x15C, s);
    o2_set_scripts(s, 0);
    o2_cards_setup(m.r32(O(m) + 0x18), m.r32(m.r32(O(m) + 0x18) + 4));
    o2_hangar_alloc();
    return 1;
}

u32 Game::o2_ld_done() {
    cover(0x800FBB74u);
    Frame f(*this, 0x18);
    if (o2_fade_state(1) == 2 ||
        ((m.r32(sc(m) + 0x12C) & 0x10u) && !(m.r32(sc(m) + 0x128) & 0x2Fu))) {
        o2_group_set(2, 0);
        return 2;
    }
    return 0;
}

u32 Game::o2_ld_fade() {
    cover(0x800FBBE0u);
    Frame f(*this, 0x18);
    o2_fade_set(1, 1, 0, 10);
    return 2;
}

u32 Game::o2_ld_state(u32 fn) {
    switch (fn) {
    case 0x800FB4F0u: return o2_ld_common();
    case 0x800FB574u: return o2_ld_wait();
    case 0x800FB58Cu: return o2_ld_common_unpack();
    case 0x800FB6E8u: return o2_ld_brief();
    case 0x800FB7A4u: return o2_ld_progress();
    case 0x800FB7F4u: return o2_ld_brief_unpack();
    case 0x800FB948u: return o2_ld_hangar();
    case 0x800FB9C4u: return o2_ld_hangar_wait();
    case 0x800FBA30u: return o2_ld_hangar_unpack();
    case 0x800FBB74u: return o2_ld_done();
    case 0x800FBBE0u: return o2_ld_fade();
    default: {
        u32 out = 0;
        out = call_by_address(fn, "a loading state of overlay 2", 0, 0, 0, 0);
        return out;
    }
    }
}

u32 Game::o2_phase0() {
    cover(0x800F60F0u);
    Frame f(*this, 0x40);
    const u32 dp = m.r32(kDisplay);
    m.w8(dp + 0x44, 1);
    m.w32(dp + 4, 1);
    m.w32(dp + 0x14, 0);
    m.w32(dp, m.r32(dp) | 9u);
    cd_reset(0, 0);
    m.w8(m.r32(kDisplay) + 0x10, 0);
    m.w8(m.r32(kDisplay) + 0x11, 0);
    m.w8(m.r32(kDisplay) + 0x12, 0);
    kcd_voice_reset();
    m.w32(kO, mem_alloc(0x284));
    for (u32 k = 0; k < 2; ++k) {
        const u32 b = mem_alloc(0x8000);
        m.w32(O(m) + 4 * k + 0x278, b);
    }
    {
        const u32 s = sc(m);
        if (m.r8(s + 0x3BC) == 0x36) m.w8(s + 0x3BC, 0x22);
        if (m.r8(sc(m) + 0x3BC) == 0x3A) m.w8(sc(m) + 0x3BC, 0x27);
        if (m.r8(sc(m) + 0x3BD) == 0x38) m.w8(sc(m) + 0x3BD, 0x2A);
        if (m.r8(sc(m) + 0x3BD) == 0x39) m.w8(sc(m) + 0x3BD, 0x25);
        const u32 s2 = sc(m);
        if (m.r8(s2 + 0x957) == 1) {
            const u32 v = m.r8(0x8010E638u + static_cast<u32>(m.r16s(s2 + 0x38C)));
            m.w16(s2 + 0x954, 0);
            m.w16(s2 + 0x38C, v);
        }
    }
    m.w32(O(m) + 0x80, o2_brief_kind(m.r16s(sc(m) + 0x38C)));
    for (u32 gi = 0; gi < 3; ++gi) {
        const u32 b = mem_alloc(0x180);
        m.w32(O(m) + 4 * gi + 0x16C, b);
        for (u32 i = 0; i < 32; ++i) {
            const u32 e = m.r32(O(m) + 4 * gi + 0x16C) + 12 * i;
            m.w16(e, 0);
            m.w16(m.r32(O(m) + 4 * gi + 0x16C) + 12 * i + 2, 0);
            o2_entry_reset(gi, i);
        }
    }
    {
        const u32 b = mem_alloc(0x10);
        m.w32(O(m) + 0x1AC, b);
        m.w16(b + 0xA, 0xFFFF);
    }
    for (u32 k = 0; k < 2; ++k) {
        m.w32(O(m) + 4 * k, mem_alloc(0x40));
        const u32 b = mem_alloc(0x400);
        m.w32(O(m) + 4 * k + 0xC, b);
    }
    for (u32 k = 0; k < 2; ++k) {
        const u32 b = mem_alloc(0x129C);
        m.w32(O(m) + 4 * k + 0x38, b);
    }
    o2_screens_setup();
    o2_set_scripts(ld_relocate_dir(0x8010BBF0u), 2);
    o2_group_set(0, 0);
    o2_group_set(1, 0);
    o2_group_set(2, 1);
    o2_text_setup();
    m.w32(g::kWorld, mem_alloc(0x2BC));
    o2_pilots_alloc();
    o2_voice_alloc();
    o2_voice_script(0x8010E854u);
    m.w16(O(m) + 0x1C6, 0);
    reverb_on();
    {
        const u32 s = sc(m);
        if (m.r32(s + 0x96C) & 0x10u) {
            m.w32(O(m) + 0xC4, 0);
            if (m.r16s(s + 0x954) == 1) m.w16(s + 0x954, 3);
            const u32 s2 = sc(m);
            if (m.r32(s2 + 0x96C) & 0x10u) m.w16(s2 + 0x38C, 0x3B);
        }
    }
    {
        const u32 s = sc(m);
        if (m.r16s(s + 0x954) == 1) {
            switch (m.r8(s + 0x3BC)) {
            case 0x12: case 0x22: case 0x27: case 0x30: case 0x35: {
                m.w32(s + 0x96C, m.r32(s + 0x96C) | 8u);
                m.w32(O(m) + 0xE0, 2);
                if (o2_mission_saved(m.r8(s + 0x3BC))) {
                    const u32 s2 = sc(m);
                    const u32 v = m.r32(s2 + 0x96C);
                    if (v & 0x20u) {
                        m.w32(s2 + 0x96C, v & ~0x20u);
                        m.w32(O(m) + 0xE0, 3);
                    } else {
                        m.w32(s2 + 0x96C, v | 0x20u);
                    }
                }
                break;
            }
            default:
                m.w32(s + 0x96C, m.r32(s + 0x96C) & ~8u);
                break;
            }
            if (m.r32(sc(m) + 0x96C) & 8u) {
                o2_shop_reset();
                return 0x4000000u;
            }
        }
    }
    const u32 s = sc(m);
    if (m.r8(s + 0x95F) == 1) return 0x4000000u;
    if (m.r32(s + 0x96C) & 1u) return 0x4000000u;
    if (m.r8(s + 0x957) == 1) return 0x4000000u;
    const s32 st = m.r16s(s + 0x954);
    if (st == 1) {
        m.w16(s + 0x38C, m.r8(s + 0x3BD));
        if (m.r32(s + 0x96C) & 0x10u) m.w16(s + 0x38C, 0x3B);
    } else if (st == 0 || st == 2) {
        if (m.r32(s + 0x96C) & 0x10u) m.w16(s + 0x38C, 0x3B);
        return 0x4000000u;
    } else if (st == 3) {
        m.w16(s + 0x38C, 0x3B);
    }
    o2_shop_reset();
    o2_sequence_setup(f.at(16));
    return 0x4000000u;
}

u32 Game::o2_phase1() {
    cover(0x800F6848u);
    Frame f(*this, 0x28);
    const u32 s = sc(m);
    const u32 o = O(m);
    m.w32(o + 8, m.r32(o + 4 * m.r32(s + 4)));
    clear_otag_r(m.r32(O(m) + 8), 16);
    {
        const u32 o2 = O(m), s2 = sc(m);
        const u32 ot = m.r32(o2 + 8), top = m.r32(s2 + 8);
        m.w32(ot, (m.r32(ot) & 0xFF000000u) | (m.r32(top) & 0x00FFFFFFu));
        m.w32(top, (m.r32(top) & 0xFF000000u) | ((ot + 60) & 0x00FFFFFFu));
        m.w32(o2 + 0x274, m.r32(o2 + 4 * m.r32(s2 + 4) + 0x278));
    }
    const u32 v = m.r32(sc(m) + 0x96C);
    if (v & 1u) return 0x4000000u;
    if (v & 8u) return 0x4000000u;
    if (m.r8(sc(m) + 0x95F) == 1) return 0x4000000u;
    const s32 st = m.r16s(sc(m) + 0x954);
    if (st != 1 && st != 3) return 0x4000000u;
    if (m.r8(sc(m) + 0x957) == 1) return 0x4000000u;
    return o2_sequence_phase();
}

u32 Game::o2_phase2() {
    cover(0x800FBC48u);
    Frame f(*this, 0x20);
    m.w8(O(m) + 0x7C, 0);
    m.w8(O(m) + 0x7D, 0);
    m.w32(O(m) + 0x274, m.r32(O(m) + 4 * m.r32(sc(m) + 4) + 0x278));
    o2_set_scripts(ld_relocate_dir(0x8010ED18u), 2);
    o2_group_set(2, 0);
    m.w32(f.at(0x10), 0);
    o2_fade_set(1, 1, 0, 120);
    if (m.r8(sc(m) + 0x957) == 1) {
        o2_group_set(2, 64);
        const u32 s = sc(m);
        m.w32(O(m) + 0x50, 120);
        const u32 at = s + 4 * static_cast<u32>(m.r16s(s + 0x38C)) + 0xF3C;
        m.w32(at, m.r32(at) + static_cast<u32>(m.r16s(s + 0x958)));
        m.w32(s + 0x1034, m.r32(s + 0x1034) + static_cast<u32>(m.r16s(s + 0x958)));
    }
    if (m.r32(sc(m) + 0x96C) & 1u) {
        o2_group_set(2, 128);
        m.w32(O(m) + 0x50, 120);
    }
    return 0x4000000u;
}

u32 Game::o2_phase3() { return o2_result_screen(); }

u32 Game::o2_phase4() {
    cover(0x800FBF70u);
    Frame f(*this, 0x28);
    m.w32(O(m) + 0x274, m.r32(O(m) + 4 * m.r32(sc(m) + 4) + 0x278));
    m.w8(sc(m) + 0x957, 0);
    const u32 v = m.r32(sc(m) + 0x96C);
    m.w32(sc(m) + 0x96C, v & ~1u);
    if (m.r16(O(m) + 0x1C6)) {
        m.w16(O(m) + 0x88, 0);
        m.w16(O(m) + 0x8A, 0);
        return 0x4000000u;
    }
    if (v & 8u) return 0x4000000u;
    if (m.r8(sc(m) + 0x95F) == 1) return 0x4000000u;
    if (m.r8(sc(m) + 0x95E)) return 0x4000000u;
    if (m.r16s(sc(m) + 0x38C) == 59) return 0x4000000u;
    const u32 r = o2_front_loader_call(m.r32(0x80111730u + 4 * m.r8(O(m) + 0x7D)));
    const u32 ot = m.r32(O(m) + 4 * m.r32(sc(m) + 4));
    m.w32(O(m) + 8, ot);
    clear_otag_r(ot, 16);
    {
        const u32 o = O(m), s = sc(m);
        const u32 t = m.r32(o + 8), top = m.r32(s + 8);
        m.w32(t, (m.r32(t) & 0xFF000000u) | (m.r32(top) & 0x00FFFFFFu));
        m.w32(top, (m.r32(top) & 0xFF000000u) | ((t + 60) & 0x00FFFFFFu));
        const u32 p = o2_load_bar(m.r32(o + 0x274));
        m.w32(O(m) + 0x274, p);
        m.w32(O(m) + 0x274, o2_screens_frame(p));
    }
    if (!r) return 0;
    if (r == 1) {
        m.w8(O(m) + 0x7D, m.r8(O(m) + 0x7D) + 1);
        return 0;
    }
    if (r == 2) {
        m.w16(O(m) + 0x88, 0);
        m.w16(O(m) + 0x8A, 0);
        m.w16(O(m) + 0x1C6, 1);
        return 0x4000000u;
    }
    return 0;
}

u32 Game::o2_phase5() {
    cover(0x800FD634u);
    m.w8(O(m) + 0x7C, 0);
    m.w8(O(m) + 0x7D, 0);
    const u32 v = m.r32(sc(m) + 0x96C);
    if (v & 0x20u) m.w16(sc(m) + 0x38C, 59);
    else if (v & 8u) m.w16(O(m) + 0x1C6, 3);
    return 0x4000000u;
}

u32 Game::o2_phase6() {
    cover(0x800FD698u);
    Frame f(*this, 0x18);
    if (m.r16(O(m) + 0x1C6) && !(m.r32(sc(m) + 0x96C) & 8u)) return 0x4000000u;
    const u32 r = o2_front_loader_call(m.r32(0x80111710u + 4 * m.r8(O(m) + 0x7D)));
    if (!r) return 0;
    if (r == 1) {
        m.w8(O(m) + 0x7D, m.r8(O(m) + 0x7D) + 1);
        return 0;
    }
    if (r == 2) {
        m.w16(O(m) + 0x88, 0);
        m.w16(O(m) + 0x8A, 0);
        m.w16(O(m) + 0x1C6, 1);
        return 0x4000000u;
    }
    return 2;
}

u32 Game::o2_p7_buffer() {
    cover(0x800CD7C0u);
    m.w32(O(m) + 0x274, m.r32(O(m) + 4 * m.r32(sc(m) + 4) + 0x278));
    return 1;
}

u32 Game::o2_p7_setup() {
    cover(0x800CD7F0u);
    Frame f(*this, 0x20);
    {
        const u32 o = O(m);
        const u32 b = m.r32(o + 4 * m.r32(sc(m) + 4) + 0x278);
        m.w32(o + 0x4C, 0);
        m.w32(o + 0x270, 0);
        m.w32(o + 0x274, b);
    }
    m.w32(m.r32(g::kWorld) + 0x250, mem_alloc(0x100));
    for (u32 i = 0; i < 64; ++i) {
        const u32 b = mem_alloc(0x1C);
        m.w32(m.r32(m.r32(g::kWorld) + 0x250) + 4 * i, b);
    }
    const s32 mission = m.r16s(sc(m) + 0x38C);
    m.w32(sc(m) + 0x964, m.r32(sc(m) + 0x964) | 1u);
    o2_unlock_by_mission(mission);
    m.w32(O(m) + 0x94, 0);
    m.w16(m.r32(kHangar) + 0x6E, 0);
    set_geom_offset(160, 240);
    set_geom_screen(250);
    {
        const u32 o = O(m);
        m.w16(o + 0x30, 0);
        m.w16(o + 0x32, 0);
        m.w32(o + 0x144, 0);
        m.w32(o + 0x148, 1);
    }
    for (u32 k = 0; k < 5; ++k) m.w32(O(m) + 0x130 + 4 * k, sound_play(93 + k));
    for (u32 k = 0; k < 5; ++k) voice_level(m.r32(O(m) + 0x130 + 4 * k), 0, 1);
    m.w8(O(m) + 0x7D, 0);
    const u32 s = sc(m);
    if (m.r8(s + 0x95F) == 1) {
        m.w8(O(m) + 0x7C, 4);
        m.w8(O(m) + 0x7D, 2);
    } else if (m.r32(s + 0x96C) & 8u) {
        m.w8(O(m) + 0x7C, 1);
        m.w8(O(m) + 0x7D, 0);
    } else {
        const s32 st = m.r16s(s + 0x954);
        m.w8(O(m) + 0x7C, (st && st != 3) ? 1u : 0u);
    }
    {
        const u32 o = O(m);
        m.w8(o + 0x7E, 4);
        m.w8(o + 0x7F, 0);
        m.w8(o + 0x1B0, 0);
        m.w32(o + 0x11C, 0);
        m.w16(o + 0x34, 2);
        m.w8(o + 0x1B1, 0);
        m.w8(O(m) + 0x1B2, 0);
    }
    m.w32(sc(m) + 0x103C, 12);
    o2_text_font(1);
    o2_text_semi(-1);
    o2_text_fold(1, 1, 1);
    o2_text_frame_mode(1);
    m.w32(O(m) + 0x50, 0);
    const u32 s2 = sc(m);
    if (!(m.r32(s2 + 0x96C) & 8u)) {
        const s32 st = m.r16s(s2 + 0x954);
        if (!st || st == 3) return 1;
    }
    return 2;
}

u32 Game::o2_phase7() {
    cover(0x800CDCD4u);
    Frame f(*this, 0x18);
    const u32 fn = m.r32(0x8010982Cu + 4 * static_cast<u32>(m.r16s(O(m) + 0x8A)));
    u32 r;
    switch (fn) {
    case 0x800CD7C0u: r = o2_p7_buffer(); break;
    case 0x800CD7F0u: r = o2_p7_setup(); break;
    case 0x800CDAF4u: r = o2_hangar_backdrop_fade(); break;
    case 0x800CDBF0u: r = o2_hangar_mode_begin(); break;
    case 0x800CDC68u: r = o2_hangar_mode_active(); break;
    default: r = call_guest(fn, 0, 0); break;
    }
    if (r == 1) {
        m.w16(O(m) + 0x8A, m.r16(O(m) + 0x8A) + 1);
        return 0;
    }
    if (static_cast<s32>(r) < 2) return 0;
    if (r == 2) m.w16(O(m) + 0x1C6, 0);
    return 0x4000000u;
}

void Game::o2_unlock_by_mission(s32 mission) {
    cover(0x800FEDECu);
    for (u32 a = 0; a < 62; ++a)
        for (u32 k = 0; k < 4; ++k)
            if (static_cast<s32>(m.r8(0x80112228u + 4 * a + k)) == mission && !m.r8(sc(m) + a + 0xD48))
                m.w8(sc(m) + a + 0xD48, 1);
}

void Game::o2_unlock_by_id(s32 id) {
    cover(0x800FEE68u);
    for (u32 a = 0; a < 62; ++a)
        for (u32 k = 0; k < 8; ++k)
            if (m.r16s(0x80112330u + 16 * a + 2 * k) == id && !m.r8(sc(m) + a + 0xD48))
                m.w8(sc(m) + a + 0xD48, 1);
}

u32 Game::run_o2_phase(u32 fn) {
    switch (fn) {
    case 0x800F60F0u: return o2_phase0();
    case 0x800F6848u: return o2_phase1();
    case 0x800FBC48u: return o2_phase2();
    case 0x800FBD74u: return o2_phase3();
    case 0x800FBF70u: return o2_phase4();
    case 0x800FD634u: return o2_phase5();
    case 0x800FD698u: return o2_phase6();
    case 0x800CDCD4u: return o2_phase7();
    case 0x800CDF40u: return o2_phase8();
    default: unported(fn, "a phase of overlay 2"); return 0;
    }
}

u32 Game::o2_mission_saved(u32 mission) { return o2_campaign_milestones(mission); }
void Game::o2_shop_reset() { o2_mission_result_store(); }
}
