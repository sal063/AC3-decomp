#include "scripts.hpp"

#include "sim/game/globals.hpp"

#include <format>

namespace ac3::viewer {
namespace {
u32 u8_at(const sim::Memory& m, u32 a) { return m.r8(a); }
s32 s16_at(const sim::Memory& m, u32 a) { return static_cast<s16>(m.r8(a) | (m.r8(a + 1) << 8)); }
u32 u32_at(const sim::Memory& m, u32 a) {
    return m.r8(a) | (m.r8(a + 1) << 8) | (m.r8(a + 2) << 16) | (static_cast<u32>(m.r8(a + 3)) << 24);
}
bool in_ram(u32 a) { return a >= 0x80010000u && a < 0x80200000u; }
const char* on_off(u32 v) { return v == 1 ? "on" : v == 0 ? "off" : "?"; }

std::string field(u32 off) {
    switch (off) {
    case 28: return "mode";
    case 30: return "counter A target";
    case 31: return "counter B target";
    case 32: return "counter A";
    case 33: return "counter B";
    case 34: return "counter C";
    case 39: return "ending";
    case 41: return "result";
    case 43: return "message";
    case 44: return "music";
    default: return std::format("state+{}", off);
    }
}
}

std::string unit_name(u32 slot) {
    if (slot == 0) return "player";
    return slot < 32 ? std::format("air {}", slot) : std::format("ground {}", slot - 32);
}

Decoded decode(const sim::Memory& m, u32 pc) {
    Decoded d;
    const u32 op = m.r8(pc), a = pc + 1;
    auto set = [&](u32 off) {
        d.text = std::format("{} = {}", field(off), u8_at(m, a));
        d.size = 2;
    };
    auto cond = [&](std::string text, u32 yes, u32 no, u32 end) {
        d.text = std::move(text);
        d.conditional = true;
        d.yes = yes;
        d.no = no;
        d.size = end - pc;
    };
    auto cond_word = [&](const char* what, bool word_less_than_value) {
        const s32 v = static_cast<s32>(u32_at(m, a));
        const u32 q = a + 4, p = q + 1;
        cond(std::format("IF {} {} {}", what, word_less_than_value ? ">" : "<", v), p + u8_at(m, p), p + 1,
             q + u8_at(m, q));
    };
    auto cond_byte = [&](std::string text, bool true_is_next) {
        const u32 end = a + u8_at(m, a + 1) + 1, branch = a + u8_at(m, a + 2) + 2;
        cond(std::move(text), true_is_next ? a + 3 : branch, true_is_next ? branch : a + 3, end);
    };
    switch (op) {
    case 0x00: d.text = "END"; break;
    case 0x01: d.text = std::format("RESULT {} (ends the mission in flight)", u8_at(m, a)); d.size = 2; break;
    case 0x02: set(36); break;
    case 0x03: set(35); break;
    case 0x04: set(37); break;
    case 0x05: set(38); break;
    case 0x06: {
        static constexpr u32 kMap[8] = {0, 1, 2, 8, 4, 16, 32, 64};
        const u32 v = u8_at(m, a);
        d.text = v < 8 ? std::format("ending = {}", kMap[v]) : std::format("ending (bad {})", v);
        d.size = 2;
        break;
    }
    case 0x07: d.text = "end flags |= 4, result = 9"; break;
    case 0x08: d.text = std::format("NEXT MISSION {}", u8_at(m, a)); d.size = 2; break;
    case 0x09: d.text = "end flags |= 1"; break;
    case 0x0A: d.text = std::format("scene+2390 = {}", u8_at(m, a)); d.size = 2; break;
    case 0x0B: cond_word("timer", true); break;
    case 0x0C: cond_word("score", false); break;
    case 0x0D: cond_byte(std::format("IF counter C < {}", u8_at(m, a)), false); break;
    case 0x0E: cond_byte(std::format("IF counter B > {}", u8_at(m, a)), false); break;
    case 0x0F: cond_byte(std::format("IF counter A < {}", u8_at(m, a)), false); break;
    case 0x10: {
        const u32 q = a + 4, p = q + 1;
        cond("IF state+47 == 1", p + 1, p + u8_at(m, p), q + u8_at(m, q));
        break;
    }
    case 0x11: cond("IF kills >= losses", a + u8_at(m, a + 1) + 1, a + 2, a + u8_at(m, a)); break;
    case 0x12: cond_byte(std::format("IF story flag {}", u8_at(m, a)), true); break;
    case 0x13: {
        const u32 p = a + 1;
        cond("IF random (1 in 2)", p + 1, p + u8_at(m, p), a + u8_at(m, a));
        break;
    }
    case 0x14:
        cond(std::format("IF hp of {} < {}", unit_name(u8_at(m, a)), u8_at(m, a + 1)), a + u8_at(m, a + 3) + 3, a + 4,
             a + u8_at(m, a + 2) + 2);
        break;
    case 0x15: d.text = std::format("ACTIVATE {}", unit_name(u8_at(m, a))); d.size = 2; break;
    case 0x16: d.text = std::format("RELEASE {}", unit_name(u8_at(m, a))); d.size = 2; break;
    case 0x17:
    case 0x19: d.text = std::format("(nothing; {})", u8_at(m, a)); d.size = 2; break;
    case 0x18: {
        const s32 v = s16_at(m, a + 1);
        d.text = std::format("HP of {} = {}", unit_name(u8_at(m, a)),
                             v == -1 ? std::string("full") : v == -2 ? std::string("saved") : v == -3 ? std::string("(saved now)")
                                                                                                         : std::to_string(v));
        d.size = 4;
        break;
    }
    case 0x1A: d.text = std::format("{}: slot flag 0x20000", unit_name(u8_at(m, a))); d.size = 2; break;
    case 0x1B: d.text = std::format("TARGET {} {}", unit_name(u8_at(m, a)), on_off(u8_at(m, a + 1))); d.size = 3; break;
    case 0x1C: d.text = std::format("FRIENDLY {} {}", unit_name(u8_at(m, a)), on_off(u8_at(m, a + 1))); d.size = 3; break;
    case 0x1D: d.text = std::format("RADAR {} bits {}", unit_name(u8_at(m, a)), u8_at(m, a + 1)); d.size = 3; break;
    case 0x1E: d.text = "(nothing; 2 bytes)"; d.size = 3; break;
    case 0x1F: d.text = std::format("ROUTE {} from entry {}", unit_name(u8_at(m, a)), u8_at(m, a + 1)); d.size = 3; break;
    case 0x20: d.text = std::format("GROUND ROUTE record {} = route {}", u8_at(m, a), u8_at(m, a + 1)); d.size = 3; break;
    case 0x21: d.text = std::format("FLIGHT LIMITS {} = {}", unit_name(u8_at(m, a)), u8_at(m, a + 1)); d.size = 3; break;
    case 0x22: d.text = std::format("{}: flag 0x1000 {}", unit_name(u8_at(m, a)), on_off(u8_at(m, a + 1))); d.size = 3; break;
    case 0x23: d.text = std::format("{}: flag 0x2000 {}", unit_name(u8_at(m, a)), on_off(u8_at(m, a + 1))); d.size = 3; break;
    case 0x24: d.text = std::format("LINK {} {}", unit_name(u8_at(m, a)), on_off(u8_at(m, a + 1))); d.size = 3; break;
    case 0x25: {
        const u32 k = u8_at(m, a);
        d.text = std::format("PLACE {} at {} {} {} facing {} {} {}", unit_name(k), static_cast<s32>(u32_at(m, a + 1)),
                             static_cast<s32>(u32_at(m, a + 5)), static_cast<s32>(u32_at(m, a + 9)), s16_at(m, a + 13),
                             s16_at(m, a + 15), s16_at(m, a + 17));
        d.size = 22;
        break;
    }
    case 0x29: d.text = std::format("RADIO call {} after {}", u8_at(m, a), s16_at(m, a + 1)); d.size = 4; break;
    case 0x2A: d.text = std::format("SOUND {} {}", u8_at(m, a), u8_at(m, a + 1)); d.size = 3; break;
    case 0x2B: {
        const u32 n = u8_at(m, a);
        d.text = "SOUNDS";
        for (u32 k = 0; k < n && k < 16; ++k) d.text += std::format(" {}/{}", u8_at(m, a + 1 + 2 * k), u8_at(m, a + 2 + 2 * k));
        d.size = 2 + 2 * n;
        break;
    }
    case 0x2D: set(30); break;
    case 0x2E: d.text = "counter A = 0"; break;
    case 0x2F: set(31); break;
    case 0x30: d.text = "counter B = 0"; break;
    case 0x31: d.text = "counter C += 1"; break;
    case 0x32: d.text = "counter C = 0"; break;
    case 0x33: d.text = "SHOW MESSAGE"; break;
    case 0x34: set(43); break;
    case 0x35: d.text = "counter A *= 2"; break;
    case 0x36: set(44); break;
    case 0x37: d.text = "PLAY MUSIC"; break;
    case 0x38: d.text = std::format("PLAY MUSIC {}", u8_at(m, a)); d.size = 2; break;
    case 0x39: set(28); break;
    case 0x3A: d.text = std::format("state+20 = {}", s16_at(m, a)); d.size = 3; break;
    case 0x3B: d.text = "world flag 4"; break;
    case 0x3C: cond_word("clock", true); break;
    case 0x3D: cond_byte(std::format("IF variant == {}", u8_at(m, a)), true); break;
    case 0x3E: cond_byte(std::format("IF scene+916 == {}", u8_at(m, a)), true); break;
    case 0x46: d.text = std::format("end flags |= 2, state+62 = {}", u8_at(m, a)); d.size = 2; break;
    case 0x47: d.text = "end flags |= 4"; d.size = 2; break;
    case 0x48: d.text = std::format("state+49 = {}, state+50 = {}", u8_at(m, a), s16_at(m, a + 1)); d.size = 4; break;
    case 0x49: d.text = "state+48 = 1, state+26 = 0"; break;
    case 0x4A:
        d.text = std::format("state+52.. = {} {} {} {}", u8_at(m, a), u8_at(m, a + 1), u8_at(m, a + 2), u8_at(m, a + 3));
        d.size = 5;
        break;
    case 0x4B: d.text = std::format("SCORE += {}", s16_at(m, a)); d.size = 3; break;
    case 0x4C:
        d.text = std::format("state+52 = {}, state+56 = {}, state+58 = {}", u8_at(m, a), s16_at(m, a + 1), s16_at(m, a + 3));
        d.size = 6;
        break;
    default: d.text = std::format("(op {:02X}: nothing)", op); break;
    }
    return d;
}

void listing(const sim::Memory& m, u32 pc, std::vector<std::string>& out, size_t limit, int depth) {
    if (depth > 12) return;
    const std::string indent(static_cast<size_t>(2 * depth), ' ');
    for (u32 n = 0; n < 4096 && out.size() < limit; ++n) {
        if (!in_ram(pc)) {
            out.push_back(indent + "(outside RAM)");
            return;
        }
        const Decoded d = decode(m, pc);
        out.push_back(std::format("{}{:08X}  {}", indent, pc, d.text));
        if (m.r8(pc) == 0) return;
        if (d.conditional) {
            out.push_back(indent + "  THEN");
            listing(m, d.yes, out, limit, depth + 2);
            out.push_back(indent + "  ELSE");
            listing(m, d.no, out, limit, depth + 2);
        }
        pc += d.size;
    }
}

namespace {
std::string header_label(const sim::Memory& m, u32 md, u32 off) {
    auto var = [](u32 base, u32 o) { return (o - base) / 4; };
    if (off >= 8 && off < 24) return std::format("intro (variant {})", var(8, off));
    if (off >= 24 && off < 40) return std::format("entering sub-phase 8 (variant {})", var(24, off));
    if (off >= 40 && off < 56) return std::format("counter A reached its target (variant {})", var(40, off));
    if (off >= 56 && off < 72) return std::format("counter B reached its target (variant {})", var(56, off));
    if (off >= 72 && off < 328) {
        const u32 k = (off - 72) / 64, i = ((off - 72) % 64) / 4;
        return std::format("timed {} (variant {}) at timer {}", i, k, m.r16(md + 344 + 32 * k + 2 * i));
    }
    if (off == 328) return "mission event 328";
    if (off == 332) return "mission event 332";
    if (off == 336) return "mission event 336";
    if (off == 340) return "mission event 340";
    if (off >= 472 && off < 488) return std::format("sub-phase 16 (variant {})", var(472, off));
    if (off >= 504 && off < 520) return std::format("entering sub-phase 64 (variant {})", var(504, off));
    if (off >= 536 && off < 552) return std::format("sub-phase 0x4000 (variant {})", var(536, off));
    if (off >= 552 && off < 568) return std::format("the mission's end (variant {})", var(552, off));
    if (off >= 568 && off < 584) return std::format("restart (variant {})", var(568, off));
    if (off >= 584 && off < 600) return std::format("entering sub-phase 0x2000 (variant {})", var(584, off));
    return {};
}
constexpr u32 kMissionFile = 0x800BCE98u;
}

std::string label(const sim::Memory& m, u32 pc) {
    if (!pc) return {};
    const u32 md = m.r32(sim::g::kMissionData);
    if (in_ram(md))
        for (u32 off = 8; off < 600; off += 4)
            if (m.r32(md + off) == pc)
                if (std::string s = header_label(m, md, off); !s.empty()) return s;
    const u32 start = m.r32(kMissionFile + 0x14);
    if (in_ram(start) && m.r32(start) == pc) return "mission start";
    const u32 air = m.r32(sim::g::kSlotRecords), ground = m.r32(sim::g::kGroundRecords);
    for (u32 k = 0; k < 32 && in_ram(air); ++k) {
        const u32 r = air + 64 * k;
        if (m.r32(r + 52) == pc) return unit_name(k) + " shot down";
        if (m.r32(r + 56) == pc) return unit_name(k) + " sighted";
        if (m.r32(r + 60) == pc) return unit_name(k) + " sighted (variant > 0)";
    }
    for (u32 g = 0; g < 32 && in_ram(ground); ++g) {
        const u32 r = ground + 40 * g;
        if (m.r32(r + 28) == pc) return unit_name(32 + g) + " destroyed";
        if (m.r32(r + 32) == pc) return unit_name(32 + g) + " sighted";
        if (m.r32(r + 36) == pc) return unit_name(32 + g) + " sighted (variant > 0)";
    }
    return {};
}

std::vector<std::pair<u32, std::string>> entries(const sim::Memory& m) {
    std::vector<std::pair<u32, std::string>> out;
    auto add = [&](u32 pc, std::string what) {
        if (!in_ram(pc) || !m.r8(pc)) return;
        for (auto& e : out)
            if (e.first == pc) {
                if (e.second.find(what) == std::string::npos && e.second.size() < 120) e.second += "; " + what;
                return;
            }
        out.emplace_back(pc, std::move(what));
    };
    const u32 start = m.r32(kMissionFile + 0x14);
    if (in_ram(start)) add(m.r32(start), "mission start");
    const u32 md = m.r32(sim::g::kMissionData);
    if (in_ram(md))
        for (u32 off = 8; off < 600; off += 4)
            if (std::string s = header_label(m, md, off); !s.empty()) add(m.r32(md + off), std::move(s));
    const u32 air = m.r32(sim::g::kSlotRecords), ground = m.r32(sim::g::kGroundRecords);
    for (u32 k = 0; k < 32 && in_ram(air); ++k) {
        const u32 r = air + 64 * k;
        add(m.r32(r + 52), unit_name(k) + " shot down");
        add(m.r32(r + 56), unit_name(k) + " sighted");
        add(m.r32(r + 60), unit_name(k) + " sighted (variant > 0)");
    }
    for (u32 g = 0; g < 32 && in_ram(ground); ++g) {
        const u32 r = ground + 40 * g;
        add(m.r32(r + 28), unit_name(32 + g) + " destroyed");
        add(m.r32(r + 32), unit_name(32 + g) + " sighted");
        add(m.r32(r + 36), unit_name(32 + g) + " sighted (variant > 0)");
    }
    return out;
}

void Tracer::add(int depth, bool header, std::string text) {
    if (frozen) return;
    lines_.push_back(Line{field, depth, header, std::move(text)});
    ++total_;
    while (lines_.size() > 4000) lines_.pop_front();
}

void Tracer::script_begin(u32 pc) {
    if (depth_ == 0) {
        ++runs_;
        std::string what = label(m_, pc);
        add(0, true, std::format("{:08X}  {}", pc, what.empty() ? std::string("(a script)") : what));
    }
    ++depth_;
}

void Tracer::script_op(u32 pc, u32  ) {
    add(depth_, false, std::format("{:08X}  {}", pc, decode(m_, pc).text));
}

void Tracer::script_end(u32  ) {
    if (depth_ > 0) --depth_;
}
}
