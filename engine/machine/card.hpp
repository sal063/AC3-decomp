#pragma once

#include "core/types.hpp"

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace ac3::sim {
class Memory;
}

namespace ac3::machine {
class Card {
public:
    static constexpr u32 kFrameSize = 128, kFrames = 1024, kSize = kFrameSize * kFrames;

    explicit Card(sim::Memory& m);

    std::function<void(u32 cls, u32 spec)> deliver_event;

    void set_path(int slot, const std::string& path, bool persist);

    void InitCARD();
    u32 StartCARD();
    u32 StopCARD();
    u32 card_info(u32 chan);
    u32 card_load(u32 chan);
    u32 card_status(u32 drv);
    u32 card_wait(u32 drv);
    u32 card_read(u32 chan, u32 frame, u32 dst);
    u32 card_write(u32 chan, u32 frame, u32 src);
    u32 card_clear(u32 chan);
    u32 card_format(u32 chan);
    u32 card_chan() const { return last_chan_; }
    u32 card_auto(u32 on);

    u32 open(u32 path, u32 flags);
    u32 close(u32 fd);
    u32 lseek(u32 fd, u32 off, u32 whence);
    u32 read(u32 fd, u32 dst, u32 n);
    u32 write(u32 fd, u32 src, u32 n);
    u32 firstfile(u32 path, u32 out);
    u32 nextfile(u32 out);

private:
    struct Fd {
        bool used = false;
        int slot = 0, first = 0;
        u32 pos = 0, size = 0;
        bool async = false;
    };

    void ensure(int slot);
    void flush(int slot);
    bool formatted(int slot) const;
    void format_image(int slot);
    u8* frame_at(int slot, int frame) { return data_[slot].data() + frame * kFrameSize; }
    const u8* frame_at(int slot, int frame) const { return data_[slot].data() + frame * kFrameSize; }
    int dir_find(int slot, const std::string& name) const;
    int dir_create(int slot, const std::string& name, int blocks);
    int block_step(int slot, int first, u32 index) const;
    bool path_split(u32 addr, int* slot, std::string* name) const;
    int scan_next(int slot, const std::string& pat, int from) const;
    void fill_direntry(u32 out, int slot, int block);
    u32 file_io(Fd& f, u32 guest, u32 len, bool writing);
    void file_done(const Fd& f, bool ok);
    void done(u32 chan, u32 cls, u32 spec);
    Fd* fd_get(u32 fd);

    sim::Memory& m;
    std::array<std::vector<u8>, 2> data_;
    std::array<std::string, 2> path_ = {"saves/card1.mcd", "saves/card2.mcd"};
    std::array<bool, 2> persist_ = {true, true}, loaded_ = {false, false}, unconfirmed_ = {false, false},
                        flush_failed_ = {false, false};
    bool started_ = false;
    u32 last_chan_ = 0, auto_ = 1;
    Fd fds_[8];
    int scan_slot_ = 0, scan_block_ = 0;
    std::string scan_pattern_;
};
}
