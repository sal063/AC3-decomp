#pragma once

#include "core/types.hpp"

#include <vector>

namespace ac3::sim {
struct PreciseVertex {
    float sx = 0, sy = 0;
    float vx = 0, vy = 0, vz = 0;
};

class PreciseCache {
public:
    PreciseCache();

    void record(u32 key, const PreciseVertex& v);
    int lookup_all(u32 key, PreciseVertex* out, int max) const;
    bool lookup(u32 key, PreciseVertex& out) const { return lookup_all(key, &out, 1) == 1; }
    static bool edge_key(u32 key) {
        const s16 x = static_cast<s16>(key & 0xFFFFu), y = static_cast<s16>(key >> 16);
        return x <= -1024 || x >= 1023 || y <= -1024 || y >= 1023;
    }
    void next_frame();

    struct Packet {
        PreciseVertex v[4];
        u8 ok = 0;
        u8 fx = 0;
        u16 lit = 0;
        u8 depth_shift = 0;
    };
    struct Light {
        float dir[3][3];
        float colour[3][3];
        float back[3];
        float up[3];
        float horizon[3];
    };
    struct Lit {
        float n[4][3];
        u16 light = 0;
    };
    int add_light(const Light& l);
    void record_packet(u32 addr, const Packet& p, const Lit* lit = nullptr);
    const Packet* lookup_packet(u32 addr, const Lit** lit = nullptr, const Light** light = nullptr) const;

    u32 projections() const { return projections_; }

    struct Water {
        float rot[3][3];
        double eye[3];
        float sun[3];
        float sun_colour[3];
        float back[3];
        float horizon[3];
        float sky[4][3];
        float sky_tan[3];
        float time = 0;
        u8 sky_age = 255;
        bool valid = false;
    };
    Water water;

    struct Shore {
        static constexpr u32 kCells = 8, kSide = 256 * kCells;
        std::vector<u8> cells;
        u32 key[3] = {};
        u32 generation = 0;
    };
    Shore shore;

private:
    static constexpr u32 kBits = 15, kSize = 1u << kBits;
    static constexpr u32 kEdgeMax = 2048;
    struct Slot {
        u32 key = 0, gen = 0;
        PreciseVertex v;
    };
    static u32 hash(u32 key) { return (key * 0x9E3779B1u) >> (32 - kBits); }
    static bool same_point(const PreciseVertex& a, const PreciseVertex& b) {
        const float dx = a.vx - b.vx, dy = a.vy - b.vy, dz = a.vz - b.vz;
        return dx * dx + dy * dy + dz * dz < 0.25f;
    }
    std::vector<Slot> tables_[2];
    std::vector<Slot> edge_;
    struct PacketSlot {
        u32 key = 0, gen = 0;
        Packet p;
    };
    static constexpr u32 kPacketBits = 18, kPacketSize = 1u << kPacketBits;
    std::vector<PacketSlot> packets_[2];
    u32 packet_count_[2] = {};
    static constexpr u32 kLits = 16384, kLights = 1024;
    std::vector<Lit> lits_[2];
    std::vector<Light> lights_[2];
    u32 gen_ = 1;
    u32 projections_ = 0;
};
}
