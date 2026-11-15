#!/usr/bin/env python3
import os
import pathlib
import re
import shutil
import subprocess
import sys
import concurrent.futures as cf

ROOT = pathlib.Path(__file__).resolve().parents[2]
REF = pathlib.Path(os.environ.get('AC3_ORACLE_REF', str(ROOT.parent / 'oracle_ref')))
DISC = os.environ.get('AC3_DISC', '')
SESSIONS = ['m01', 'idle']
BUILD = ROOT / 'build' / 'release'
OUT = ROOT / 'build' / 'gate'

def run(cmd, **kw):
    print('$', ' '.join(str(c) for c in cmd), flush=True)
    return subprocess.run(cmd, cwd=ROOT, **kw)

def session(name):
    ref = REF / name
    out = OUT / name
    out.mkdir(parents=True, exist_ok=True)
    cards = out / 'cards'
    shutil.rmtree(cards, ignore_errors=True)
    cards.mkdir(parents=True)
    for c in ('card1.mcd', 'card2.mcd'):
        shutil.copy(ref / 'cards_pristine' / c, cards / c)
    exe = BUILD / ('simdiff.exe' if os.name == 'nt' else 'simdiff')
    cmd = [str(exe), str(ref / 'state.bin'), '--frames', '--boot', '--disc', DISC,
           '--input', str(ref / 'input.ac3i'), '--cards', str(cards), '--vram', str(ref / 'frames.tsv'),
           '--audio-dump', str(out / 'engine_audio.wav'), '--coverage']
    with open(out / 'simdiff.txt', 'w', encoding='utf-8') as f:
        rc = subprocess.run(cmd, cwd=ROOT, stdout=f, stderr=subprocess.STDOUT).returncode
    shutil.rmtree(cards, ignore_errors=True)
    text = (out / 'simdiff.txt').read_text(encoding='utf-8', errors='replace')
    problems = []
    if rc != 0:
        problems.append(f'simdiff exit status {rc}')
    parts = re.findall(r'part\s+(\d+):\s+(\d+) exact, (\d+) different, (\d+) the oracle\'s', text)
    if not parts:
        problems.append('no chained-frames summary (did simdiff stop early?)')
    for p, ex, diff, orc in parts:
        if int(diff) or int(orc):
            problems.append(f'part {p}: {diff} different, {orc} the oracle\'s')
    m = re.search(r'VRAM at the fields\' ends: (\d+) as the oracle\'s, (\d+) different', text)
    if not m:
        problems.append('no VRAM summary')
    elif int(m.group(2)):
        problems.append(f'VRAM: {m.group(2)} fields differ')
    m = re.search(r'(\d+) calls reached code not reimplemented', text)
    if m:
        problems.append(f'{m.group(1)} calls reached code that is not there')
    try:
        a = (ref / 'audio.wav').read_bytes()[44:]
        b = (out / 'engine_audio.wav').read_bytes()[44:]
        n = min(len(a), len(b))
        if a[:n] != b[:n]:
            problems.append('audio differs from the oracle\'s')
        elif len(a) - len(b) > 16384:
            problems.append(f'audio ends early ({len(b)} of {len(a)} bytes)')
    except OSError as e:
        problems.append(f'audio: {e}')
    first = re.search(r'first difference at tick (\d+)', text)
    if first and problems:
        problems.append(f'first difference at tick {first.group(1)}; see {out / "simdiff.txt"}')
    return name, problems, rc

def main():
    args = sys.argv[1:]
    failed = []
    if '--no-build' not in args:
        if run(['cmake', '--build', '--preset', 'release']).returncode:
            print('GATE: FAIL (build)')
            return 1
    if run(['ctest', '--preset', 'release'], stdout=subprocess.DEVNULL).returncode:
        failed.append('unit tests')
    with cf.ThreadPoolExecutor(len(SESSIONS)) as ex:
        results = list(ex.map(session, SESSIONS))
    for name, problems, rc in results:
        print(f'session {name}: ' + ('exact' if not problems else 'FAIL'))
        for p in problems:
            print('   ', p)
        if problems:
            failed.append(f'session {name}')
    print('GATE: ' + ('PASS' if not failed else 'FAIL (' + ', '.join(failed) + ')'))
    return 1 if failed else 0

if __name__ == '__main__':
    sys.exit(main())
