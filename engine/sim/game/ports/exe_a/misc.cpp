#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDebugText = 0x800BCB98u;
constexpr u32 kDebugCursor = 0x800BCE68u;
constexpr u32 kKcd = 0x800BCBA8u;
constexpr u32 kStrm = 0x800BCBACu;
constexpr u32 kRingState = 0x800BCBB4u;
constexpr u32 kReq = 0x800BCBBCu;
constexpr u32 kCommonAttr = 0x800C5800u;
constexpr u32 kEngineCount = 0x800C6610u;
constexpr u32 kEar = 0x800C6620u;
constexpr u32 kEngines = 0x800C6630u;
}

void Game::main_nothing() { cover(0x80025724u); }

void Game::pads_stop() {
    cover(0x80027064u);
    Frame f(*this, 24);
    pad_stop_com_api();
}

void Game::flow_shutdown() {
    cover(0x80025FE8u);
    Frame f(*this, 24);
    hw->vsync_callback(0);
    pads_stop();
    hw->stop_card();
    hw->reset_graph(3);
}

u32 Game::bytes3_differ(u32 a, u32 b) {
    cover(0x800267A8u);
    if (m.r8(a) != m.r8(b)) return 1;
    if (m.r8(a + 1) != m.r8(b + 1)) return 1;
    return m.r8(a + 2) != m.r8(b + 2) ? 1u : 0u;
}

u32 Game::record_checksum(u32 data, u32 size, u32 expected) {
    cover(0x800273BCu);
    u32 sum = 0;
    u32 p = data + 4;
    for (u32 left = size - 4; left != 0; left -= 2, p += 2) sum += m.r8(p) + 0x10001u + left;
    return sum - expected;
}

u32 Game::mem_low_check(u32 n) {
    cover(0x800274E0u);
    Frame f(*this, 24);
    const u32 sc = m.r32(g::kScene);
    if (m.r32(sc + 28) + (n & ~3u) < m.r32(sc + 24)) return 0;
    return m.r32(sc + 28);
}

void Game::debug_font_page(u32 page) {
    cover(0x800275FCu);
    m.w32(m.r32(kDebugText) + 0xE20, page);
}

namespace {
u32 tab_advance(s32 printed) { return static_cast<u32>(24 * ((printed / 6 + 4) >> 2)); }
}

u32 Game::debug_print_at(u32 x, u32 y, u32 fmt, u32 first) {
    cover(0x80027990u);
    m.w32(sp + 8, fmt);
    m.w32(sp + 12, first);
    const u32 args = sp + 12;
    Frame f(*this, 0x230);
    const u32 buf = f.at(0x10);
    const s32 n = static_cast<s32>(vsprintf(buf, fmt, args));
    u32 start = 0, px = x, py = y;
    for (s32 i = 0; i < n; ++i) {
        const u32 at = buf + static_cast<u32>(i);
        const u32 c = m.r8(at);
        if (c == '\t') {
            m.w8(at, 0);
            const u32 end = debug_text(px, py, buf + start);
            start = static_cast<u32>(i) + 1;
            const s32 cells = static_cast<s16>(static_cast<s16>(end - px) / 6);
            px += static_cast<u32>(24 * ((cells + 4) >> 2));
        } else if (c == '\n') {
            m.w8(at, 0);
            debug_text(px, py, buf + start);
            start = static_cast<u32>(i) + 1;
            py += 6;
            px = static_cast<u32>(static_cast<s32>(static_cast<s16>(x)));
        }
    }
    const u32 end = debug_text(px, py, buf + start);
    return (end & 0xFFFFu) | (py << 16);
}

u32 Game::debug_print(u32 fmt, u32 a1, u32 a2, u32 a3) {
    cover(0x80027AE0u);
    m.w32(sp, fmt);
    m.w32(sp + 4, a1);
    m.w32(sp + 8, a2);
    m.w32(sp + 12, a3);
    const u32 args = sp + 4;
    Frame f(*this, 0x228);
    const u32 buf = f.at(0x10);
    const u32 cursor = m.r32(kDebugCursor);
    u32 px = cursor & 0xFFFFu, py = cursor >> 16;
    const s32 n = static_cast<s32>(vsprintf(buf, fmt, args));
    u32 start = 0;
    for (s32 i = 0; i < n; ++i) {
        const u32 at = buf + static_cast<u32>(i);
        const u32 c = m.r8(at);
        if (c == '\t') {
            m.w8(at, 0);
            const u32 end = debug_text(px, py, buf + start);
            start = static_cast<u32>(i) + 1;
            px += tab_advance(static_cast<s32>((end & 0xFFFFu) - px));
        } else if (c == '\n') {
            m.w8(at, 0);
            debug_text(px, py, buf + start);
            start = static_cast<u32>(i) + 1;
            py += 6;
            px = 16;
        }
    }
    const u32 end = debug_text(px, py, buf + start);
    const u32 pen = (end & 0xFFFFu) | (py << 16);
    m.w32(kDebugCursor, pen);
    return pen;
}

void Game::load_tim_at(u32 tim, u32 px, u32 py, u32 cx, u32 cy) {
    cover(0x80027084u);
    Frame f(*this, 0x48);
    const u32 img = f.at(0x10), rect = f.at(0x28);
    open_tim(tim);
    read_tim(img);
    auto upload = [&](u32 rect_at, u32 data_at, u32 x, u32 y) {
        if (!m.r32(data_at)) return;
        const u32 src = m.r32(rect_at);
        for (u32 k = 0; k < 8; ++k) m.w8(rect + k, m.r8(src + k));
        m.w16(rect, x);
        m.w16(rect + 2, y);
        m.w32(rect_at, rect);
        hw->load_image(rect, m.r32(data_at));
    };
    upload(img + 4, img + 8, cx, cy);
    upload(img + 12, img + 16, px, py);
}

void Game::hud_alert_burning_hard() {
    cover(0x800296F0u);
    Frame f(*this, 24);
    const u32 p = hud_alert_take(161);
    if (!p) return;
    m.w8(p, 1);
    m.w8(p + 2, static_cast<u32>((rand() & 0x3F) + 97));
}

u32 Game::kcd_lid_closed() {
    cover(0x8002B46Cu);
    Frame f(*this, 24);
    const u32 k = m.r32(kKcd);
    if (m.r8(k + 0x28) & 0x10u) return 0;
    if (m.r32(k + 0xC) - 1u >= 2u) return 0;
    if (m.r32(k) != 2 || m.r32(k + 4) != 2) return 0;
    if (!hw->cd_control(7, 0, k + 0x28)) return 0;
    const u32 k2 = m.r32(kKcd);
    m.w32(k2, 0);
    m.w32(k2 + 4, 0);
    return 1;
}

u32 Game::kcd_ring_release(u32 rec) {
    cover(0x8002B718u);
    const u32 step = m.r32(m.r32(kRingState));
    const s32 n = static_cast<s32>(m.r8(rec + 2));
    for (s32 i = 0; i < n;) {
        if (m.r8(rec) != 2) return 1;
        ++i;
        m.w8(rec, 0);
        rec += step;
    }
    return 0;
}

u32 Game::stream_ring_mark() {
    cover(0x8002BCA4u);
    if (m.r32(m.r32(kStrm) + 0x20) != 0x40) return 0;
    return m.r32(m.r32(kRingState) + 0x34);
}

void Game::req_stereo_set(u32 v) {
    cover(0x8002BD28u);
    m.w32(m.r32(kReq) + 0x160, v);
}
u32 Game::req_stereo() {
    cover(0x8002BD88u);
    return m.r32(m.r32(kReq) + 0x160);
}

void Game::req_volume_set(s32 v) {
    cover(0x8002BD34u);
    m.w32(m.r32(kReq) + 0x164, static_cast<u32>(v < 0 ? 0 : v >= 256 ? 255 : v));
}
u32 Game::req_volume() {
    cover(0x8002BD9Cu);
    return m.r32(m.r32(kReq) + 0x164);
}

void Game::req_volume_apply() {
    cover(0x8002BD5Cu);
    Frame f(*this, 24);
    const u32 q = m.r32(kReq);
    kcd_cd_volume(m.r32s(q + 0x164), m.r32(q + 0x160));
}

u32 Game::stream_kind(u32 id) {
    cover(0x8002CBA4u);
    Frame f(*this, 24);
    return (m.r32(kcd_entry(id)) >> 29) & 3u;
}

u32 Game::snd_nothing_a() {
    cover(0x8002D6E0u);
    return 0;
}
u32 Game::snd_nothing_b() {
    cover(0x800304ECu);
    return 0;
}

void Game::spu_memory_clear() {
    cover(0x8002D6E8u);
    Frame f(*this, 24);
    hw->spu_init();
    hw->spu_set_transfer_start_addr(0x1010);
    hw->spu_write0(0x7EFF0);
}

void Game::spu_on() {
    cover(0x8002D71Cu);
    Frame f(*this, 24);
    hw->spu_start();
}
void Game::spu_off() {
    cover(0x8002D73Cu);
    Frame f(*this, 24);
    hw->spu_quit();
}

void Game::snd_cd_reverb(u32 on) {
    cover(0x8002D8C8u);
    Frame f(*this, 24);
    m.w32(kCommonAttr, 0x100);
    m.w32(kCommonAttr + 20, on ? 1u : 0u);
    hw->spu_set_common_attr(kCommonAttr);
}

void Game::reverb_none() {
    cover(0x8002DA60u);
    Frame f(*this, 0x30);
    const u32 attr = f.at(0x10);
    m.w32(attr, 1);
    m.w32(attr + 4, 0);
    hw->spu_set_reverb_mode_param(attr);
    hw->spu_set_reverb(0);
}

void Game::engine_set_word(u32 k, u32 v) {
    cover(0x8002F6E4u);
    m.w32(kEngines + 92 * k + 0x24, v);
}

u32 Game::sound_scene_reset() {
    cover(0x800304F4u);
    Frame f(*this, 24);
    alert_channels_open(2);
    m.w32(kEar + 8, 0);
    m.w32(kEar + 4, 0);
    m.w32(kEar, 0);
    return 0;
}

u32 Game::sound_scene_stop() {
    cover(0x80030750u);
    Frame f(*this, 24);
    voices_off();
    alert_channels_stop();
    return 0;
}

u32 Game::sound_scene(u32 rec) {
    cover(0x80030528u);
    Frame f(*this, 0x70);
    const u32 ear = f.at(0x10), pos = f.at(0x28), cand = f.at(0x38), chosen = f.at(0x48);
    for (u32 k = 0; k < 3; ++k) m.w32(ear + 4 * k, m.r32(rec + 12 + 4 * k) << 12);
    for (u32 k = 0; k < 3; ++k) m.w16(ear + 16 + 2 * k, m.r16(rec + 4 + 2 * k));
    const u32 list = rec + 24;
    distance(ear, kEar);
    for (u32 k = 0; k < 4; ++k) m.w32(kEar + 4 * k, m.r32(ear + 4 * k));
    for (s32 i = 0; i < m.r32s(kEngineCount); ++i)
        m.w32(cand + 4 * static_cast<u32>(i), static_cast<u32>(static_cast<s32>(m.r16s(list + 24 * static_cast<u32>(i)))));
    engine_assign(cand, chosen);
    for (s32 i = 0; i < m.r32s(kEngineCount); ++i) {
        const u32 e = list + 24 * static_cast<u32>(i);
        const u32 k = m.r32(chosen + 4 * static_cast<u32>(i));
        const u32 ch = kEngines + 92 * k;
        m.w32(ch + 12, m.r32(e + 8) << 12);
        m.w32(ch + 16, m.r32(e + 12) << 12);
        m.w32(ch + 20, m.r32(e + 16) << 12);
        engine_set_word(k, m.r32(e + 20));
        m.w16(ch + 28, m.r16(e + 2));
        m.w16(ch + 30, m.r16(e + 4));
        m.w16(ch + 32, m.r16(e + 6));
    }
    engine_mix(ear);
    const u32 shot = list + 96;
    while (m.r16s(shot) != -1) {
        for (u32 k = 0; k < 3; ++k) m.w32(pos + 4 * k, m.r32(shot + 8 + 4 * k) << 12);
        sound_at(static_cast<u32>(static_cast<s32>(m.r16s(shot))), pos, 4);
    }
    return 0;
}

u32 Game::height_curve(s32 y) {
    cover(0x80031E08u);
    s32 h = static_cast<s32>(0u - static_cast<u32>(y)) >> 12;
    if (h < 0) h = 0;
    if (h > 0xD6D8) h = 0xD6D8;
    const s32 t = h * 0x7899;
    const s32 hi = static_cast<s32>((static_cast<s64>(t) * static_cast<s32>(0x98852841u)) >> 32);
    const s32 q = ((hi + t) >> 15) - (t >> 31);
    return static_cast<u32>(0x38B73 - q);
}

void Game::phase_hook_nothing(u32 addr) { cover(addr); }
}
