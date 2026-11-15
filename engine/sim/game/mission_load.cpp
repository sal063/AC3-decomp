#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kLoadScreen = 0x800BCBE0u;
constexpr u32 kDisplay = 0x800BCE48u;
constexpr u32 kMissionRecords = 0x800B0B08u;
constexpr u32 kBriefingRecords = 0x800B0B84u;
constexpr u32 kMissionKinds = 0x800B0C64u;
constexpr u32 kDemoMissions = 0x800B0BD0u;
constexpr u32 kVoiceTable = 0x800C5A10u;
constexpr u32 kReverbMode = 0x800C65F4u;
constexpr u32 kTimCursor = 0x800CBC58u;
constexpr u32 kFadeSource = 0x800BCDCCu;
constexpr u32 kHeapTop = 0x801FB700u;
constexpr u32 kRecordsLoaded = 0x800357B4u;
}

u32 Game::request_mission() {
    cover(0x800358F8u);
    Frame f(*this, 0x20);
    cd_reset(0, 0);
    reverb_off();
    if (m.r8(m.r32(g::kScene) + 0x3A5)) demo_next_mission();
    m.w8(m.r32(g::kScene) + 0x957, 0);
    const u32 sc = m.r32(g::kScene);
    const s32 msn = m.r16s(sc + 0x38C);
    m.w16(sc + 0x958, 0);
    m.w16(sc + 0x3AC, 0);
    const u32 x = m.r32(g::kExit);
    m.w32(x + 0x60, static_cast<u32>(m.r16s(kMissionRecords + (static_cast<u32>(msn) << 1))));
    const u32 at = mem_alloc(records_size(x + 0x60, 1));
    m.w32(m.r32(kLoadScreen) + 8, 1);
    request_records(m.r32(g::kExit) + 0x60, 1, at, kRecordsLoaded);
    const u32 d = m.r32(kDisplay);
    const u32 flags = m.r32(d);
    m.w32(d + 4, 0);
    m.w32(d + 0x14, 0);
    m.w32(d, flags | 1u);
    const u32 l = m.r32(kLoadScreen);
    m.w16(l, 0);
    m.w8(l + 4, m.r8(kMissionKinds + static_cast<u32>(msn)));
    if (m.r8(m.r32(g::kScene) + 0x3A5)) m.w8(m.r32(kLoadScreen) + 4, 1);
    m.w8(m.r32(kLoadScreen) + 6, 0);
    m.w16(m.r32(kLoadScreen) + 2, 0);
    loading_frame();
    return 0x04000000u;
}

u32 Game::unpack_mission() {
    cover(0x80035A38u);
    Frame f(*this, 0x28);
    const u32 x = m.r32(g::kExit);
    const u32 d = ld_relocate_dir(m.r32(x + 0x60));
    snd_load_bank(m.r32(d + 0x40));
    for (u32 k = 0x3C; k >= 0x2C; k -= 4) ld_texture_set(m.r32(d + k), m.r32(d + k + 4));
    const s32 n = static_cast<s32>(m.r32(d + 0x2C) - d) >> 2;
    const u32 top = kHeapTop - (static_cast<u32>(n) << 2);
    const u32 delta = static_cast<u32>(static_cast<s32>(top - d) >> 2) << 2;
    word_copy(top, d, static_cast<u32>(n));
    for (u32 i = 0; i < 11; ++i) m.w32(top + 4 * i, m.r32(top + 4 * i) + delta);
    u32 at = d + 0x44;
    m.w32(d, at);
    for (u32 i = 0; i < 11; ++i) {
        at = ulz_decode(m.r32(top + 4 * i), at);
        m.w32(d + 4 + 4 * i, at);
    }
    m.w32(x + 0x18, m.r32(d));
    const u32 sc = m.r32(g::kScene);
    m.w32(sc + 0x103C, m.r32(m.r32(d + 0xC)));
    m.w32(sc + 0x1044, m.r32(m.r32(d + 0x14)));
    const u32 v = x + 0x18;
    m.w32(v + 4, m.r32(d + 4));
    m.w32(v + 0x20, m.r32(d + 0x20));
    m.w32(v + 0x18, m.r32(d + 0x18));
    m.w32(v + 0x1C, m.r32(d + 0x1C));
    m.w32(v + 8, ld_relocate_dir(m.r32(d + 8)) - 4);
    m.w32(v + 0xC, ld_relocate_dir(m.r32(d + 0xC)));
    m.w32(v + 0x10, ld_relocate_dir(m.r32(d + 0x10)));
    m.w32(v + 0x14, ld_relocate_dir(m.r32(d + 0x14)));
    m.w32(v + 0x24, ld_relocate_dir(m.r32(d + 0x24)));
    m.w32(v + 0x28, ld_relocate_dir(m.r32(d + 0x28)));
    auto relocate_all = [&](u32 list, u32 count, s32 from, u32 words) {
        for (s32 i = from; i < m.r32s(m.r32(d + count)) + from; ++i) {
            const u32 a = m.r32(m.r32(v + list) + 4u * static_cast<u32>(i));
            ld_relocate_words(a, a + 4, static_cast<s32>(words));
        }
    };
    relocate_all(8, 8, 1, 4);
    relocate_all(0xC, 0xC, 0, 6);
    relocate_all(0x10, 0x10, 0, 6);
    relocate_all(0x14, 0x14, 0, 4);
    for (s32 i = 0; i < m.r32s(m.r32(d + 0x10)); ++i) {
        const u32 o = 4u * static_cast<u32>(i);
        m.w32(m.r32(v + 0xC) + o + 0x180, m.r32(m.r32(v + 0x10) + o));
    }
    const u32 b = m.r32(v);
    palette_fades(b + 8, b + 0x78, m.r32s(b + 0x74));
    heap_set(m.r32(d + 0x2C));
    loading_frame();
    return 0x04000000u;
}

u32 Game::request_briefing() {
    cover(0x80035F18u);
    Frame f(*this, 0x18);
    const u32 x = m.r32(g::kExit);
    const s32 id = m.r16s(kBriefingRecords + (static_cast<u32>(m.r16s(m.r32(g::kScene) + 0x38E)) << 1));
    m.w32(x + 0x60, static_cast<u32>(id));
    const u32 at = mem_alloc(records_size(x + 0x60, 1));
    m.w32(m.r32(kLoadScreen) + 8, 1);
    request_records(m.r32(g::kExit) + 0x60, 1, at, kRecordsLoaded);
    loading_frame();
    return 0x04000000u;
}

u32 Game::unpack_briefing() {
    cover(0x80035F98u);
    Frame f(*this, 0x28);
    const u32 x = m.r32(g::kExit);
    const u32 d = ld_relocate_dir(m.r32(x + 0x60));
    snd_load_bank(m.r32(d + 0xC));
    ld_texture_set(m.r32(d + 8), m.r32(d + 0xC));
    const s32 n = static_cast<s32>(m.r32(d + 8) - d) >> 2;
    const u32 top = kHeapTop - (static_cast<u32>(n) << 2);
    const u32 delta = static_cast<u32>(static_cast<s32>(top - d) >> 2) << 2;
    word_copy(top, d, static_cast<u32>(n));
    m.w32(top, m.r32(top) + delta);
    m.w32(top + 4, m.r32(top + 4) + delta);
    m.w32(d, d + 0x10);
    u32 at = ulz_decode(m.r32(top), d + 0x10);
    m.w32(d + 4, at);
    at = ulz_decode(m.r32(top + 4), at);
    m.w32(d + 8, at);
    m.w32(x + 8, ld_relocate_dir(m.r32(d)));
    const u32 v = x + 8;
    for (s32 i = 0; i < m.r32s(m.r32(d)); ++i) {
        const u32 a = m.r32(m.r32(v) + 4u * static_cast<u32>(i));
        ld_relocate_words(a, a + 4, 6);
    }
    m.w32(m.r32(g::kScene) + 0x1040, m.r32(m.r32(d)));
    m.w32(v + 4, m.r32(d + 4));
    heap_set(m.r32(d + 8));
    m.w32(m.r32(g::kExit) + 0x5C, mem_peek(0));
    loading_frame();
    const u32 dp = m.r32(kDisplay);
    const u32 flags = m.r32(dp);
    m.w32(dp + 4, 0);
    m.w32(dp + 0x14, 0);
    m.w32(dp, flags | 5u);
    return 0x04000000u;
}

void Game::demo_next_mission() {
    cover(0x80036130u);
    const u32 sc = m.r32(g::kScene);
    m.w8(sc + 0x3A6, m.r8(sc + 0x3A6) + 1);
    m.w8(sc + 0x3A6, m.r8(sc + 0x3A6) % 37u);
    const u32 e = kDemoMissions + 4 * m.r8(sc + 0x3A6);
    m.w16(sc + 0x38C, m.r8(e + 2));
    m.w16(sc + 0x38E, m.r8(e + 3));
}

u32 Game::records_size(u32 list, s32 n) {
    cover(0x8002C63Cu);
    Frame f(*this, 0x28);
    u32 total = 0;
    for (s32 i = 0; i < n; ++i) total += ld_record_size(m.r32(list + 4u * static_cast<u32>(i)));
    return total;
}

u32 Game::request_records(u32 list, s32 n, u32 at, u32 cb) {
    cover(0x8002C82Cu);
    Frame f(*this, 0x28);
    for (s32 i = 0; i < n - 1; ++i, list += 4) {
        const u32 id = m.r32(list);
        m.w32(list, at);
        at = ld_request(id, at, 0);
    }
    const u32 id = m.r32(list);
    m.w32(list, at);
    return ld_request(id, at, cb);
}

u32 Game::ld_relocate_dir(u32 dir) {
    cover(0x80027338u);
    const u32 n = m.r16(dir);
    for (u32 i = 0; i < n; ++i) {
        const u32 e = dir + 4 + 4 * i;
        const u32 w = m.r32(e);
        if (!(w & 0x80000000u)) m.w32(e, w + dir);
    }
    return dir + 4;
}

u32 Game::ld_relocate_words(u32 base, u32 p, s32 n) {
    cover(0x80027380u);
    for (s32 i = 0; i < n; ++i, p += 4) {
        const u32 w = m.r32(p);
        if (!(w & 0x80000000u)) m.w32(p, w + base);
    }
    return base;
}

u32 Game::ld_texture_set(u32 src, u32 dst) {
    cover(0x800272F0u);
    Frame f(*this, 0x20);
    const u32 end = ulz_decode(src, dst);
    ld_upload_tims(ld_relocate_dir(dst), m.r32s(dst));
    return end;
}

void Game::ld_upload_tims(u32 list, s32 n) {
    cover(0x8002722Cu);
    Frame f(*this, 0x38);
    const u32 img = f.at(0x10);
    for (s32 i = 0; i < n; ++i, list += 4) {
        open_tim(m.r32(list));
        if (!read_tim(img)) unported(0x800ADFB8u, "ld_upload_tims' \"Can't read TIM\"");
        if (m.r32(img + 0x10)) hw->load_image(m.r32(img + 0xC), m.r32(img + 0x10));
        if (m.r32(img + 8)) hw->load_image(m.r32(img + 4), m.r32(img + 8));
    }
}

u32 Game::open_tim(u32 tim) {
    cover(0x800AAD48u);
    m.w32(kTimCursor, tim);
    return 0;
}

u32 Game::read_tim(u32 img) {
    cover(0x800AAD58u);
    Frame f(*this, 0x18);
    const s32 n = tim_parse(m.r32(kTimCursor), img);
    if (n == -1) return 0;
    m.w32(kTimCursor, m.r32(kTimCursor) + (static_cast<u32>(n) << 2));
    return img;
}

s32 Game::tim_parse(u32 tim, u32 img) {
    cover(0x800AB0A4u);
    Frame f(*this, 0x20);
    if (m.r32(tim) != 0x10) return -1;
    m.w32(img, m.r32(tim + 4));
    u32 p = tim + 8;
    for (int k = 0; k < 3; ++k) get_graph_debug();
    u32 clut = 0;
    if (m.r32(img) & 8u) {
        const u32 bytes = m.r32(p);
        m.w32(img + 4, p + 4);
        m.w32(img + 8, p + 0xC);
        clut = bytes >> 2;
        p += clut << 2;
    } else {
        m.w32(img + 4, 0);
        m.w32(img + 8, 0);
    }
    const u32 bytes = m.r32(p);
    m.w32(img + 0xC, p + 4);
    m.w32(img + 0x10, p + 0xC);
    return static_cast<s32>(clut + (bytes >> 2) + 2);
}

u32 Game::ulz_decode(u32 src, u32 dst) {
    cover(0x800AFD08u);
    const u32 type = m.r8(src + 7), bits = m.r8(src + 11);
    u32 flags = src + 16;
    u32 lit = src + (m.r32(src + 8) & 0xFFFFFFu);
    u32 ref = src + (m.r32(src + 12) & 0xFFFFFFu);
    u32 out = dst;
    if (type == 0 || type == 1) {
        const u32 b = (type == 0 ? bits : bits - 1u) & 31u;
        const u32 mask = (1u << b) - 1u;
        for (;;) {
            u32 w = m.r32(flags);
            flags += 4;
            if (!w) return out;
            for (;;) {
                const bool literal = (w & 0x80000000u) != 0;
                w <<= 1;
                if (!w) break;
                const u32 e = literal ? 0u : m.r16(ref);
                if (type == 0) {
                    if (literal) {
                        m.w8(out++, m.r8(lit++));
                        continue;
                    }
                    ref += 2;
                    u32 from = out - ((e & mask) + 1u);
                    for (u32 k = (e >> b) + 3u; k; --k) m.w8(out++, m.r8(from++));
                } else {
                    if (literal) {
                        m.w16(out, m.r16(lit));
                        lit += 2;
                        out += 2;
                        continue;
                    }
                    ref += 2;
                    u32 from = out - (((e & mask) << 1) + 2u);
                    for (u32 k = (e >> b) + 2u; k; --k, out += 2, from += 2) m.w16(out, m.r16(from));
                }
            }
        }
    }
    if (type != 2) {
        unported(0x800AFD08u, "an ULZ of type 3");
        return out;
    }
    const u32 b = bits & 31u;
    const u32 mask = (1u << b) - 1u;
    s32 left = static_cast<s32>(m.r32(src + 4) & 0xFFFFFFu);
    s32 nbits = 0x20, have = -1;
    u32 w = m.r32(flags), word = 0;
    flags += 4;
    if (left <= 0) return out;
    for (;;) {
        if (--nbits < 0) {
            nbits = 0x1F;
            w = m.r32(flags);
            flags += 4;
        }
        const bool literal = static_cast<s32>(w) < 0;
        w <<= 1;
        if (literal) {
            --left;
            if (have > 0) {
                --have;
            } else {
                word = m.r32(lit);
                lit += 4;
                have = 3;
            }
            m.w8(out++, word & 0xFFu);
            word >>= 8;
        } else {
            const u32 e = m.r16(ref);
            ref += 2;
            u32 from = out - ((e & mask) + 1u);
            const s32 len = static_cast<s32>(e >> b) + 3;
            left -= len;
            for (s32 k = len; k > 0; --k) m.w8(out++, m.r8(from++));
        }
        if (left <= 0) return out;
    }
}

void Game::snd_load_bank(u32 b) {
    cover(0x8002D96Cu);
    Frame f(*this, 0x18);
    for (u32 i = 0; i < m.r8(b + 1); ++i) {
        const u32 to = kVoiceTable + 20 * (m.r8(b) + i), from = b + 0xC + 20 * i;
        for (u32 k = 0; k < 20; k += 4) m.w32(to + k, m.r32(from + k));
    }
    snd_spu_upload(m.r32(b + 4), b + (static_cast<u32>(m.r16s(b + 2)) << 2), m.r32(b + 8));
}

void Game::snd_spu_upload(u32 addr, u32 src, u32 size) {
    cover(0x8002D910u);
    Frame f(*this, 0x20);
    hw->spu_set_transfer_mode(1);
    hw->spu_set_transfer_start_addr(addr);
    hw->spu_write(src, size);
    hw->spu_is_transfer_completed(1);
}

void Game::reverb_off() {
    cover(0x8002DAF4u);
    Frame f(*this, 0x30);
    const u32 attr = f.at(0x10);
    m.w32(attr, 6);
    m.w16(attr + 8, 0);
    m.w16(attr + 0xA, 0);
    hw->spu_set_reverb_depth(attr);
    hw->spu_set_reverb(0);
    hw->spu_set_reverb_voice(0, 0xFFFFFF);
    hw->spu_clear_reverb_work_area(m.r32(kReverbMode));
}

void Game::palette_fades(u32 color, u32 list, s32 n) {
    cover(0x8005EBB8u);
    Frame f(*this, 0x1050);
    m.w32(f.at(0x1050), color);
    m.w32(f.at(0x1058), static_cast<u32>(n));
    const u32 rect = f.at(0x1018), buf = f.at(0x18), out = f.at(0x818), r = f.at(0x1020);
    m.w32(rect, m.r32(kFadeSource));
    m.w32(rect + 4, m.r32(kFadeSource + 4));
    hw->store_image(rect, buf);
    s32 ex = 0x140, ey = 0x180, ox = 0, oy = 0xF0;
    for (u32 i = 0; i < 16; ++i) {
        const s32 t = rsin(static_cast<s32>(i << 6));
        m.w32(f.at(0x10), static_cast<u32>(t));
        palette_fade(out, buf, 0x400, m.r32(f.at(0x1050)), static_cast<u32>(t));
        if (i & 1u) {
            m.w16(r, static_cast<u32>(ox));
            ox += 64;
            m.w16(r + 2, static_cast<u32>(oy));
            m.w16(r + 4, 16);
            m.w16(r + 6, 16);
            if (ox >= 256) {
                ox = 0;
                oy = 0x1F0;
            }
            hw->load_image(r, out);
            for (u32 k = 1; k < 4; ++k) {
                m.w16(r, m.r16(r) + 16);
                hw->load_image(r, out + 0x200 * k);
            }
        } else {
            m.w16(r, static_cast<u32>(ex));
            ex += 16;
            m.w16(r + 6, 64);
            m.w16(r + 2, static_cast<u32>(ey));
            m.w16(r + 4, 16);
            if (ex >= 384) {
                ex = 0x140;
                ey += 64;
            }
            hw->load_image(r, out);
        }
    }
    for (s32 j = 0; j < m.r32s(f.at(0x1058)); ++j, list += 12)
        m.w16(list + 6, m.r16(buf + (static_cast<u32>(m.r16s(list)) << 1)));
}

void Game::palette_fade(u32 out, u32 in, s32 n, u32 color, u32 t) {
    cover(0x8005EB20u);
    gte.ctc2(21, m.r8(color) << 4);
    gte.ctc2(22, m.r8(color + 1) << 4);
    gte.ctc2(23, m.r8(color + 2) << 4);
    gte.mtc2(8, t);
    for (s32 i = 0; i < n; ++i, in += 2, out += 2) {
        const u32 c = m.r16(in);
        u32 o = 0;
        if (c) {
            gte.mtc2(28, c);
            gte.op(0x980011);
            o = gte.mfc2(29);
            if (c & 0x8000u) o |= 0x8000u;
        }
        m.w16(out, o);
    }
}
}
