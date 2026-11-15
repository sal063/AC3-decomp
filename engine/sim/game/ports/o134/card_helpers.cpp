#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kCards = 0x800DDA48u, kScreens = 0x800DD9DCu;
constexpr u32 kResourceBuffers = 0x800DDA18u, kResourceTop = 0x800DDA20u;
constexpr u32 kResourceBase = 0x800DDA28u, kResourceUsed = 0x800DDA2Cu;
constexpr u32 kSaveSlots = 748, kLoadedSave = 760, kCardProfiles = 764, kAccount = 788;
}

u32 Game::o1_account_byte_set(u32 value) {
    cover(0x800D5D54u);
    const u32 account = m.r32(m.r32(kCards) + kAccount); m.w8(account + 274, value); return account;
}
u32 Game::o1_save_byte_45c_set(u32 value) {
    cover(0x800D5D6Cu);
    const u32 save = m.r32(m.r32(kCards) + kLoadedSave); m.w8(save + 0x45C, value); return save;
}
u32 Game::o1_save_byte_45d_set(u32 value) {
    cover(0x800D5D84u);
    const u32 save = m.r32(m.r32(kCards) + kLoadedSave); m.w8(save + 0x45D, value); return save;
}
u32 Game::o1_save_byte_45e_set(u32 value) {
    cover(0x800D5D9Cu);
    const u32 save = m.r32(m.r32(kCards) + kLoadedSave); m.w8(save + 0x45E, value); return save;
}
u32 Game::o1_account_byte() {
    cover(0x800D5AA8u);
    return m.r8(m.r32(m.r32(kCards) + kAccount) + 274);
}
u32 Game::o1_save_flags() {
    cover(0x800D5CA0u);
    return static_cast<u32>(m.r16s(m.r32(m.r32(kCards) + kLoadedSave) + 22));
}
u32 Game::o1_flow_clear() {
    cover(0x800DA504u);
    const u32 flow = m.r32(0x800DD910u); m.w32(flow + 4, 0); m.w32(flow, 0); return 1;
}
u32 Game::o1_resource_arena_set(u32 base) {
    cover(0x800CD914u);
    m.w32(kResourceTop, base); m.w32(kResourceBase, base); m.w32(kResourceUsed, 0);
    return 0x800E0000u;
}
u32 Game::o1_drive_ready() {
    cover(0x800DA694u);
    Frame frame(*this, 24);
    return kcd_drive_state() == 3;
}
u32 Game::o1_glyph_rom_address(u32 character) {
    cover(0x800D783Cu);
    Frame frame(*this, 24);
    const u32 code = (static_cast<u32>(m.r8(character)) << 8) | m.r8(character + 1);
    (void)code;
    return 0xFFFFFFFFu;
}
u32 Game::o1_save_name_set(u32 name) {
    cover(0x800D75B4u);
    Frame frame(*this, 24);
    return libc_strcpy(m.r32(m.r32(kCards) + kLoadedSave) + 8, name);
}
u32 Game::o1_primitive_buffer_select() {
    cover(0x800D7C18u);
    const u32 buffers = m.r32(0x800DDA50u), index = m.r32(m.r32(g::kScene) + 4);
    const u32 current = m.r32(buffers + 4 * index); m.w32(buffers + 8, current); return current;
}
u32 Game::o1_text_item_field(u32 index) {
    cover(0x800DA800u);
    const u32 records = m.r32(m.r32(kScreens) + 32);
    return static_cast<u32>(m.r16s(records + 12 * index + 10));
}
u32 Game::o1_resource_release(u32 size) {
    cover(0x800CD978u);
    const u32 aligned = ((size + 3) >> 2) << 2, used = m.r32(kResourceUsed) - aligned;
    m.w32(kResourceUsed, used); m.w32(kResourceTop, m.r32(kResourceTop) - aligned); return used;
}
u32 Game::o1_save_scene_name() {
    cover(0x800D4190u);
    Frame frame(*this, 24);
    return libc_strcpy(m.r32(m.r32(kCards) + kLoadedSave) + 8, m.r32(g::kScene) + 2376);
}
u32 Game::o1_save_route(u32 slot) {
    cover(0x800D5CC0u);
    const u32 save = m.r32(m.r32(kCards) + kSaveSlots + 4 * slot);
    return m.r32(0x800DD800u + 4 * m.r8(save + 1124));
}
u32 Game::o1_save_flag_test(u32 slot, u32 bit) {
    cover(0x800D59C0u);
    const u32 cards = m.r32(kCards), save = m.r32(cards + (slot == 0xFFFFFFFFu ? kLoadedSave : kSaveSlots + 4 * slot));
    return (static_cast<u32>(m.r16s(save + 22)) >> (bit & 31u)) & 1;
}
u32 Game::o1_resource_buffer_select() {
    cover(0x800CD8C8u);
    const u32 scene = m.r32(g::kScene), buffers = m.r32(kResourceBuffers);
    m.w32(buffers + 8, m.r32(buffers + 4 * m.r32(scene + 4)));
    const u32 current = m.r32(buffers + 12 + 4 * m.r32(scene + 4));
    m.w32(buffers + 32, m.r32(buffers + 32) + 1); m.w32(buffers + 20, current); return current;
}
u32 Game::o1_card_profile_type(u32 slot) {
    cover(0x800D5C2Cu);
    const u32 profile = m.r32(m.r32(kCards) + kCardProfiles + 4 * (slot & 255));
    const u32 type = m.r8(0x800DD7B0u + m.r8(profile + 275));
    return m.r32(0x800DD798u + 4 * type);
}
u32 Game::o1_save_slot_load(u32 slot) {
    cover(0x800D47B8u);
    const u32 cards = m.r32(kCards), source = m.r32(cards + kSaveSlots + 4 * slot), save = m.r32(cards + kLoadedSave);
    for (u32 offset = 0; offset < 1152; offset += 16) {
        const u32 a = m.r32(source + offset), b = m.r32(source + offset + 4);
        const u32 c = m.r32(source + offset + 8), d = m.r32(source + offset + 12);
        m.w32(save + offset, a); m.w32(save + offset + 4, b);
        m.w32(save + offset + 8, c); m.w32(save + offset + 12, d);
    }
    return save + 1152;
}
}
