#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kLibgpuTail = 0x800BBA24u;
constexpr u32 kLibgpuTailNext = 0x800BBA10u;
constexpr u32 kOtTerm = 0x00FFFFFFu;
}

u32 Game::clear_otag(u32 ot, s32 n) {
    for (s32 i = 0; i < n; ++i) {
        const u32 at = ot + 4u * static_cast<u32>(i);
        m.w32(at, i + 1 < n ? ((at + 4u) & 0x00FFFFFFu) : kOtTerm);
    }
    if (n > 0) {
        m.w32(kLibgpuTail, (4u << 24) | (kLibgpuTailNext & 0x00FFFFFFu));
        m.w32(kLibgpuTailNext, (4u << 24) | kOtTerm);
        m.w32(ot + 4u * static_cast<u32>(n - 1), kLibgpuTail & 0x00FFFFFFu);
    }
    return ot;
}

u32 Game::set_draw_offset(u32 p, u32 ofs) {
    if (m.ptr(ofs, 8)) {
        m.w32(p, (2u << 24) | (m.r32(p) & 0x00FFFFFFu));
        m.w32(p + 4, 0xE5000000u | ((m.r16(ofs + 2) & 0x7FFu) << 11) | (m.r16(ofs) & 0x7FFu));
        m.w32(p + 8, 0);
    }
    return p;
}

u32 Game::get_tpage(u32 tp, u32 abr, u32 x, u32 y) {
    return (((tp & 3u) << 7) | ((abr & 3u) << 5) | ((y & 0x100u) >> 4) | ((x & 0x3FFu) >> 6) | ((y & 0x200u) << 2)) &
           0xFFFFu;
}

u32 Game::libc_memmove(u32 d, u32 s, u32 n) {
    if (d < s)
        for (u32 i = 0; i < n; ++i) m.w8(d + i, m.r8(s + i));
    else
        for (u32 i = n; i-- > 0;) m.w8(d + i, m.r8(s + i));
    return d;
}

bool Game::port_call_lib_native(u32 fn, const u32* r, u32& out) {
    const u32 a0 = r[4], a1 = r[5], a2 = r[6], a3 = r[7];
    auto s = [](u32 v) { return static_cast<s32>(v); };
    switch (fn) {
    case 0x8009BA54u: out = 1; return true;
    case 0x8009BABCu: out = 0; return true;
    case 0x8009CD70u: out = 0; return true;
    case 0x8009D054u: out = 0; return true;
    case 0x8009D1DCu: out = 0; return true;
    case 0x8009D4E0u: out = 2; return true;
    case 0x8009D518u: out = 1; return true;
    case 0x8009F078u:
    case 0x8009F1D8u: hw->spu_init(); out = 0; return true;
    case 0x8009F618u:
    case 0x8009F6D4u:
    case 0x8009F77Cu:
    case 0x8009F9FCu:
    case 0x8009FA80u:
    case 0x8009FB28u:
    case 0x8009FBCCu:
    case 0x8009FC08u:
    case 0x8009FD18u:
    case 0x8009FEB8u: out = 0; return true;
    case 0x8009FAE4u: out = hw->spu_fset_rxx(a0, a1, a2); return true;
    case 0x8009FF38u: out = hw->spu_is_in_allocate_area(a0); return true;
    case 0x800A0A28u: out = a1; return true;
    case 0x800A1738u: out = hw->spu_rget_all_keys_status(s(a0), s(a1), a2); return true;
    case 0x800A22D8u: out = hw->interrupt_callback(a0, a1); return true;
    case 0x800A236Cu: out = hw->vsync_callbacks(a0, a1); return true;
    case 0x800A23CCu: out = hw->restart_callback(); return true;
    case 0x800A23FCu: out = hw->check_callback(); return true;
    case 0x800A240Cu: out = hw->get_intr_mask(); return true;
    case 0x800A2F84u:
    case 0x800A2FB8u: out = 0; return true;
    case 0x800A300Cu: out = hw->cd_ready(a0, a1); return true;
    case 0x800A3A1Cu: out = hw->cd_sync(a0, a1); return true;
    case 0x800A3C9Cu: out = hw->cd_ready_raw(a1); return true;
    case 0x800A3F64u:
    case 0x800A4370u:
    case 0x800A43F8u:
    case 0x800A45BCu:
    case 0x800A47E8u:
    case 0x800A4950u:
    case 0x800A4B3Cu:
    case 0x800A5AF8u: out = 0; return true;
    case 0x800A4A50u: out = hw->cd_get_sector(a0, a1); return true;
    case 0x800A7F8Cu:
    case 0x800A7FE8u:
    case 0x800A809Cu:
    case 0x800AACF8u: out = 0; return true;
    case 0x800A83A8u: hw->clear_image(a0, a1, a2, a3); out = 0; return true;
    case 0x800A85B8u: out = clear_otag(a0, s(a1)); return true;
    case 0x800A872Cu: hw->draw_prim(a0); out = 0; return true;
    case 0x800A88B8u:
        build_dr_env(a1);
        m.w32(a1 + 28, (m.r32(a1 + 28) & 0xFF000000u) | (a0 & 0x00FFFFFFu));
        hw->put_draw_env(a1);
        out = 0;
        return true;
    case 0x800A8990u: hw->get_draw_env(a0); out = a0; return true;
    case 0x800A8EBCu: hw->get_disp_env(a0); out = a0; return true;
    case 0x800A8FA0u: out = set_draw_offset(a0, a1); return true;
    case 0x800AAAB8u: hw->move_image(a0, a1, a2); out = 0; return true;
    case 0x800AABFCu: hw->draw_otag(a0); out = 0; return true;
    case 0x800AC6C8u: out = get_tpage(a0, a1, a2, a3); return true;
    case 0x800AC8CCu: out = a0; return true;
    case 0x800AC958u: out = hw->dec_dct_put_env(a0); return true;
    case 0x800ACA8Cu: out = 0; return true;
    case 0x800ACB10u: out = hw->dec_dct_in_callback(a0); return true;
    case 0x800AD538u: hw->init_card(a0); out = 0; return true;
    case 0x800AD548u: out = hw->start_card(); return true;
    case 0x800AD558u:
    case 0x800AD788u: out = hw->stop_card(); return true;
    case 0x800AD568u:
    case 0x800AD644u:
    case 0x800AD6D8u:
    case 0x800AD748u: out = 0; return true;
    case 0x800ADFF8u: out = 0; return true;
    case 0x800AE698u:
    case 0x800AE794u:
    case 0x800AE7D0u: out = a0; return true;
    case 0x800AF178u: out = libc_memmove(a0, a1, a2); return true;
    case 0x800AF284u:
        unported(fn, "longjmp");
        out = 0;
        return true;
    case 0x800AF2D8u:
    case 0x800AF2E8u: out = 0; return true;
    case 0x800AF348u: out = hw->wait_event(a0); return true;
    case 0x800AF378u: out = hw->disable_event(a0); return true;
    case 0x800AF58Cu: out = 1; return true;
    case 0x800AF898u: out = hw->first_file(a0, a1); return true;
    default: return false;
    }
}
}
