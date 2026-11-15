#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kUlzMagic = 0x1A7A6C55u, kTextureScratchSize = 0x10000u;
constexpr u32 kFlow = 0x800DD910u, kRequiredDisc = 0x800DD8D4u;
}

u32 Game::o1_resource_read(u32 directory, u32 filename, u32 capacity, u32 sizeOut) {
    cover(0x800D8AF0u);
    Frame frame(*this, 120);
    const u32 path = frame.at(16);
    libc_strcpy(path, directory);
    const u32 length = libc_strlen(directory);
    libc_strcpy(path + length, filename);
    u32 buffer = mem_alloc(capacity);
    const u32 file = hw->file_open(path, 1);
    if (file == 0xFFFFFFFFu) {
        m.w32(sizeOut, file); buffer = 0;
    } else {
        m.w32(sizeOut, hw->file_read(file, buffer, capacity));
    }
    hw->file_close(file);
    return buffer;
}
u32 Game::o1_texture_set_upload(u32 directory) {
    cover(0x800D8BA8u);
    Frame frame(*this, 32);
    const u32 scratch = mem_alloc(0x30000);
    if (m.r32(directory) == kUlzMagic) {
        ulz_decode(directory, scratch); directory = scratch;
    }
    const s32 count = m.r32s(directory);
    ld_upload_tims(ld_relocate_dir(directory), count);
    heap_set(scratch);
    return 1;
}
u32 Game::o1_texture_directory_upload(u32 directory) {
    cover(0x800D8C30u);
    Frame frame(*this, 40);
    const s32 count = m.r32s(directory);
    const u32 scratch = mem_alloc(kTextureScratchSize);
    const u32 entries = ld_relocate_dir(directory);
    for (s32 i = 0; i < count; ++i) {
        u32 texture = m.r32(entries + 4 * static_cast<u32>(i));
        if (m.r32(texture) == kUlzMagic) {
            ulz_decode(texture, scratch); texture = scratch;
        }
        load_tim(texture);
    }
    return heap_set(scratch);
}
u32 Game::o1_texture_upload(u32 texture) {
    cover(0x800D8CECu);
    Frame frame(*this, 32);
    const u32 scratch = mem_alloc(kTextureScratchSize);
    if (m.r32(texture) == kUlzMagic) {
        ulz_decode(texture, scratch); texture = scratch;
    }
    load_tim(texture);
    return heap_set(scratch);
}
u32 Game::o1_texture_upload_at(u32 texture, u32 pixelX, u32 pixelY, u32 paletteX, u32 paletteY) {
    cover(0x800D8D64u);
    Frame frame(*this, 56);
    const u32 scratch = mem_alloc(kTextureScratchSize);
    if (m.r32(texture) == kUlzMagic) {
        ulz_decode(texture, scratch); texture = scratch;
    }
    const u32 y = static_cast<u32>(static_cast<s16>(paletteY));
    m.w32(frame.at(16), y);
    load_tim_at(texture, static_cast<u32>(static_cast<s16>(pixelX)),
                static_cast<u32>(static_cast<s16>(pixelY)),
                static_cast<u32>(static_cast<s16>(paletteX)), y);

    return heap_set(scratch);
}
u32 Game::o1_texture_palette_upload(u32 texture, u32 paletteOut, u32 greyOut) {
    cover(0x800D8E38u);
    Frame frame(*this, 48);
    const u32 rectangle = frame.at(16);
    m.w16(rectangle, 432); m.w16(rectangle + 2, 56);
    m.w16(rectangle + 4, 16); m.w16(rectangle + 6, 8);
    const u32 scratch = mem_alloc(kTextureScratchSize);
    if (m.r32(texture) == kUlzMagic) {
        ulz_decode(texture, scratch); texture = scratch;
    }
    load_tim(texture);
    for (u32 word = 0; word < 8; ++word) {
        const u32 colour = m.r32(texture + 20 + 4 * word);
        m.w32(paletteOut + 4 * word, colour);
        m.w32(greyOut + 4 * word, 0xCE73CE73u);
    }
    hw->load_image(rectangle, greyOut);
    return heap_set(scratch);
}
u32 Game::o1_draw_mode_packet(u32 description, u32 packet, u32 orderingTable) {
    cover(0x800D8F3Cu);
    Frame frame(*this, 40);
    const u32 drawField = m.r8(description), dither = m.r8(description + 1);
    const u32 depth = m.r8(description + 2), blend = m.r8(description + 3);
    const u32 y = m.r16(description + 6), x = m.r16(description + 4);
    const u32 page = ((depth & 3) << 7) | ((blend & 3) << 5)
                   | ((y & 0x100) >> 4) | ((x & 0x3FF) >> 6) | ((y & 0x200) << 2);
    m.w32(frame.at(16), 0);
    set_draw_mode(packet, drawField, dither, page, 0);
    m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(orderingTable) & 0xFFFFFFu));
    m.w32(orderingTable, (m.r32(orderingTable) & 0xFF000000u) | (packet & 0xFFFFFFu));
    return packet + 12;
}
u32 Game::o1_flow_disc_check() {
    cover(0x800DA520u);
    Frame frame(*this, 24);
    const s32 disc = disc_number();
    const u32 flow = m.r32(kFlow);
    if (disc == 0) { m.w32(flow, 1); return 0; }
    const u32 required = m.r8(kRequiredDisc + m.r32(flow + 8));
    if (required == 3 || disc == 3) { m.w32(flow, 0); return 2; }
    if (disc == 4 || disc == -1) {
        if (required == 1) m.w32(flow, m.r32(flow + 4) ? 4 : 2);
        const u32 current = m.r32(kFlow), index = m.r32(current + 8);
        if (m.r8(kRequiredDisc + index) == 2) m.w32(current, m.r32(current + 4) ? 5 : 3);
    } else if (disc == 1) {
        if (required == 1) { m.w32(flow, 6); return 2; }
        m.w32(flow, m.r32(flow + 4) ? 5 : 3);
    } else if (disc == 2) {
        if (required == 2) { m.w32(flow, 6); return 2; }
        m.w32(flow, m.r32(flow + 4) ? 4 : 2);
    } else {
        return 0;
    }
    kcd_open_drive();
    return 1;
}
}
