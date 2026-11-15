#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDigitsEnd = 0x800C1350u;
constexpr u32 kUpperDigits = 0x80010134u;
constexpr u32 kLowerDigits = 0x80010148u;
constexpr u32 kNullText = 0x800BCB9Cu;
constexpr u32 kCtype = 0x800BCA55u;
constexpr u32 kShort = 1, kLong = 2, kWide = 4, kAlt = 0x10, kLeft = 0x20, kZero = 0x40;
constexpr u32 kPrefixLowerX = 0x80, kPrefixUpperX = 0x100, kPrefixOctal = 0x200, kPrefixBinary = 0x400;
}

u32 Game::text_put(u32 c, u32 out) {
    cover(0x80027C24u);
    const u32 p = m.r32(out);
    m.w8(p, c);
    m.w32(out, p + 1);
    return c;
}

u32 Game::number_digits(u32 lo, u32 hi, u32 flags, s32 precision, u32 base, u32 len_out, u32 digits) {
    cover(0x80027C3Cu);
    Frame f(*this, 0x50);
    u32 shift = 0, mask = 0;
    if (base == 8) { shift = 3; mask = 7; }
    else if (base == 2) { shift = 1; mask = 1; }
    else if (base == 16) { shift = 4; mask = 15; }
    u32 p = kDigitsEnd;
    if (precision != 0) {
        if (flags & kWide) {
            u64 v = (static_cast<u64>(hi) << 32) | lo;
            if (shift) {
                do {
                    const u32 d = m.r8(digits + (mask & static_cast<u32>(v)));
                    v >>= shift;
                    m.w8(--p, d);
                } while (v);
            } else {
                const u64 b = static_cast<u64>(static_cast<s64>(static_cast<s32>(base)));
                do {
                    const u64 q = udivdi3(v, b);
                    const u32 rem = static_cast<u32>(v) - static_cast<u32>(q) * base;
                    const u32 d = m.r8(digits + rem);
                    v = q;
                    m.w8(--p, d);
                } while (v);
            }
        } else {
            u32 v = lo;
            if (shift) {
                do {
                    const u32 d = m.r8(digits + (v & mask));
                    v >>= shift;
                    m.w8(--p, d);
                } while (v);
            } else {
                do {
                    const u32 q = base ? v / base : 0xFFFFFFFFu;
                    const u32 d = m.r8(digits + (v - q * base));
                    v = q;
                    m.w8(--p, d);
                } while (v);
            }
        }
    }
    m.w32(len_out, kDigitsEnd - p);
    return p;
}

s32 Game::text_field(u32 out, u32 text, u32 flags, u32 sign, s32 width, s32 precision, s32 len) {
    cover(0x80027E80u);
    Frame f(*this, 0x38);
    s32 body = static_cast<s32>((flags >> 9) & 1u) + len + (sign ? 1 : 0);
    if (flags & (kPrefixLowerX | kPrefixUpperX)) body += 2;
    const s32 total = body < precision ? precision : body;
    if (!(flags & (kLeft | kZero)))
        for (s32 i = total; i < width; ++i) text_put(' ', out);
    if (sign) {
        text_put(sign, out);
    } else if (flags & (kPrefixLowerX | kPrefixUpperX | kPrefixOctal)) {
        text_put('0', out);
        if (flags & kPrefixLowerX) text_put('x', out);
        else if (flags & kPrefixUpperX) text_put('X', out);
        else if (flags & kPrefixBinary) text_put('b', out);
    }
    if ((flags & (kLeft | kZero)) == kZero)
        for (s32 i = total; i < width; ++i) text_put('0', out);
    for (s32 i = body; i < precision; ++i) text_put('0', out);
    for (s32 i = 0; i < len; ++i) text_put(m.r8(text + static_cast<u32>(i)), out);
    if (flags & kLeft)
        for (s32 i = total; i < width; ++i) text_put(' ', out);
    return total < width ? width : total;
}

u32 Game::text_format(u32 fmt, u32 args, u32 out) {
    cover(0x80028074u);
    Frame f(*this, 0x50);
    const u32 len_at = f.at(0x20), char_at = f.at(0x24);
    u32 count = 0;
    auto digit = [&](u32 c) { return (m.r8(kCtype + c) & 4u) != 0; };
    auto next_word = [&]() {
        args += 4;
        return m.r32(args - 4);
    };
    for (;;) {
        u32 c = m.r8(fmt++);
        while (c && c != '%') {
            text_put(c, out);
            ++count;
            c = m.r8(fmt++);
        }
        if (!c) return count;
        --fmt;
        u32 flags = 0, sign = 0;
        s32 width = 0, precision = -1;
        bool done = false;
        while (!done) {
            ++fmt;
            const u32 t = m.r8(fmt);
            u32 lo = 0, hi = 0;
            auto take_unsigned = [&](u32 prefix) {
                if (flags & kWide) {
                    args += 8;
                    lo = m.r32(args - 8);
                    hi = m.r32(args - 4);
                } else {
                    args += 4;
                    lo = (flags & kShort) ? m.r16(args - 4) : m.r32(args - 4);
                    hi = 0;
                }
                if ((flags & kAlt) && (lo | hi)) flags |= prefix;
            };
            auto number = [&](u32 base, u32 table, u32 lead) {
                const u32 text = number_digits(lo, hi, flags, precision, base, len_at, table);
                count += static_cast<u32>(text_field(out, text, flags, lead, width, precision, m.r32s(len_at)));
                done = true;
            };
            switch (t) {
            case 0: return count;
            case ' ':
                if (!sign) sign = ' ';
                break;
            case '#': flags |= kAlt; break;
            case '+': sign = '+'; break;
            case '-': flags |= kLeft; break;
            case '0':
                if (sign != '-' && precision <= 0) flags |= kZero;
                break;
            case 'h': flags |= kShort; break;
            case 'l': flags |= kLong; break;
            case 'L': flags |= kWide | 8u; break;
            case '.': {
                ++fmt;
                u32 d = m.r8(fmt);
                if (d == '*') {
                    precision = static_cast<s32>(next_word());
                    break;
                }
                s32 n = 0;
                while (digit(d)) {
                    n = n * 10 - 48 + static_cast<s32>(d);
                    ++fmt;
                    d = m.r8(fmt);
                }
                --fmt;
                precision = n;
                break;
            }
            case '*':
            case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                if (t == '*') {
                    width = static_cast<s32>(next_word());
                    if (width < 0) {
                        width = -width;
                        flags |= kLeft;
                    }
                } else {
                    s32 n = 0;
                    do {
                        n = n * 10 - 48 + static_cast<s32>(m.r8(fmt));
                        ++fmt;
                    } while (digit(m.r8(fmt)));
                    width = n;
                    --fmt;
                }
                break;
            case 'n':
                if (!(flags & kLong) && (flags & kShort)) m.w16(next_word(), count);
                else m.w32(next_word(), count);
                done = true;
                break;
            case 'c':
                m.w8(char_at, next_word());
                count += static_cast<u32>(text_field(out, char_at, flags, 0, width, precision, 1));
                done = true;
                break;
            case 's': {
                u32 s = next_word();
                if (!s) s = kNullText;
                if (precision >= 0) {
                    u32 n = static_cast<u32>(precision);
                    for (u32 i = 0; i < static_cast<u32>(precision); ++i)
                        if (!m.r8(s + i)) {
                            n = i;
                            break;
                        }
                    m.w32(len_at, n);
                } else {
                    m.w32(len_at, libc_strlen(s));
                }
                const s32 n = m.r32s(len_at);
                count += static_cast<u32>(text_field(out, s, flags, 0, width, n, n));
                done = true;
                break;
            }
            case 'X':
                take_unsigned(kPrefixUpperX);
                number(16, kUpperDigits, 0);
                break;
            case 'p':
                if (precision == -1) precision = 8;
                [[fallthrough]];
            case 'x':
                take_unsigned(kPrefixLowerX);
                number(16, kLowerDigits, 0);
                break;
            case 'o':
                take_unsigned(kPrefixOctal);
                number(8, kLowerDigits, 0);
                break;
            case 'b':
                take_unsigned(kPrefixBinary);
                number(2, kLowerDigits, 0);
                break;
            case 'U': case 'Z':
                flags |= kLong;
                [[fallthrough]];
            case 'u':
                take_unsigned(0);
                number(10, kLowerDigits, 0);
                break;
            case 'D': case 'I':
                flags |= kLong;
                [[fallthrough]];
            case 'd': case 'i': {
                if (flags & kWide) {
                    args += 8;
                    lo = m.r32(args - 8);
                    hi = m.r32(args - 4);
                } else {
                    args += 4;
                    lo = (flags & kShort) ? static_cast<u32>(static_cast<s32>(m.r16s(args - 4))) : m.r32(args - 4);
                    hi = static_cast<u32>(static_cast<s32>(lo) >> 31);
                }
                if (static_cast<s32>(hi) < 0) {
                    lo = 0u - lo;
                    hi = 0u - hi - (lo ? 1u : 0u);
                    sign = '-';
                }
                number(10, kLowerDigits, sign);
                break;
            }
            default:
                ++count;
                text_put(t, out);
                done = true;
                break;
            }
        }
        ++fmt;
    }
}
}
