#include "machine/input_recording.hpp"
#include "machine/machine.hpp"
#include "sim/game/game.hpp"
#include "sim/state_file.hpp"
#include "vfs/disc_image.hpp"
#include "vfs/iso9660.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace ac3;
using namespace ac3::sim;

namespace {
struct Options {
    std::string path;
    int part = 0;
    u32 tick_from = 0, tick_to = 0xFFFFFFFFu;
    int show = 12;
    bool verbose = false;
    bool coverage = false;
    int mode = 0;
    bool reads = false;
    bool stale = false;
    bool keep_gte = false;
    std::vector<std::pair<u32, u32>> keep, scrub;
    std::vector<int> oracle_parts;
    std::vector<std::pair<u32, u32>> drop;
    bool consumers = false, resync = false, skip_gte = false;
    std::vector<std::pair<u32, u32>> draw_copy;
    bool draw_scratch = false, draw_gte = false;
    std::string input;
    std::string machine;
    std::string disc;
    u32 field_from = 0, field_to = 0xFFFFFFFFu;
    std::string vram;
    bool draw_oracle = false;
    bool draw_full = false;
    bool stack_sync = false;
    bool boot = false;
    std::string audio_dump;
    std::string cards;
    u32 vram_dump_field = 0;
    std::string vram_dump_path;
};

u64 hash_bytes(const void* p, size_t n) {
    const u8* b = static_cast<const u8*>(p);
    u64 h = 0x9E3779B97F4A7C15ull ^ static_cast<u64>(n);
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        u64 w;
        std::memcpy(&w, b + i, 8);
        h ^= w;
        h *= 0xFF51AFD7ED558CCDull;
        h ^= h >> 32;
    }
    for (; i < n; i++) {
        h ^= b[i];
        h *= 0x100000001B3ull;
    }
    h ^= h >> 33;
    h *= 0xC4CEB9FE1A85EC53ull;
    h ^= h >> 33;
    return h;
}

std::map<u32, u64> load_vram_hashes(const std::string& path) {
    std::map<u32, u64> out;
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return out;
    char line[512];
    if (!std::fgets(line, sizeof line, f)) {
        std::fclose(f);
        return out;
    }
    while (std::fgets(line, sizeof line, f)) {
        unsigned vb;
        int grp, ph, want, res, msn;
        unsigned long long ram, vram;
        if (std::sscanf(line, "%u %d %d %d %d %d %llx %llx", &vb, &grp, &ph, &want, &res, &msn, &ram, &vram) == 8)
            out[vb] = vram;
    }
    std::fclose(f);
    return out;
}

using machine::InputRecording;

void attach_machine(machine::Machine& mach, Game& game, const InputRecording& rec, const MachineState& at) {
    game.hw = &mach;
    mach.set_clock(at.field, at.field);
    mach.poll_input = [&rec](u32 vb, machine::PadInput p[2]) { rec.pads(vb, p); };
    game.phase_result = at.regs[2];
}

bool parse_range(const char* s, std::pair<u32, u32>& out) {
    char* end;
    out.first = static_cast<u32>(std::strtoul(s, &end, 16));
    if (*end != '-') return false;
    out.second = static_cast<u32>(std::strtoul(end + 1, nullptr, 16));
    return out.second > out.first;
}

bool in_ranges(const std::vector<std::pair<u32, u32>>& rs, u32 a) {
    for (const auto& [lo, hi] : rs)
        if (a >= lo && a < hi) return true;
    return false;
}

constexpr u32 kStackWindow = 0x2000u;

constexpr u32 kTickFn = 0x80032F3Cu;
constexpr int kPartList[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};

int part_between(const MachineState& a, const MachineState& b) {
    const bool a_phase = Game::is_flight_phase(a.target), b_phase = Game::is_flight_phase(b.target);
    const bool a_tick = Game::is_flight_tick(a.target), b_tick = Game::is_flight_tick(b.target);
    if (a_tick && b_tick && a.target == b.target && a.tick == b.tick) {
        if (a.checkpoint == 0 && b.checkpoint == 15) return 17;
        if (a.target != kTickFn) return -1;
        if (b.checkpoint >= 1 && b.checkpoint <= 9 && a.checkpoint + 1 == b.checkpoint)
            return static_cast<int>(b.checkpoint);
        if (a.checkpoint == 9 && b.checkpoint == 15) return 10;
        return -1;
    }
    if (a_tick && a.checkpoint == 15 && b_phase && b.checkpoint == 18) return 11;
    if (a_phase && b_phase && a.target == b.target) {
        if (a.checkpoint == 18 && b.checkpoint == 19) return 12;
        if (a.checkpoint == 19 && b.checkpoint == 20) return 13;
        if (a.checkpoint == 20 && b.checkpoint == 31) return 18;
        if (a.checkpoint == 30 && b.checkpoint == 16) return 19;
    }
    if (a_phase && a.checkpoint == 16 && b_tick && b.checkpoint == 0) return 15;
    if (a_phase && a.checkpoint == 30 && b_tick && b.checkpoint == 0) return 15;
    if (a.checkpoint == 31 && b.checkpoint == 30) return 14;
    if (a.checkpoint == 30 && b.checkpoint == 31 && a.target == b.target) return 16;
    return -1;
}

bool inside_call(const MachineState& prev, const MachineState& cur) {
    return prev.checkpoint == 30 && !Game::is_flight_phase(prev.target) && cur.checkpoint != 31;
}

bool ported(int part, const MachineState& from) {
    return part == 16 || Game::part_ported(part, from.target);
}

bool frame_part(int part) { return part > 10; }

constexpr std::pair<u32, u32> kRenderBuffers[] = {{0x80171830u, 0x801B0830u}, {0x800BD2A8u, 0x800BF2D0u}};

struct Diff {
    std::string where;
    u32 addr, before, oracle, engine;
};

void diff_bytes(const char* space, u32 base, const u8* before, const u8* oracle, const u8* engine, u32 size,
                u32 skip_lo, u32 skip_hi, std::vector<Diff>& out) {
    for (u32 off = 0; off + 4 <= size; off += 4) {
        const u32 a = base + off;
        if (a >= skip_lo && a < skip_hi) continue;
        u32 b, o, e;
        std::memcpy(&o, oracle + off, 4);
        std::memcpy(&e, engine + off, 4);
        if (o == e) continue;
        std::memcpy(&b, before + off, 4);
        out.push_back({space, a, b, o, e});
    }
}

const char* gte_name(u32 r) {
    static const char* names[64] = {
        "VXY0", "VZ0", "VXY1", "VZ1", "VXY2", "VZ2", "RGBC", "OTZ", "IR0", "IR1", "IR2", "IR3", "SXY0", "SXY1",
        "SXY2", "SXYP", "SZ0", "SZ1", "SZ2", "SZ3", "RGB0", "RGB1", "RGB2", "RES1", "MAC0", "MAC1", "MAC2", "MAC3",
        "IRGB", "ORGB", "LZCS", "LZCR", "RT11", "RT13", "RT22", "RT31", "RT33", "TRX", "TRY", "TRZ", "L11", "L13",
        "L22", "L31", "L33", "RBK", "GBK", "BBK", "LR1", "LR3", "LG2", "LB1", "LB3", "RFC", "GFC", "BFC", "OFX",
        "OFY", "H", "DQA", "DQB", "ZSF3", "ZSF4", "FLAG"};
    return names[r & 63];
}

int run_ticks(const Options& o) {
    StateFile file;
    std::string err;
    if (!file.open(o.path, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 2;
    }
    Memory mem;
    Gte gte;
    Game game(mem, gte);
    game.track_coverage = o.coverage || o.stale;
    const bool chain = o.mode == 2;
    std::vector<u32> tags(Memory::kRamSize + Memory::kScratchSize);
    std::map<std::pair<u32, u32>, u32> stale;

    MachineState cur, start, last_end;
    bool have_start = false, have_end = false, live = false;
    std::vector<u8> access(Memory::kRamSize + Memory::kScratchSize);
    struct Use {
        u32 ticks = 0, first = 0, last = 0;
    };
    std::map<u32, Use> consumed;
    auto use = [&](u32 addr, u32 tick) {
        Use& u = consumed[addr];
        if (!u.ticks++) u.first = tick;
        u.last = tick;
    };
    int pass = 0, fail = 0, shown = 0;
    u32 first_fail = 0;
    while (file.next(cur)) {
        if (cur.target != 0x80032F3Cu) continue;
        if (cur.checkpoint == 0) {
            start = cur;
            have_start = true;
            continue;
        }
        if (cur.checkpoint != 15 || !have_start || start.tick != cur.tick) continue;
        if (cur.tick < o.tick_from || cur.tick > o.tick_to) continue;

        if (chain && live && have_end) {
            u8* ram = mem.ram();
            for (u32 off = 0; off < Memory::kRamSize; off += 4)
                if (std::memcmp(&start.ram[off], &last_end.ram[off], 4) && !in_ranges(o.keep, 0x80000000u + off))
                    std::memcpy(ram + off, &start.ram[off], 4);
            u8* sp = mem.scratch();
            for (u32 off = 0; off < Memory::kScratchSize; off += 4)
                if (std::memcmp(&start.scratch[off], &last_end.scratch[off], 4) &&
                    !in_ranges(o.keep, Memory::kScratchBase + off))
                    std::memcpy(sp + off, &start.scratch[off], 4);
            if (!o.keep_gte) gte.load(start.gte);
            for (const auto& [lo, hi] : o.scrub)
                for (u32 a = lo; a < hi; ++a)
                    if (u8* q = mem.ptr(a)) *q = 0xA5;
        } else {
            std::memcpy(mem.ram(), start.ram.data(), Memory::kRamSize);
            std::memcpy(mem.scratch(), start.scratch.data(), Memory::kScratchSize);
            gte.load(start.gte);
            live = true;
        }
        mem.reset_faults();
        const u32 tick_sp = start.regs[29] - 0x30u;
        game.sp = tick_sp;
        game.reg_s4 = start.regs[20];
        game.reg_s0 = start.regs[16];
        game.reg_ra = start.regs[31];
        const bool consuming = o.reads && chain && have_end;
        const bool logging = consuming || o.stale;
        if (logging) {
            std::fill(access.begin(), access.end(), u8{0});
            mem.set_access_log(&access);
            if (o.stale) mem.set_access_tags(&tags, &game.frame_owner);
        }
        for (int p = 1; p <= 10; ++p) game.run_part(p);
        mem.set_access_log(nullptr);
        mem.set_access_tags(nullptr, nullptr);
        if (o.stale) {
            for (u32 a = (tick_sp - kStackWindow) & ~3u; a < tick_sp; a += 4) {
                const u32 off = a & 0x1FFFFFu;
                for (u32 b = 0; b < 4; ++b)
                    if (access[off + b] == Memory::kReadFirst) {
                        ++stale[{tags[off + b], tick_sp - a}];
                        break;
                    }
            }
        }
        if (consuming) {
            for (u32 off = 0; off < Memory::kRamSize; off += 4) {
                if (!std::memcmp(&start.ram[off], &last_end.ram[off], 4)) continue;
                for (u32 b = 0; b < 4; ++b)
                    if (access[off + b] == Memory::kReadFirst) {
                        use(0x80000000u + off, cur.tick);
                        break;
                    }
            }
            for (u32 off = 0; off < Memory::kScratchSize; off += 4) {
                if (!std::memcmp(&start.scratch[off], &last_end.scratch[off], 4)) continue;
                for (u32 b = 0; b < 4; ++b)
                    if (access[Memory::kRamSize + off + b] == Memory::kReadFirst) {
                        use(Memory::kScratchBase + off, cur.tick);
                        break;
                    }
            }
        }

        std::vector<Diff> diffs;
        diff_bytes("ram", 0x80000000u, start.ram.data(), cur.ram.data(), mem.ram(), Memory::kRamSize,
                   0x80000000u + ((tick_sp - kStackWindow) & 0x1FFFFFu), 0x80000000u + (tick_sp & 0x1FFFFFu), diffs);
        diff_bytes("scratch", Memory::kScratchBase, start.scratch.data(), cur.scratch.data(), mem.scratch(),
                   Memory::kScratchSize, 0, 0, diffs);
        u32 eg[64];
        gte.save(eg);
        for (u32 r = 0; r < 64; ++r)
            if (eg[r] != cur.gte[r]) diffs.push_back({std::string("gte ") + gte_name(r), r, start.gte[r], cur.gte[r], eg[r]});
        if (diffs.empty() && !mem.faults()) {
            ++pass;
            if (o.verbose) std::printf("tick %u: ok\n", cur.tick);
        } else {
            if (!fail) first_fail = cur.tick;
            ++fail;
            if (shown++ < 3 || o.verbose) {
                std::printf("tick %u (field %u): %zu words differ%s\n", cur.tick, cur.field, diffs.size(),
                            mem.faults() ? ", and the engine accessed unmapped memory" : "");
                for (int i = 0; i < static_cast<int>(diffs.size()) && i < o.show; ++i) {
                    const Diff& d = diffs[static_cast<size_t>(i)];
                    std::printf("  %-10s %08X  before %08X  oracle %08X  engine %08X\n", d.where.c_str(), d.addr,
                                d.before, d.oracle, d.engine);
                }
            }
        }
        last_end = cur;
        have_end = true;
    }
    if (!file.error().empty()) std::fprintf(stderr, "state file: %s\n", file.error().c_str());
    std::printf("\n%s ticks: %d exact, %d different", chain ? "chained" : "whole", pass, fail);
    if (fail) std::printf(" (first at tick %u)", first_fail);
    std::printf("\n");
    if (o.stale) {
        std::printf("\nstack words read before written (function, tick sp minus address, ticks):\n");
        for (const auto& [k, n] : stale) std::printf("  %08X  -%-5u %u\n", k.first, k.second, n);
    }
    if (o.reads) {
        std::printf("\nwords the frame work between ticks changed and the next tick read (ticks, first, last):\n");
        for (const auto& [addr, u] : consumed) std::printf("  %08X %6u %6u %6u\n", addr, u.ticks, u.first, u.last);
    }
    if (o.coverage) {
        std::printf("\ncoverage (reimplemented functions, calls):\n");
        for (const auto& [addr, n] : game.coverage) std::printf("  %08X %llu\n", addr, static_cast<unsigned long long>(n));
    }
    if (game.unported_hits) std::printf("\n%u calls reached code not reimplemented yet\n", game.unported_hits);
    return fail ? 1 : 0;
}

int run_frames(const Options& o) {
    StateFile file;
    std::string err;
    if (!file.open(o.path, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 2;
    }
    Memory mem;
    Gte gte;
    Game game(mem, gte);
    game.track_coverage = o.coverage || o.consumers;
    machine::Machine mach(mem);
    mach.call_guest = [&game](u32 fn, u32 a0, u32 a1) { game.call_guest(fn, a0, a1); };
    if (!o.cards.empty()) {
        mach.card.set_path(0, o.cards + "/card1.mcd", false);
        mach.card.set_path(1, o.cards + "/card2.mcd", false);
    }
    if (!o.disc.empty()) {
        std::string derr;
        if (!mach.mount(o.disc, {}, &derr)) {
            std::fprintf(stderr, "simdiff: %s\n", derr.c_str());
            return 2;
        }
    }
    InputRecording rec;
    if (!o.input.empty() && !rec.load(o.input)) {
        std::fprintf(stderr, "simdiff: cannot read %s\n", o.input.c_str());
        return 2;
    }
    std::vector<u8> access(Memory::kRamSize + Memory::kScratchSize);
    std::vector<u32> tags(Memory::kRamSize + Memory::kScratchSize);
    std::vector<u8> copied(Memory::kRamSize / 4);
    std::vector<u8> drawing_own(Memory::kRamSize / 4);
    std::vector<u8> drawing_own_scratch(Memory::kScratchSize / 4);
    std::vector<u8> before12(Memory::kRamSize), before12_scratch(Memory::kScratchSize);
    std::map<u32, std::pair<u32, u32>> diff_addrs;
    struct Reader {
        int part = 0;
        u32 fn = 0, reads = 0;
    };
    std::map<std::pair<u32, int>, Reader> consumers;
    std::map<int, std::pair<int, int>> tally;
    std::map<int, int> supplied;
    std::map<int, int> shown;
    MachineState prev, cur;
    bool have_prev = false, live = false;
    bool result_live = false;
    u32 tick_sp = 0, first_fail_tick = 0;
    int fails = 0;
    bool machine_live = o.machine.empty();
    if (!o.machine.empty()) {
        std::string merr;
        if (!mach.load(o.machine, &merr)) {
            std::fprintf(stderr, "simdiff: %s\n", merr.c_str());
            return 2;
        }
        game.hw = &mach;
        mach.poll_input = [&rec](u32 vb, machine::PadInput p[2]) { rec.pads(vb, p); };
        std::printf("machine from %s: tick %u, field %u, checkpoint %u\n", o.machine.c_str(), mach.loaded_tick,
                    mach.loaded_field, mach.loaded_checkpoint);
    }
    const std::map<u32, u64> vram_hashes = o.vram.empty() ? std::map<u32, u64>{} : load_vram_hashes(o.vram);
    u32 vram_same = 0, vram_diff = 0, vram_first = 0;
    bool vram_dumped = false;
    if (!o.vram.empty()) {
        if (vram_hashes.empty()) std::fprintf(stderr, "simdiff: no VRAM hashes in %s\n", o.vram.c_str());
        mach.on_vblank = [&](u32 vb) {
            const auto it = vram_hashes.find(vb);
            if (it == vram_hashes.end() || vb < o.field_from || vb > o.field_to) return;
            const u64 h = hash_bytes(mach.gpu.vram.data(), mach.gpu.vram.size() * sizeof(u16));
            if (h == it->second) {
                ++vram_same;
            } else {
                if (!vram_diff) vram_first = vb;
                ++vram_diff;
            }
        };
    }
    std::FILE* wav = o.audio_dump.empty() ? nullptr : std::fopen(o.audio_dump.c_str(), "wb");
    u32 wav_frames = 0;
    auto put32 = [&](u32 v) { std::fwrite(&v, 4, 1, wav); };
    auto put16 = [&](u16 v) { std::fwrite(&v, 2, 1, wav); };
    if (wav) {
        std::fwrite("RIFF", 1, 4, wav);
        put32(0);
        std::fwrite("WAVEfmt ", 1, 8, wav);
        put32(16);
        put16(1);
        put16(2);
        put32(machine::Spu::kSampleRate);
        put32(machine::Spu::kSampleRate * 4);
        put16(4);
        put16(16);
        std::fwrite("data", 1, 4, wav);
        put32(0);
        mach.spu.submit = [&](const s16* p, int n) {
            std::fwrite(p, 4, static_cast<size_t>(n), wav);
            wav_frames += static_cast<u32>(n);
        };
    }
    bool booted = false;
    if (o.boot) {
        vfs::DiscImage img;
        Bytes exe;
        const auto e = img.open(o.disc) ? vfs::iso_find(img, "ACE3.BIN") : std::nullopt;
        if (!e || !img.read_bytes(e->lba, e->size, exe) || !game.load_exe(exe)) {
            std::fprintf(stderr, "simdiff: no ACE3.BIN on %s\n", o.disc.c_str());
            return 2;
        }
        game.hw = &mach;
        mach.poll_input = [&rec](u32 vb, machine::PadInput p[2]) { rec.pads(vb, p); };
        machine_live = true;
        const u32 first = game.boot();
        booted = true;
        std::printf("booted to field %u: the first phase function %08X, sp %08X\n", mach.vblank(), first, game.sp);
    }
    auto oracle_part = [&](int p, u32 target) {
        if (!machine_live) return true;
        if (p != 16 && !Game::part_ported(p, target)) return true;
        return std::find(o.oracle_parts.begin(), o.oracle_parts.end(), p) != o.oracle_parts.end();
    };
    auto step = [&](const MachineState& r) {
        if (!machine_live && !o.machine.empty() && r.checkpoint == mach.loaded_checkpoint &&
            (r.checkpoint == 20 ? r.tick == mach.loaded_tick && Game::is_flight_phase(r.target)
                                : r.field == mach.loaded_field)) {
            machine_live = true;
            if (mach.vblank() != r.field)
                std::printf("warning: the machine dump is at field %u, the state at field %u\n", mach.vblank(), r.field);
        }
        prev = r;
        have_prev = true;
    };
    while (file.next(cur)) {
        if (cur.checkpoint == 0 && cur.target == kTickFn) tick_sp = cur.regs[29] - 0x30u;
        if (!vram_dumped && !o.vram_dump_path.empty() && have_prev && prev.checkpoint == 30 &&
            prev.field >= o.vram_dump_field) {
            if (FILE* f = std::fopen(o.vram_dump_path.c_str(), "wb")) {
                std::fwrite(mach.gpu.vram.data(), sizeof(u16), mach.gpu.vram.size(), f);
                std::fclose(f);
                std::printf("VRAM at field %u written to %s\n", cur.field, o.vram_dump_path.c_str());
            }
            vram_dumped = true;
        }
        if (booted && !have_prev) {
            std::vector<Diff> diffs;
            const u32 hi = cur.regs[29] + 0x100u < 0x80200000u ? cur.regs[29] + 0x100u : 0x80200000u;
            diff_bytes("ram", 0x80000000u, cur.ram.data(), cur.ram.data(), mem.ram(), Memory::kRamSize,
                       cur.regs[29] - 0x4000u, hi, diffs);
            diff_bytes("scratch", Memory::kScratchBase, cur.scratch.data(), cur.scratch.data(), mem.scratch(),
                       Memory::kScratchSize, 0, 0, diffs);
            if (!o.skip_gte) {
                u32 eg[64];
                gte.save(eg);
                for (u32 r = 0; r < 64; ++r)
                    if (eg[r] != cur.gte[r]) diffs.push_back({std::string("gte ") + gte_name(r), r, cur.gte[r], cur.gte[r], eg[r]});
            }
            if (mach.vblank() != cur.field || game.sp != cur.regs[29])
                std::printf("the boot ends at field %u, sp %08X; the oracle's first record at field %u, sp %08X\n",
                            mach.vblank(), game.sp, cur.field, cur.regs[29]);
            auto& t = tally[0];
            if (diffs.empty() && !mem.faults()) {
                ++t.first;
            } else {
                ++t.second;
                if (!fails++) first_fail_tick = cur.tick;
                std::printf("the boot (part 0): %zu words differ%s\n", diffs.size(),
                            mem.faults() ? ", and the engine accessed unmapped memory" : "");
                for (int i = 0; i < static_cast<int>(diffs.size()) && i < o.show; ++i) {
                    const Diff& d = diffs[static_cast<size_t>(i)];
                    std::printf("  %-10s %08X  oracle %08X  engine %08X\n", d.where.c_str(), d.addr, d.oracle, d.engine);
                }
            }
            live = true;
            step(cur);
            continue;
        }
        if (have_prev && inside_call(prev, cur)) continue;
        const int part = have_prev ? part_between(prev, cur) : -1;
        const bool in_range = cur.tick >= o.tick_from && cur.tick <= o.tick_to && cur.field >= o.field_from &&
                              cur.field <= o.field_to;
        if (part < 0 || !in_range) {
            if (have_prev && part < 0 && in_range && machine_live && !o.machine.empty())
                std::printf("warning: no part runs from (%08X, %u) to (%08X, %u) at field %u\n", prev.target,
                            prev.checkpoint, cur.target, cur.checkpoint, cur.field);
            live = false;
            step(cur);
            continue;
        }
        if (!live) {
            std::memcpy(mem.ram(), prev.ram.data(), Memory::kRamSize);
            std::memcpy(mem.scratch(), prev.scratch.data(), Memory::kScratchSize);
            gte.load(prev.gte);
            std::fill(copied.begin(), copied.end(), u8{0});
            live = true;
        }
        mem.reset_faults();
        const bool is_frame_part = frame_part(part);
        if (oracle_part(part, prev.target)) {
            const bool selective = part == 12 && !o.draw_copy.empty();
            u8* ram = mem.ram();
            for (u32 off = 0; off < Memory::kRamSize; off += 4) {
                if (!std::memcmp(&cur.ram[off], &prev.ram[off], 4)) continue;
                const u32 a = 0x80000000u + off;
                if (in_ranges(o.drop, a)) continue;
                if (selective && !in_ranges(o.draw_copy, a)) {
                    drawing_own[off / 4] = 1;
                    continue;
                }
                std::memcpy(ram + off, &cur.ram[off], 4);
                copied[off / 4] = static_cast<u8>(part);
            }
            for (u32 off = 0; off < Memory::kScratchSize; off += 4) {
                if (!std::memcmp(&cur.scratch[off], &prev.scratch[off], 4)) continue;
                if (selective && !o.draw_scratch) drawing_own_scratch[off / 4] = 1;
                else std::memcpy(mem.scratch() + off, &cur.scratch[off], 4);
            }
            if (!selective || o.draw_gte) gte.load(cur.gte);
            ++supplied[part];
            if (part == 13 || part == 16) result_live = false;
            step(cur);
            continue;
        } else {
            game.sp = is_frame_part ? prev.regs[29] : tick_sp;
            game.reg_s4 = prev.regs[20];
            game.reg_s0 = prev.regs[16];
            game.reg_ra = prev.regs[31];
            if (part == 14) {
                if (o.machine.empty()) attach_machine(mach, game, rec, prev);
                if (!result_live) game.phase_result = prev.regs[2];
            }
            if (o.consumers) {
                std::fill(access.begin(), access.end(), u8{0});
                mem.set_access_log(&access);
                mem.set_access_tags(&tags, &game.last_covered);
            }
            if (o.stack_sync && frame_part(part)) {
                const u32 lo = (prev.regs[29] - 0x4000u) & 0x1FFFFFu, hi = (prev.regs[29] + 0x100u) & 0x1FFFFFu;
                if (lo < hi && hi <= Memory::kRamSize) std::memcpy(mem.ram() + lo, prev.ram.data() + lo, hi - lo);
            }
            if (part == 12) {
                std::memcpy(before12.data(), mem.ram(), Memory::kRamSize);
                std::memcpy(before12_scratch.data(), mem.scratch(), Memory::kScratchSize);
            }
            game.part_target = prev.target;
            if (part == 16) game.run_phase(prev.target);
            else game.run_part(part);
            if (part == 13 || part == 16) result_live = true;
            mem.set_access_log(nullptr);
            mem.set_access_tags(nullptr, nullptr);
            if (o.consumers) {
                for (u32 w = 0; w < Memory::kRamSize / 4; ++w) {
                    if (!copied[w]) continue;
                    const u8* a = &access[4 * w];
                    bool read_first = false, written = false;
                    for (u32 b = 0; b < 4; ++b) {
                        if (a[b] == Memory::kReadFirst) read_first = true;
                        if (a[b] == Memory::kWroteFirst) written = true;
                    }
                    if (read_first) {
                        Reader& c = consumers[{0x80000000u + 4 * w, copied[w]}];
                        if (!c.reads++) {
                            c.part = part;
                            for (u32 b = 0; b < 4; ++b)
                                if (a[b] == Memory::kReadFirst) {
                                    c.fn = tags[4 * w + b];
                                    break;
                                }
                        }
                    }
                    if (written && !read_first) copied[w] = 0;
                }
            }
        }
        if (part == 12 && !oracle_part(part, prev.target) && o.draw_oracle) {
            for (u32 off = 0; off < Memory::kRamSize; off += 4)
                if (!std::memcmp(mem.ram() + off, &before12[off], 4) && std::memcmp(&cur.ram[off], &prev.ram[off], 4))
                    std::memcpy(mem.ram() + off, &cur.ram[off], 4);
        }
        if (part == 12 && !oracle_part(part, prev.target) && !o.draw_full) {
            for (u32 off = 0; off < Memory::kRamSize; off += 4)
                if (!std::memcmp(mem.ram() + off, &before12[off], 4) && std::memcmp(&cur.ram[off], &prev.ram[off], 4))
                    drawing_own[off / 4] = 1;
            for (u32 off = 0; off < Memory::kScratchSize; off += 4)
                if (!std::memcmp(mem.scratch() + off, &before12_scratch[off], 4) &&
                    std::memcmp(&cur.scratch[off], &prev.scratch[off], 4))
                    drawing_own_scratch[off / 4] = 1;
        }
        std::vector<Diff> diffs;
        const u32 stack_hi = is_frame_part ? prev.regs[29] + 0x100u : tick_sp;
        const u32 stack_lo = is_frame_part ? prev.regs[29] - 0x4000u : tick_sp - kStackWindow;
        diff_bytes("ram", 0x80000000u, prev.ram.data(), cur.ram.data(), mem.ram(), Memory::kRamSize, stack_lo,
                   stack_hi < 0x80200000u ? stack_hi : 0x80200000u, diffs);
        diffs.erase(std::remove_if(diffs.begin(), diffs.end(),
                                   [&](const Diff& d) {
                                       if (d.where != "ram") return false;
                                       if (!o.draw_full)
                                           for (const auto& [lo, hi] : kRenderBuffers)
                                               if (d.addr >= lo && d.addr < hi) return true;
                                       if (d.addr >= tick_sp - 0x4000u && d.addr < 0x80200000u) return true;
                                       if (drawing_own[(d.addr & 0x1FFFFFu) / 4]) return true;
                                       return in_ranges(o.drop, d.addr);
                                   }),
                    diffs.end());
        diff_bytes("scratch", Memory::kScratchBase, prev.scratch.data(), cur.scratch.data(), mem.scratch(),
                   Memory::kScratchSize, 0, 0, diffs);
        diffs.erase(std::remove_if(diffs.begin(), diffs.end(),
                                   [&](const Diff& d) {
                                       return d.where == "scratch" &&
                                              drawing_own_scratch[(d.addr - Memory::kScratchBase) / 4];
                                   }),
                    diffs.end());
        if (!o.skip_gte) {
            u32 eg[64];
            gte.save(eg);
            for (u32 r = 0; r < 64; ++r)
                if (eg[r] != cur.gte[r]) diffs.push_back({std::string("gte ") + gte_name(r), r, prev.gte[r], cur.gte[r], eg[r]});
        }
        auto& t = tally[part];
        if (diffs.empty() && !mem.faults()) {
            ++t.first;
        } else {
            ++t.second;
            for (const Diff& d : diffs) {
                auto& a = diff_addrs[d.addr];
                if (!a.first++) a.second = cur.tick;
            }
            if (!fails++) first_fail_tick = cur.tick;
            if (shown[part]++ < 3 || o.verbose) {
                std::printf("tick %u (field %u) part %d: %zu words differ%s\n", cur.tick, cur.field, part, diffs.size(),
                            mem.faults() ? ", and the engine accessed unmapped memory" : "");
                for (int i = 0; i < static_cast<int>(diffs.size()) && i < o.show; ++i) {
                    const Diff& d = diffs[static_cast<size_t>(i)];
                    std::printf("  %-10s %08X  before %08X  oracle %08X  engine %08X\n", d.where.c_str(), d.addr,
                                d.before, d.oracle, d.engine);
                }
            }
            if (o.resync) {
                std::memcpy(mem.ram(), cur.ram.data(), Memory::kRamSize);
                std::memcpy(mem.scratch(), cur.scratch.data(), Memory::kScratchSize);
                gte.load(cur.gte);
            }
        }
        step(cur);
    }
    if (!file.error().empty()) std::fprintf(stderr, "state file: %s\n", file.error().c_str());
    if (wav) {
        std::fseek(wav, 4, SEEK_SET);
        put32(36u + wav_frames * 4u);
        std::fseek(wav, 40, SEEK_SET);
        put32(wav_frames * 4u);
        std::fclose(wav);
        std::printf("audio: wrote %u frames\n", wav_frames);
    }
    std::printf("\nchained frames (exact, different, the oracle's):\n");
    for (int p : kPartList) {
        const auto t = tally[p];
        if (t.first || t.second || supplied[p])
            std::printf("  part %2d: %d exact, %d different, %d the oracle's\n", p, t.first, t.second, supplied[p]);
    }
    if (fails) std::printf("first difference at tick %u\n", first_fail_tick);
    if (!o.vram.empty()) {
        std::printf("VRAM at the fields' ends: %u as the oracle's, %u different", vram_same, vram_diff);
        if (vram_diff) std::printf(" (the first at field %u)", vram_first);
        std::printf("\n");
    }
    if (!diff_addrs.empty()) {
        std::printf("\nwords that differed (word, parts, first tick):\n");
        int n = 0;
        for (const auto& [addr, v] : diff_addrs)
            if (n++ < 64) std::printf("  %08X  %u  %u\n", addr, v.first, v.second);
        if (n > 64) std::printf("  ... %d words in all\n", n);
    }
    if (o.consumers) {
        std::printf("\nwords an oracle part changed that a later engine part read first\n"
                    "(word, by part, first reading part and function, reads):\n");
        for (const auto& [k, v] : consumers)
            std::printf("  %08X  %2d  %2d %08X  %u\n", k.first, k.second, v.part, v.fn, v.reads);
    }
    if (o.coverage) {
        std::printf("\ncoverage (reimplemented functions, calls):\n");
        for (const auto& [addr, n] : game.coverage) std::printf("  %08X %llu\n", addr, static_cast<unsigned long long>(n));
    }
    if (game.unported_hits) std::printf("\n%u calls reached code not reimplemented yet\n", game.unported_hits);
    return fails ? 1 : 0;
}
}

int main(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const char* s = argv[i];
        if (!std::strcmp(s, "--part") && i + 1 < argc) o.part = std::atoi(argv[++i]);
        else if (!std::strcmp(s, "--ticks") && i + 1 < argc) {
            char* end;
            o.tick_from = static_cast<u32>(std::strtoul(argv[++i], &end, 10));
            o.tick_to = *end == '-' ? static_cast<u32>(std::strtoul(end + 1, nullptr, 10)) : o.tick_from;
        } else if (!std::strcmp(s, "--show") && i + 1 < argc) o.show = std::atoi(argv[++i]);
        else if (!std::strcmp(s, "--verbose")) o.verbose = true;
        else if (!std::strcmp(s, "--coverage")) o.coverage = true;
        else if (!std::strcmp(s, "--whole")) o.mode = 1;
        else if (!std::strcmp(s, "--chain")) o.mode = 2;
        else if (!std::strcmp(s, "--reads")) o.reads = true;
        else if (!std::strcmp(s, "--stale")) o.stale = true;
        else if (!std::strcmp(s, "--keep-gte")) o.keep_gte = true;
        else if (!std::strcmp(s, "--frames")) o.mode = 3;
        else if (!std::strcmp(s, "--consumers")) o.consumers = true;
        else if (!std::strcmp(s, "--resync")) o.resync = true;
        else if (!std::strcmp(s, "--no-gte")) o.skip_gte = true;
        else if (!std::strcmp(s, "--oracle-part") && i + 1 < argc) o.oracle_parts.push_back(std::atoi(argv[++i]));
        else if (!std::strcmp(s, "--draw-scratch")) o.draw_scratch = true;
        else if (!std::strcmp(s, "--input") && i + 1 < argc) o.input = argv[++i];
        else if (!std::strcmp(s, "--machine") && i + 1 < argc) o.machine = argv[++i];
        else if (!std::strcmp(s, "--disc") && i + 1 < argc) o.disc = argv[++i];
        else if (!std::strcmp(s, "--fields") && i + 1 < argc) {
            char* end;
            o.field_from = static_cast<u32>(std::strtoul(argv[++i], &end, 10));
            o.field_to = *end == '-' ? static_cast<u32>(std::strtoul(end + 1, nullptr, 10)) : o.field_from;
        }
        else if (!std::strcmp(s, "--draw-gte")) o.draw_gte = true;
        else if (!std::strcmp(s, "--vram") && i + 1 < argc) o.vram = argv[++i];
        else if (!std::strcmp(s, "--draw-oracle")) o.draw_oracle = true;
        else if (!std::strcmp(s, "--draw-full")) o.draw_full = true;
        else if (!std::strcmp(s, "--boot")) o.boot = true;
        else if (!std::strcmp(s, "--audio-dump") && i + 1 < argc) o.audio_dump = argv[++i];
        else if (!std::strcmp(s, "--stack-sync")) o.stack_sync = true;
        else if (!std::strcmp(s, "--cards") && i + 1 < argc) o.cards = argv[++i];
        else if (!std::strcmp(s, "--vram-dump") && i + 2 < argc) {
            o.vram_dump_field = static_cast<u32>(std::strtoul(argv[++i], nullptr, 10));
            o.vram_dump_path = argv[++i];
        }
        else if (!std::strcmp(s, "--draw-copy") && i + 1 < argc) {
            std::pair<u32, u32> r;
            if (!parse_range(argv[i + 1], r)) {
                std::fprintf(stderr, "simdiff: bad range %s (want LO-HI in hex)\n", argv[i + 1]);
                return 2;
            }
            o.draw_copy.push_back(r);
            ++i;
        }
        else if (!std::strcmp(s, "--drop") && i + 1 < argc) {
            std::pair<u32, u32> r;
            if (!parse_range(argv[i + 1], r)) {
                std::fprintf(stderr, "simdiff: bad range %s (want LO-HI in hex)\n", argv[i + 1]);
                return 2;
            }
            o.drop.push_back(r);
            ++i;
        }
        else if ((!std::strcmp(s, "--keep") || !std::strcmp(s, "--scrub")) && i + 1 < argc) {
            std::pair<u32, u32> r;
            if (!parse_range(argv[i + 1], r)) {
                std::fprintf(stderr, "simdiff: bad range %s (want LO-HI in hex)\n", argv[i + 1]);
                return 2;
            }
            (s[2] == 'k' ? o.keep : o.scrub).push_back(r);
            ++i;
        }
        else if (s[0] != '-' && o.path.empty()) o.path = s;
        else {
            std::fprintf(stderr, "usage: simdiff <state.bin> [--part K] [--ticks A-B] [--show N] [--verbose] [--coverage]\n");
            return 2;
        }
    }
    if (o.path.empty()) {
        std::fprintf(stderr, "usage: simdiff <state.bin> [--part K] [--ticks A-B] [--show N] [--verbose] [--coverage]\n");
        return 2;
    }

    if (o.mode == 3) return run_frames(o);
    if (o.mode) return run_ticks(o);

    StateFile file;
    std::string err;
    if (!file.open(o.path, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 2;
    }

    Memory mem;
    Gte gte;
    Game game(mem, gte);
    game.track_coverage = o.coverage;
    machine::Machine mach(mem);
    mach.call_guest = [&game](u32 fn, u32 a0, u32 a1) { game.call_guest(fn, a0, a1); };
    if (!o.cards.empty()) {
        mach.card.set_path(0, o.cards + "/card1.mcd", false);
        mach.card.set_path(1, o.cards + "/card2.mcd", false);
    }
    if (!o.disc.empty()) {
        std::string derr;
        if (!mach.mount(o.disc, {}, &derr)) {
            std::fprintf(stderr, "simdiff: %s\n", derr.c_str());
            return 2;
        }
    }
    InputRecording rec;
    if (!o.input.empty() && !rec.load(o.input)) {
        std::fprintf(stderr, "simdiff: cannot read %s\n", o.input.c_str());
        return 2;
    }

    MachineState prev, cur;
    bool have_prev = false;
    std::map<int, std::pair<int, int>> tally;
    std::map<int, u64> activity;
    std::map<int, int> shown;
    u32 ticks_seen = 0;
    u32 tick_sp = 0;

    while (file.next(cur)) {
        if (cur.checkpoint == 0 && cur.target == kTickFn) {
            ++ticks_seen;
            tick_sp = cur.regs[29] - 0x30u;
        }
        if (have_prev && inside_call(prev, cur)) continue;
        const bool in_range = cur.tick >= o.tick_from && cur.tick <= o.tick_to && cur.field >= o.field_from &&
                              cur.field <= o.field_to;
        if (have_prev && in_range) {
            const int part = part_between(prev, cur);
            const bool is_ported = ported(part, prev);
            if (part > 0 && (o.part == 0 || o.part == part) && is_ported) {
                std::memcpy(mem.ram(), prev.ram.data(), Memory::kRamSize);
                std::memcpy(mem.scratch(), prev.scratch.data(), Memory::kScratchSize);
                gte.load(prev.gte);
                mem.reset_faults();
                const bool is_frame_part = frame_part(part);
                game.sp = is_frame_part ? prev.regs[29] : tick_sp;
                game.reg_s4 = prev.regs[20];
                game.reg_s0 = prev.regs[16];
                game.reg_ra = prev.regs[31];
                game.part_target = prev.target;
                if (part == 14) attach_machine(mach, game, rec, prev);
                if (part == 16) {
                    attach_machine(mach, game, rec, prev);
                    game.run_phase(prev.target);
                } else {
                    game.run_part(part);
                }

                std::vector<Diff> diffs;
                const u32 stack_hi = is_frame_part ? prev.regs[29] + 0x100u : tick_sp;
                const u32 stack_lo = is_frame_part ? prev.regs[29] - 0x4000u : tick_sp - kStackWindow;
                diff_bytes("ram", 0x80000000u, prev.ram.data(), cur.ram.data(), mem.ram(), Memory::kRamSize, stack_lo,
                           stack_hi < 0x80200000u ? stack_hi : 0x80200000u, diffs);
                if (is_frame_part)
                    diffs.erase(std::remove_if(diffs.begin(), diffs.end(),
                                               [](const Diff& d) {
                                                   for (const auto& [lo, hi] : kRenderBuffers)
                                                       if (d.where == "ram" && d.addr >= lo && d.addr < hi) return true;
                                                   return false;
                                               }),
                                diffs.end());
                diff_bytes("scratch", Memory::kScratchBase, prev.scratch.data(), cur.scratch.data(), mem.scratch(),
                           Memory::kScratchSize, 0, 0, diffs);
                if (part == 12) {
                    diffs.erase(std::remove_if(diffs.begin(), diffs.end(),
                                               [](const Diff& d) { return d.engine == d.before; }),
                                diffs.end());
                } else {
                    u32 eg[64];
                    gte.save(eg);
                    for (u32 r = 0; r < 64; ++r)
                        if (eg[r] != cur.gte[r]) diffs.push_back({std::string("gte ") + gte_name(r), r, prev.gte[r], cur.gte[r], eg[r]});
                }

                activity[part] += cur.changed;
                auto& t = tally[part];
                if (diffs.empty() && !mem.faults()) {
                    ++t.first;
                    if (o.verbose) std::printf("tick %u part %d: ok\n", cur.tick, part);
                } else {
                    ++t.second;
                    if (shown[part]++ < 3 || o.verbose) {
                        std::printf("tick %u (field %u) part %d: %zu words differ%s\n", cur.tick, cur.field, part,
                                    diffs.size(), mem.faults() ? ", and the engine accessed unmapped memory" : "");
                        for (int i = 0; i < static_cast<int>(diffs.size()) && i < o.show; ++i) {
                            const Diff& d = diffs[static_cast<size_t>(i)];
                            std::printf("  %-10s %08X  before %08X  oracle %08X  engine %08X\n", d.where.c_str(), d.addr,
                                        d.before, d.oracle, d.engine);
                        }
                        if (mem.faults()) std::printf("  first unmapped access at %08X\n", mem.first_fault());
                    }
                }
            }
        }
        prev = cur;
        have_prev = true;
    }
    if (!file.error().empty()) std::fprintf(stderr, "state file: %s\n", file.error().c_str());

    std::printf("\n%u ticks in %s\n", ticks_seen, o.path.c_str());
    int failed = 0;
    for (int p : kPartList) {
        if (p != 16 && !Game::part_ported(p)) {
            std::printf("  part %2d: not reimplemented yet\n", p);
            continue;
        }
        const auto t = tally[p];
        std::printf("  part %2d: %d exact, %d different   (the oracle's part changed %llu RAM words in all)\n", p,
                    t.first, t.second, static_cast<unsigned long long>(activity[p]));
        failed += t.second;
    }
    if (o.coverage) {
        std::printf("\ncoverage (reimplemented functions, calls):\n");
        for (const auto& [addr, n] : game.coverage) std::printf("  %08X %llu\n", addr, static_cast<unsigned long long>(n));
    }
    if (game.unported_hits) std::printf("\n%u calls reached code not reimplemented yet\n", game.unported_hits);
    return failed ? 1 : 0;
}
