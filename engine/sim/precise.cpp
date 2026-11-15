#include "sim/precise.hpp"

#include <algorithm>

namespace ac3::sim {
PreciseCache::PreciseCache() {
    tables_[0].resize(kSize);
    tables_[1].resize(kSize);
    packets_[0].resize(kPacketSize);
    packets_[1].resize(kPacketSize);
    for (u32 t = 0; t < 2; ++t) {
        lits_[t].reserve(kLits);
        lights_[t].reserve(kLights);
    }
    edge_.reserve(kEdgeMax);
}

void PreciseCache::record(u32 key, const PreciseVertex& v) {
    ++projections_;
    if (edge_key(key)) {
        for (Slot& s : edge_)
            if (s.key == key && same_point(s.v, v)) {
                s.gen = gen_;
                s.v = v;
                return;
            }
        if (edge_.size() < kEdgeMax) edge_.push_back(Slot{key, gen_, v});
        return;
    }
    std::vector<Slot>& t = tables_[gen_ & 1u];
    u32 i = hash(key);
    for (u32 n = 0; n < kSize; ++n, i = (i + 1) & (kSize - 1)) {
        Slot& s = t[i];
        if (s.gen != gen_) {
            s.key = key;
            s.gen = gen_;
            s.v = v;
            return;
        }
        if (s.key == key && same_point(s.v, v)) {
            s.v = v;
            return;
        }
    }
}

int PreciseCache::lookup_all(u32 key, PreciseVertex* out, int max) const {
    int n = 0;
    if (max <= 0) return 0;
    if (edge_key(key)) {
        for (u32 age = 0; age < 2 && n < max; ++age) {
            const u32 g = gen_ - age;
            if (!g) break;
            for (auto s = edge_.rbegin(); s != edge_.rend() && n < max; ++s)
                if (s->key == key && s->gen == g) out[n++] = s->v;
        }
        return n;
    }
    for (u32 age = 0; age < 2 && n < max; ++age) {
        const u32 g = gen_ - age;
        if (!g) break;
        const std::vector<Slot>& t = tables_[g & 1u];
        constexpr int kChain = 64;
        const Slot* found[kChain];
        int nf = 0;
        u32 i = hash(key);
        for (u32 k = 0; k < kSize; ++k, i = (i + 1) & (kSize - 1)) {
            const Slot& s = t[i];
            if (s.gen != g) break;
            if (s.key != key) continue;
            if (nf == kChain) {
                std::copy(found + 1, found + kChain, found);
                --nf;
            }
            found[nf++] = &s;
        }
        while (nf > 0 && n < max) out[n++] = found[--nf]->v;
    }
    return n;
}

namespace {
u32 packet_hash(u32 addr, u32 bits) { return ((addr >> 2) * 0x9E3779B1u) >> (32 - bits); }
}

int PreciseCache::add_light(const Light& l) {
    std::vector<Light>& t = lights_[gen_ & 1u];
    if (t.size() >= kLights) return -1;
    t.push_back(l);
    return static_cast<int>(t.size() - 1);
}

void PreciseCache::record_packet(u32 addr, const Packet& p, const Lit* lit) {
    addr &= 0x00FFFFFFu;
    const u32 table = gen_ & 1u;
    std::vector<PacketSlot>& t = packets_[table];
    if (packet_count_[table] >= kPacketSize / 2) return;
    Packet kept = p;
    kept.lit = 0;
    if (lit && lit->light < lights_[table].size() && lits_[table].size() < kLits) {
        lits_[table].push_back(*lit);
        kept.lit = static_cast<u16>(lits_[table].size());
    }
    for (u32 i = packet_hash(addr, kPacketBits), n = 0; n < kPacketSize; ++n, i = (i + 1) & (kPacketSize - 1)) {
        PacketSlot& s = t[i];
        if (s.gen != gen_) {
            s.key = addr;
            s.gen = gen_;
            s.p = kept;
            ++packet_count_[table];
            return;
        }
        if (s.key == addr) {
            s.p = kept;
            return;
        }
    }
}

const PreciseCache::Packet* PreciseCache::lookup_packet(u32 addr, const Lit** lit, const Light** light) const {
    if (lit) *lit = nullptr;
    if (light) *light = nullptr;
    addr &= 0x00FFFFFFu;
    for (u32 age = 0; age < 2; ++age) {
        const u32 g = gen_ - age;
        if (!g) break;
        const std::vector<PacketSlot>& t = packets_[g & 1u];
        if (!packet_count_[g & 1u]) continue;
        for (u32 i = packet_hash(addr, kPacketBits), n = 0; n < kPacketSize; ++n, i = (i + 1) & (kPacketSize - 1)) {
            const PacketSlot& s = t[i];
            if (s.gen != g) break;
            if (s.key == addr) {
                if (lit && light && s.p.lit) {
                    const Lit& l = lits_[g & 1u][s.p.lit - 1u];
                    *lit = &l;
                    *light = &lights_[g & 1u][l.light];
                }
                return &s.p;
            }
        }
    }
    return nullptr;
}

void PreciseCache::next_frame() {
    ++gen_;
    projections_ = 0;
    packet_count_[gen_ & 1u] = 0;
    lits_[gen_ & 1u].clear();
    lights_[gen_ & 1u].clear();
    edge_.erase(std::remove_if(edge_.begin(), edge_.end(), [&](const Slot& s) { return gen_ - s.gen > 1; }),
                edge_.end());
}
}
