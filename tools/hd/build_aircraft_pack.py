#!/usr/bin/env python3
import argparse
import glob
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

RECORDS = range(66, 103)

HD_FOR_RIP = {
    'AF-117X': 'F-117/f_117_Base_color.png',
    'EF-2000E': 'EF2000/ef2000_typhoon_Base_color.png',
    'F-15SMT (GR)': 'F-15S/f_15s_Base_color.png',
    'F-15SMT (O)': 'F-15S/f_15s_Base_color.png',
    'F-15SMT (UPEO)': 'F-15S/f_15s_Base_color.png',
    'F-16XA (GR)': 'F-16XA/f_16xa_Base_color.png',
    'F-16XA (UPEO)': 'F-16XA/f_16xa_Base_color.png',
    'F-16XF (GR)': 'F-16XF/f_16xf_Base_color.png',
    'F-16XF (UPEO)': 'F-16XF/f_16xf_Base_color_upeo.png',
    'F-22C (GR)': 'F-22C/f_22_Base_color.png',
    'F-22C (O)': 'F-22C/f_22_Base_color_dision.png',
    'F-22C (UPEO)': 'F-22C/f_22_Base_color_upeo.png',
    'FA-18I': 'FA_18/f_a_18_Base_color.png',
    'FA-18U': 'FA_18/f_a_18_Base_color_upeo.png',
    'FA-32C (GR)': 'FA_32/f_a_32_Base_color.png',
    'FA-32C (UPEO)': 'FA_32/f_a_32_Base_color.png',
    'MiG-33': 'MiG-33/mig_33_Base_color.png',
    'R-101 (Neu)': 'R-101/r_101_Base_color.png',
    'R-101 (UPEO B)': 'R-101/r_101_Base_color.png',
    'R-101 (UPEO G)': 'R-101/r_101_Base_color_upeo.png',
    'R-102 (Neu)': 'R-102/r_102_Base_color_standard.png',
    'R-102 (UPEO)': 'R-102/r_102_Base_color_standard.png',
    'R-102-C': 'R-102/r_102_Base_color.png',
    'R-103 (Neu)': 'R-103/r_103_Base_color_standard.png',
    'R-103 (O)': 'R-103/r_103_Base_color_ouroboros.png',
    'R-103 (UPEO)': 'R-103/r_103_Base_color_standard.png',
    'R-103-C': 'R-103/r_103_Base_color_b.png',
    'R-201 (Neu)': 'R-201/r_201_Base_color.png',
    'R-201 (UPEO B)': 'R-201/r_201_Base_color.png',
    'R-201 (UPEO G)': 'R-201/r_201_Base_color_upeo.png',
    'R-211 (Neu)': 'R-211/r_211_Base_color.png',
    'R-211 (UPEO)': 'R-211/r_211_Base_color.png',
    'R-311': 'R-311/r_311_Base_color.png',
    'R-311-C': 'R-311/r_311_Base_color_b.png',
    'R-352 (Neu)': 'R-352/r_352_Base_color.png',
    'R-352 (UPEO)': 'R-352/r_352_Base_color.png',
    'RF-12A2 (GR)': 'RF-12/rf_12_Base_color.png',
    'RF-12A2 (UPEO)': 'RF-12/rf_12_Base_color.png',
    'Su-37 (Rena)': 'Su-37/su_37_Base_color.png',
    'Su-37 (UPEO)': 'Su-37/su_37_Base_color_a.png',
    'Su-43 (O)': 'Su-43/su_43_Base_color_upeo.png',
    'Su-43 (UPEO)': 'Su-43/su_43_Base_color_upeo.png',
    'UI-4054': 'UI-4054/ui_4054_Base_color.png',
    'X-49': 'X-49/x_49_Base_color.png',
    'XFA-36A (GR)': 'XFA-36/xfa_36_Base_color_gr.png',
    'XFA-36A (O)': 'XFA-36/xfa_36_Base_color.png',
    'XFA-36A (UPEO)': 'XFA-36/xfa_36_Base_color_gr.png',
    'XR-900': 'XR-900/xr_900_Base_color.png',
}

def fnv1a64(data):
    h = 0xCBF29CE484222325
    for b in data:
        h ^= b
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h

def read_tim(path):
    b = open(path, 'rb').read()
    magic, flags = struct.unpack_from('<II', b, 0)
    assert magic == 0x10 and (flags & 7) == 0 and (flags & 8), (path, hex(flags))
    clen, cx, cy, cw, ch = struct.unpack_from('<IHHHH', b, 8)
    pal = np.frombuffer(b, dtype='<u2', count=cw * ch, offset=20).reshape(ch, cw)
    at = 8 + clen
    ilen, ix, iy, iw, ih = struct.unpack_from('<IHHHH', b, at)
    words = np.frombuffer(b, dtype='<u2', count=iw * ih, offset=at + 12).reshape(ih, iw)
    idx = np.zeros((ih, iw * 4), dtype=np.uint8)
    for k in range(4):
        idx[:, k::4] = (words >> (4 * k)) & 15
    return idx, pal.astype(np.int64), words

def rgb5(c):
    return np.stack([c & 31, (c >> 5) & 31, (c >> 10) & 31], axis=-1)

def family(name):
    return re.sub(r'\s*\(.*\)$', '', name)

def materials(hd_root, hd, out, written):
    folder = os.path.join(hd_root, os.path.dirname(hd))
    prefix = os.path.basename(hd).split('_Base_color')[0]
    names = []
    for suffix, mode, tag in (('_Normal_OpenGL.png', 'RGB', 'normal'), ('_Roughness.png', 'L', 'rough')):
        src = os.path.join(folder, prefix + suffix)
        if not os.path.exists(src):
            found = sorted(glob.glob(os.path.join(folder, '*' + suffix)))
            src = found[0] if len(found) == 1 else None
        if not src:
            names.append('-')
            continue
        name = re.sub(r'[^A-Za-z0-9_.-]', '_', prefix) + '_' + tag + '.png'
        if name not in written:
            Image.open(src).convert(mode).save(os.path.join(out, name), optimize=True)
            written.add(name)
        names.append(name)
    return names

def add_materials(hd_root, pack):
    by_image = {os.path.basename(hd): hd for hd in HD_FOR_RIP.values()}
    path = os.path.join(pack, 'manifest.txt')
    lines = open(path, encoding='utf-8').read().splitlines()
    written, count = set(), 0
    for i, line in enumerate(lines):
        t = line.split(' ')
        if not t or t[0] != 'plane':
            continue
        t = [w for w in t if not w.startswith('normal=') and not w.startswith('rough=')]
        base = next((w[5:] for w in t if w.startswith('base=')), None)
        if base not in by_image:
            print('  %s: no HD set known for %s' % (t[2], base))
            continue
        normal, rough = materials(hd_root, by_image[base], pack, written)
        lines[i] = ' '.join(t[:4] + ['normal=' + normal, 'rough=' + rough] + t[4:])
        count += 1
    open(path, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('%d planes given materials, %d images -> %s' % (count, len(written), pack))

def main():
    if '--add-materials' in sys.argv:
        ap = argparse.ArgumentParser()
        ap.add_argument('--hd', required=True)
        ap.add_argument('--add-materials', required=True)
        a = ap.parse_args()
        add_materials(a.hd, a.add_materials)
        return
    ap = argparse.ArgumentParser()
    ap.add_argument('--disc', required=True)
    ap.add_argument('--ac3cat', required=True)
    ap.add_argument('--rips', required=True)
    ap.add_argument('--hd', required=True)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()

    rips = {}
    for f in glob.glob(os.path.join(a.rips, '*.bmp')):
        im = np.asarray(Image.open(f).convert('RGB'), dtype=np.int64)
        if im.shape[:2] == (256, 256):
            rips[os.path.basename(f)[:-4]] = im >> 3

    work = tempfile.mkdtemp(prefix='ac3hd_')
    planes = {}
    for r in RECORDS:
        d = os.path.join(work, str(r))
        subprocess.run([a.ac3cat, '--disc', a.disc, 'export', str(r), d], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        idx, pal, words = read_tim(os.path.join(d, '%d_2_u_0.tim' % r))
        planes[r] = (idx, pal, fnv1a64(words.astype('<u2').tobytes()))
    shutil.rmtree(work, ignore_errors=True)

    def coverage(idx, pal, rip):
        m = rip.max(axis=2) > 0
        hit = np.zeros(m.shape, bool)
        for row in pal:
            hit |= (rgb5(row[idx]) == rip).all(axis=2) & (row[idx] != 0)
        return float(hit[m].mean()), hit

    base = {}
    for r, (idx, pal, _) in planes.items():
        best = max(((coverage(idx, pal, rip)[0], n) for n, rip in rips.items()), key=lambda t: t[0])
        if best[0] < 0.999:
            sys.exit('record %d matches no rip (best %s %.3f)' % (r, best[1], best[0]))
        base[r] = best[1]
    print('%d records, each its rip exactly' % len(base))

    variants = {}
    for name, rip in sorted(rips.items()):
        if name in base.values():
            continue
        cands = [r for r in RECORDS if family(base[r]) == family(name)]
        best = None
        for r in cands:
            diff = (rips[base[r]] != rip).any(axis=2)
            if best is None or diff.sum() < best[0]:
                best = (int(diff.sum()), r, diff)
        if best is None or best[0] > 4000:
            print('  %-16s no record of its family is it with another emblem' % name)
            continue
        print('  %-16s record %d, %d pixels of emblem' % (name, best[1], best[0]))
        variants.setdefault(best[1], []).append((name, best[2]))

    rng = np.random.default_rng(3)
    os.makedirs(a.out, exist_ok=True)
    lines = ['# AC3 HD aircraft textures (tools/hd/build_aircraft_pack.py). One plane a line:',
             '#   plane <record> <name> base=<image> variant=<image>|- normal=<image>|-',
             '#         rough=<image>|- rows=<row hash>,...',
             '#         samples=<x y index>... probe=<x y index>...',
             '# x, y and the 4-bit index each in two, two and one hex digits; the row hashes',
             '# FNV-1a 64 over a palette row\'s 16 halfwords (little endian).']
    images = set()
    written = set()

    def image_for(name):
        hd = HD_FOR_RIP.get(name)
        if not hd:
            return None
        image = os.path.basename(hd)
        if image not in images:
            shutil.copyfile(os.path.join(a.hd, hd), os.path.join(a.out, image))
            images.add(image)
        return image

    def texels(points, idx):
        return ''.join('%02x%02x%x' % (x, y, idx[y, x]) for x, y in points)

    count = 0
    for r in RECORDS:
        idx, pal, _ = planes[r]
        img = image_for(base[r])
        if not img:
            print('  record %d (%s): no HD image' % (r, base[r]))
            continue
        unstable = np.zeros(idx.shape, bool)
        for _, diff in variants.get(r, []):
            unstable |= diff
        ys, xs = np.nonzero((rips[base[r]].max(axis=2) > 0) & ~unstable)
        pick = rng.choice(len(xs), size=min(384, len(xs)), replace=False)
        samples = [(int(xs[i]), int(ys[i])) for i in sorted(pick)]
        variant, probe = '-', ''
        for vname, diff in variants.get(r, [])[:1]:
            vimg = image_for(vname)
            if vimg:
                ys, xs = np.nonzero(diff)
                pick = rng.choice(len(xs), size=min(128, len(xs)), replace=False)
                variant, probe = vimg, texels([(int(xs[i]), int(ys[i])) for i in sorted(pick)], idx)
        rows = ','.join('%016x' % fnv1a64(row.astype('<u2').tobytes()) for row in pal)
        normal, rough = materials(a.hd, HD_FOR_RIP[base[r]], a.out, written)
        lines.append('plane %d %s base=%s variant=%s normal=%s rough=%s rows=%s samples=%s probe=%s' % (
            r, base[r].replace(' ', '_'), img, variant, normal, rough, rows, texels(samples, idx), probe or '-'))
        count += 1
    open(os.path.join(a.out, 'manifest.txt'), 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('%d planes, %d images -> %s' % (count, len(images), a.out))

if __name__ == '__main__':
    main()
