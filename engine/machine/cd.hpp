#pragma once

#include "core/types.hpp"
#include "vfs/disc_image.hpp"

#include <functional>
#include <string>
#include <vector>

namespace ac3::sim {
class Memory;
}

namespace ac3::machine {
class Spu;
struct StateReader;

class Xa {
public:
    explicit Xa(Spu& spu) : spu_(spu) {}
    int sector(const u8* sector);
    void reset();
    void set_filter(int active, u8 file, u8 chan);
    void set_enabled(int on);
    int enabled() const { return rt_enabled_; }
    void set_muted(int on) { muted_ = on; }
    void stop();
    bool load_state(StateReader& r);

private:
    void resample_push(const s16* l, const s16* r, int n, u32 src_rate);

    Spu& spu_;
    int filter_active_ = 0;
    u8 filter_file_ = 0, filter_chan_ = 0;
    int rt_enabled_ = 0, muted_ = 0;
    s32 hist_[2][2] = {};
    s32 res_pos_ = 0;
    s16 res_last_[2] = {};
    int res_have_last_ = 0;
    u32 res_rate_ = 0;
    std::vector<s16> pcm_[2];
};

class Cd {
public:
    static constexpr u32 kWhole = 2340;

    Cd(sim::Memory& m, Spu& spu);

    std::function<void(u32 fn, u32 a0, u32 a1)> call_guest;
    std::function<void(u32 cls, u32 spec)> deliver_event;

    std::vector<std::string> translation_dirs;

    bool mount(const std::string& booted, const std::string& other, std::string* error);
    bool english(int which) const { return discs_[which & 1].disc.translation() != nullptr; }
    bool read_file(int which, const std::string& name, std::vector<u8>& out);
    bool pack_follow(int number);

    u32 CdInit();
    u32 CdControl(u32 com, u32 param, u32 result);
    u32 CdControlB(u32 com, u32 param, u32 result);
    u32 CdControlF(u32 com, u32 param);
    u32 CdSync(u32 mode, u32 result);
    u32 CdReady(u32 mode, u32 result);
    u32 CD_ready(u32 result);
    u32 CdSyncCallback(u32 fn);
    u32 CdReadyCallback(u32 fn);
    u32 CdDataCallback(u32 fn);
    u32 CdGetSector(u32 dst, u32 words);
    u32 CdSearchFile(u32 fp, u32 name);
    s32 CdPosToInt(u32 loc);
    u32 CdIntToPos(s32 lba, u32 loc);
    u32 CdRead(u32 sectors, u32 dst, u32 mode);
    u32 CdReadSync(u32 mode, u32 result);
    u32 CdReset(u32 mode);
    u32 CdMix(u32) { return 1; }

    void tick();
    int debt_frames() const;
    void stream_ahead();

    bool load_state(StateReader& r);
    Xa& xa() { return xa_; }

    u32 both_discs() const { return merged_index_size_ ? static_cast<u32>(discs_[0].number) : 0u; }

private:
    struct Image {
        vfs::DiscImage disc;
        u32 sectors = 0, base = 0;
        int number = 0;
        bool open() const { return disc.is_open(); }
    };

    bool image_read(Image& d, u32 rel, u8* out);
    Image* image_for(u32 lba);
    bool read_sector_whole(u32 lba, u8* out);
    u32 delivery_offset() const;
    int fifo_step();
    int fifo_fill();
    u8 drive_status() const;
    void write_status(u32 result);
    void invoke_cb(u32 fn, u32 intr, u32 result);
    void command(u32 com, u32 param, u32 result);
    int delivery_rate() const;
    int deliver_one();
    bool fifo_ends_record() const;
    u32 drive_sectors() const;
    int read_index(vfs::DiscImage& d, const char* index, const char* pack, u32 code, std::vector<u8>& buf, u32* lba,
                   u32* size, u32* pack_lba);
    bool merge_indices();
    void split_packs();
    void pack_serve();
    void pack_note_read(u32 lba);

    sim::Memory& m;
    Xa xa_;
    Image discs_[2];
    std::vector<u8> merged_index_;
    u32 merged_index_lba_ = 0, merged_index_size_ = 0;
    enum PackKind : u8 { kPackSame, kPackOnlyBooted, kPackOnlyOther, kPackBoth };
    std::vector<u8> pack_index_[2], pack_merged_, pack_kind_;
    u32 pack_index_lba_ = 0, pack_index_size_ = 0, pack_lba_ = 0;
    int pack_disc_ = 0;

    int streaming_ = 0;
    u32 cur_lba_ = 0, cd_mode_ = 0;
    int filter_active_ = 0;
    u8 filter_file_ = 0, filter_chan_ = 0;
    u8 consumed_chan_ = 0;
    int consumed_known_ = 0;
    int sector_credit_ = 0;
    int rate_override_ = 0;
    u8 fifo_[kWhole] = {};
    u32 fifo_lba_ = 0;
    int fifo_valid_ = 0;
    u32 fifo_pos_ = 0;
    u32 sync_callback_ = 0, ready_callback_ = 0, data_callback_ = 0;
};
}
