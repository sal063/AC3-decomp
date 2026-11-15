#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDisplayPtr = 0x800BCE48u;
constexpr u32 kDisplayState = 0x800BD220u;
constexpr u32 kSceneBlock = 0x800BF2D0u;
constexpr u32 kExitBlock = 0x800C039Cu;
constexpr u32 kPrimsPtr = 0x800BCE58u;
constexpr u32 kPrimsBlock = 0x800BD2A8u;
constexpr u32 kHeapFloor = 0x800CBFB8u;
constexpr u32 kByeBye = 0x80010000u;
constexpr u32 kDraw0 = 0x800BD0D0u, kDraw1 = 0x800BD12Cu, kDrawHi = 0x800BD188u;
constexpr u32 kDisp0 = 0x800BD1E4u, kDisp1 = 0x800BD1F8u, kDispHi = 0x800BD20Cu;
constexpr u32 kHiResFlag = 0x800BD21Cu;
constexpr u32 kPadsPtr = 0x800BCB84u;
constexpr u32 kPadLabel = 0x800BCB7Cu;
constexpr u32 kButtonsDigital = 0x800B01E0u, kButtonsAnalog = 0x800B0220u, kButtonsStick = 0x800B0260u,
              kButtonsNegcon = 0x800B02A0u;
constexpr u32 kAnalog2 = 0x800B02E0u, kAnalog4 = 0x800B02F8u, kAnalog8 = 0x800B0310u, kAnalog16 = 0x800B0328u;
constexpr u32 kKcd = 0x800BCBA8u, kReq = 0x800BCBBCu;
constexpr u32 kDebugText = 0x800BCB98u, kDebugColor = 0x800BCE68u;
constexpr u32 kCommonAttr = 0x800C5800u, kReverbAttr = 0x800C65F0u;
constexpr u32 kRcnt1 = 0xF2000001u;
}

void Game::flow_globals_init() {
    cover(0x800257E8u);
    Frame f(*this, 40);
    m.w32(kDisplayPtr, kDisplayState);
    m.w32(g::kScene, kSceneBlock);
    m.w32(g::kExit, kExitBlock);
    auto draw_env = [&](u32 env, s32 y, s32 h) {
        m.w32(f.at(0x10), static_cast<u32>(h));
        set_def_draw_env(env, 0, y, 320, h);
    };
    auto disp_env = [&](u32 env, s32 y, s32 h) {
        m.w32(f.at(0x10), static_cast<u32>(h));
        set_def_disp_env(env, 0, y, 320, h);
    };
    draw_env(kDraw0, 0, 240);
    disp_env(kDisp0, 256, 240);
    draw_env(kDraw1, 256, 240);
    disp_env(kDisp1, 0, 240);
    draw_env(kDrawHi, 0, 480);
    disp_env(kDispHi, 0, 480);
    m.w8(kHiResFlag, 1);
    for (u32 env : {kDraw0, kDraw1, kDrawHi}) {
        m.w8(env + 0x19, 4);
        m.w8(env + 0x1A, 56);
        m.w8(env + 0x1B, 72);
        m.w8(env + 0x18, 0);
    }
    m.w16(kDisp0 + 8, 0xFFFE);
    m.w16(kDisp0 + 10, 0);
    const u32 d = m.r32(kDisplayPtr);
    m.w32(d, 0);
    m.w32(d + 4, 0);
    m.w32(d + 8, static_cast<u32>(static_cast<s32>(m.r16s(kDisp0 + 8))));
    m.w32(d + 12, static_cast<u32>(static_cast<s32>(m.r16s(kDisp0 + 10))));
    m.w32(kPrimsPtr, kPrimsBlock);
    m.w8(d + 16, 0);
    m.w32(d + 20, 0);
    m.w8(d + 17, 0);
    m.w8(d + 18, 0);

    const u32 s = m.r32(g::kScene);
    m.w32(s, 0);
    m.w32(s + 4, 0);
    m.w32(s + 32, 0);
    m.w32(s + 68, 0);
    m.w32(s + 52, 0xFFFFFFFFu);
    m.w32(s + 56, 0xFFFFFFFFu);
    m.w32(s + 60, kByeBye);
    m.w32(s + 64, kHeapFloor);
    for (u32 off : {36u, 40u, 44u, 48u}) m.w32(s + off, 0);
    for (u32 off : {908u, 910u, 912u, 914u}) m.w16(s + off, 1);
    m.w32(s + 916, 1);
    m.w8(s + 920, 0xD0);
    m.w8(s + 921, 0xD0);
    m.w8(s + 922, 0);
    m.w8(s + 924, 1);
    for (u32 off : {925u, 926u, 927u, 932u, 933u, 934u, 936u}) m.w8(s + off, 0);
    m.w32(s + 928, 0);
    for (u32 off : {935u, 937u, 938u, 939u}) m.w8(s + off, 1);
    m.w16(s + 940, 0);
    m.w16(s + 942, 0);
    m.w32(s + 944, 0);
    m.w32(s + 948, 0);
    m.w16(s + 2388, 0);
    m.w8(s + 2398, 0);
    m.w32(s + 2412, 0);
    m.w32(s + 288, 0);
    for (u32 off : {344u, 345u, 346u, 347u}) m.w8(s + off, 0);
    m.w32(s + 72, 0);
    m.w32(s + 76, 0);
    for (u32 off = 4152; off <= 4212; off += 4)
        if (off != 4156 && off != 4160 && off != 4164) m.w32(s + off, 0);
    const u32 e = m.r32(g::kExit);
    m.w32(e + 112, 1);
    m.w32(e + 116, 0);
    mem_heap_reset(m.r32(s + 64));
}

void Game::copy_halfwords_32(u32 dst, u32 src) {
    cover(0x800287BCu);
    for (u32 i = 0; i < 32; ++i) m.w16(dst + 2 * i, m.r16(src + 2 * i));
}

u32 Game::copy_words_6(u32 dst, u32 src) {
    cover(0x800287E4u);
    const u32 a = m.r32(src), b = m.r32(src + 4), c = m.r32(src + 8), d = m.r32(src + 12);
    m.w32(dst, a);
    m.w32(dst + 4, b);
    m.w32(dst + 8, c);
    m.w32(dst + 12, d);
    const u32 e = m.r32(src + 16), f = m.r32(src + 20);
    m.w32(dst + 16, e);
    m.w32(dst + 20, f);
    m.w32(dst + 20, f);
    return e;
}

u32 Game::input_config_default(u32 kind, u32 cfg) {
    cover(0x80028848u);
    Frame f(*this, 24);
    m.w32(cfg + 88, 0);
    u32 buttons, analog;
    switch (kind) {
    case 1:
        copy_halfwords_32(cfg, kButtonsDigital);
        m.w16(cfg + 80, 0);
        m.w32(cfg + 88, m.r32(cfg + 88) | 0x18u);
        return m.r32(cfg + 88);
    case 2: buttons = kButtonsAnalog; analog = kAnalog2; break;
    case 4: buttons = kButtonsAnalog; analog = kAnalog4; break;
    case 8: buttons = kButtonsStick; analog = kAnalog8; break;
    case 16: buttons = kButtonsNegcon; analog = kAnalog16; break;
    default:
        copy_halfwords_32(cfg, kButtonsDigital);
        return 0;
    }
    copy_halfwords_32(cfg, buttons);
    copy_words_6(cfg + 64, analog);
    m.w32(cfg + 88, m.r32(cfg + 88) | 0x18u);
    return m.r32(cfg + 88);
}

void Game::input_configs_default(u32 cfgs) {
    cover(0x80028BC0u);
    Frame f(*this, 24);
    rumble_clear();
    if (!cfgs) return;
    for (u32 k = 0; k < 5; ++k) input_config_default(1u << k, cfgs + 92 * k);
}

u32 Game::pads_init(u32 bufs, u32 a, u32 b, u32 c) {
    cover(0x8002654Cu);
    Frame f(*this, 24);
    const u32 block = m.r32(kPadsPtr);
    for (u32 i = 0; i < 0xD0; ++i) m.w8(block + i, 0);
    const u32 pads = m.r32(kPadsPtr);
    m.w32(pads + 0xC8, a);
    m.w32(pads + 0xC4, b);
    m.w32(pads + 0xCC, c);
    for (u32 k = 0; k < 8; ++k) {
        const u32 port = m.r32(kPadsPtr) + 0x44 + 16 * k;
        m.w32(port + 4, bufs + 26 * k);
        m.w8(port, (k >= 4 ? 12u : 0u) + k);
    }
    pad_init_direct(m.r32(kPadsPtr), m.r32(kPadsPtr) + 0x22);
    pad_start_com_api();
    return hw->pad_enable_com(3);
}

void Game::hw_init() {
    cover(0x80025B00u);
    Frame f(*this, 32);
    for (u32 i = 0; i < 8; ++i) m.w8(f.at(0x10) + i, m.r8(kPadLabel + i));
    hw->reset_callback();
    set_video_mode(0);
    while (!hw->cd_init()) {}
    cd_set_debug(0);
    hw->vsync(3);
    hw->reset_graph(0);
    hw->set_disp_mask(1);
    init_geom(0x80025B78u);
    hw->spu_init();
    hw->init_card(0);
    hw->start_card();
    pads_init(m.r32(g::kScene) + 80, kGp, 2, 3);
    hw->exit_critical_section();
}

u32 Game::display_init() {
    cover(0x80025DDCu);
    Frame f(*this, 24);
    set_geom_offset(0, 0);
    set_geom_screen(277);
    m.w32(m.r32(kDisplayPtr), 11);
    display_env_update();
    hw->set_rcnt(kRcnt1, 7680, 0x2000);
    return hw->start_rcnt(kRcnt1);
}

u32 Game::volumes_init() {
    cover(0x80025E38u);
    Frame f(*this, 0x48);
    const u32 attr = f.at(0x10), mix = f.at(0x38);
    m.w32(attr, 0x3C3);
    m.w16(attr + 4, 0x3FFF);
    m.w16(attr + 6, 0x3FFF);
    m.w16(attr + 16, 0x7FFF);
    m.w16(attr + 18, 0x7FFF);
    m.w32(attr + 20, 1);
    m.w32(attr + 24, 1);
    hw->spu_set_common_attr(attr);
    m.w8(mix, 0xFF);
    m.w8(mix + 1, 0);
    m.w8(mix + 2, 0xFF);
    m.w8(mix + 3, 0);
    return hw->cd_mix(mix);
}

s32 Game::kcd_init(u32 clock) {
    cover(0x8002B034u);
    Frame f(*this, 24);
    const u32 k = m.r32(kKcd);
    m.w32(k + 0x20, clock);
    m.w32(k, 0);
    m.w32(k + 4, 0);
    m.w32(k + 0xC, 0);
    m.w32(k + 8, 0);
    m.w32(k + 0x24, 0);
    kcd_cd_volume(255, 0);
    for (;;) {
        const s32 r = kcd_read_index();
        if (r) return r;
        hw->vsync(0);
    }
}

void Game::kcd_requests_init() {
    cover(0x8002BCD8u);
    Frame f(*this, 24);
    kcd_init(m.r32(g::kScene) + 32);
    const u32 q = m.r32(kReq);
    m.w32(q + 0x164, 0xFF);
    m.w32(q, 0);
    m.w32(q + 4, 0);
    m.w32(q + 0x148, 0);
    m.w32(q + 0x160, 0);
    m.w32(q + 0x168, 1);
    m.w32(q + 0x16C, 0);
    m.w32(q + 0x178, 0);
}

void Game::debug_text_reset() {
    cover(0x800275C8u);
    m.w32(kDebugColor, 0x100010);
    const u32 b = m.r32(kDebugText);
    m.w32(b + 0xE14, 0x56140000u);
    m.w32(b + 0xE18, 0x56540000u);
    m.w32(b, 0);
    m.w32(b + 0xE1C, 0x56940000u);
    m.w32(b + 0xE20, 0);
}

void Game::boot_record_load() {
    cover(0x80025EA0u);
    Frame f(*this, 24);
    const u32 buf = mem_peek(ld_record_size(0));
    ld_request(0, buf, 0);
    cd_pending(0);
    const u32 dir = ld_relocate_dir(buf);
    snd_load_bank(m.r32(dir + 16));
    const u32 tims = ld_relocate_dir(m.r32(dir));
    ld_upload_tims(tims, m.r32s(m.r32(dir)));
    load_tim(m.r32(dir + 4));
    load_tim(m.r32(dir + 8));
    const u32 more = ld_relocate_dir(m.r32(dir + 12));
    ld_upload_tims(more, m.r32s(m.r32(dir + 12)));
    debug_text_reset();
}

void Game::voices_reset() {
    cover(0x8002E604u);
    for (u32 k = 0; k < 24; ++k) {
        const u32 v = g::kVoices + 20 * k;
        m.w16(v, 255);
        m.w8(v + 2, 0);
        m.w8(v + 3, 0);
        m.w8(v + 16, 0x80);
        m.w8(v + 17, 0x80);
        m.w16(v + 18, 0);
    }
}

void Game::snd_volumes_set(u32 cd_mix) {
    cover(0x8002D79Cu);
    Frame f(*this, 24);
    m.w32(kCommonAttr, 0x3C3);
    m.w16(kCommonAttr + 4, 0x3FFF);
    m.w16(kCommonAttr + 6, 0x3FFF);
    m.w32(kCommonAttr + 24, cd_mix ? 1u : 0u);
    m.w32(kCommonAttr + 20, 0);
    const u32 vol = m.r8(m.r32(g::kScene) + 921) >> 1;
    m.w16(kCommonAttr + 18, vol);
    m.w16(kCommonAttr + 16, vol);
    hw->spu_set_common_attr(kCommonAttr);
}

void Game::snd_cd_mix(u32 on) {
    cover(0x8002D880u);
    Frame f(*this, 24);
    m.w32(kCommonAttr, 0x200);
    m.w32(kCommonAttr + 24, on ? 1u : 0u);
    hw->spu_set_common_attr(kCommonAttr);
}

u32 Game::reverb_setup() {
    cover(0x8002DA20u);
    Frame f(*this, 24);
    m.w32(kReverbAttr, 1);
    m.w32(kReverbAttr + 4, 3);
    m.w16(kReverbAttr + 8, 6553);
    m.w16(kReverbAttr + 10, 6553);
    return hw->spu_set_reverb_mode_param(kReverbAttr);
}

void Game::sound_init() {
    cover(0x8002D75Cu);
    Frame f(*this, 24);
    voices_reset();
    snd_volumes_set(0);
    snd_delayed_reset();
    reverb_setup();
    sound_queue_reset();
}

void Game::flow_init() {
    cover(0x80025F68u);
    Frame f(*this, 24);
    flow_globals_init();
    input_configs_default(m.r32(g::kScene) + 348);
    const u32 s = m.r32(g::kScene);
    m.w32(s + 288, m.r32(s + 288) | 1u);
    hw_init();
    display_init();
    volumes_init();
    kcd_requests_init();
    boot_record_load();
    sound_init();
    kcd_voice_reset();
    snd_cd_mix(1);
}
}
