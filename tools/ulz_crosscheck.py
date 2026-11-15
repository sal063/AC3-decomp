#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ac3cat", default=os.path.join(HERE, "build", "release", "ac3cat.exe"))
    ap.add_argument("--disc", required=True)
    ap.add_argument("--unpack", required=True)
    a = ap.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        r = subprocess.run([a.ac3cat, "--disc", a.disc, "dump-ulz", tmp], capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit(r.stderr)
        print(r.stdout.strip())
        names = [line.split("\t")[0] for line in open(os.path.join(tmp, "list.tsv")) if line.strip()]
        same = differ = ref_failed = mine_failed = 0
        for name in names:
            src = os.path.join(tmp, name + ".ulz")
            ref = os.path.join(tmp, name + ".ref")
            mine = os.path.join(tmp, name + ".mine")
            subprocess.run([a.unpack, src, ref], capture_output=True)
            have_ref, have_mine = os.path.exists(ref), os.path.exists(mine)
            if not have_ref:
                ref_failed += 1
                print(f"  {name}: the reference produced nothing")
                continue
            if not have_mine:
                mine_failed += 1
                print(f"  {name}: the engine failed to decode it")
                continue
            if open(ref, "rb").read() == open(mine, "rb").read():
                same += 1
            else:
                differ += 1
                print(f"  {name}: DIFFERENT")
        print(f"{len(names)} ULZ streams: {same} identical, {differ} different, "
              f"{mine_failed} failed in the engine, {ref_failed} failed in the reference")
        ok = same == len(names)
        print("PASS" if ok else "FAIL")
        return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
