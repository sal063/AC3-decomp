#!/usr/bin/env python3
import argparse
import struct
import sys

def records(path):
    with open(path, "rb") as f:
        if f.read(4) != b"AC3T":
            sys.exit(f"{path}: not an oracle ticks.bin")
        f.read(4)
        while True:
            head = f.read(20)
            if len(head) < 20:
                return
            tick, field, fn, when, nblocks = struct.unpack("<5I", head)
            blocks = {}
            for _ in range(nblocks):
                bid, addr, length = struct.unpack("<3I", f.read(12))
                blocks[bid] = (addr, f.read(length))
            yield tick, field, fn, when, blocks

def camera(world):
    rot = [v / 4096.0 for v in struct.unpack_from("<9h", world, 76)]
    pos = [v / 4096.0 for v in struct.unpack_from("<3i", world, 188)]
    h = struct.unpack_from("<h", world, 220)[0]
    return rot, pos, h

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("trace")
    ap.add_argument("field", type=int)
    ap.add_argument("--when", choices=["pre", "post"], default="post")
    a = ap.parse_args()
    want = 1 if a.when == "post" else 0
    best = None
    for tick, field, fn, when, blocks in records(a.trace + "/ticks.bin"):
        if when != want or 2 not in blocks or not blocks[2][1]:
            continue
        if best is None or abs(field - a.field) < abs(best[1] - a.field):
            best = (tick, field, blocks[2][1])
    if not best:
        sys.exit("no tick with a world object in the trace")
    rot, pos, h = camera(best[2])
    print(" ".join(f"{v:.6f}" for v in rot), " ".join(f"{v:.3f}" for v in pos), h)
    print(f"# tick {best[0]}, field {best[1]}", file=sys.stderr)

if __name__ == "__main__":
    sys.exit(main())
