#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
s32 scaled(s32 value, u32 shift) { return static_cast<s32>(static_cast<u32>(value) << shift); }
s32 sine(Memory& memory, s32 phase) {
    return memory.r16s(g::kSinCos + (static_cast<u32>(phase) & 0x7FFEu));
}
}

s32 Game::flight_thrust(s32 speed, s32 stall, s32 maximum, s32 throttle,
                        s32 altitude, s32 acceleration, s32 deceleration) {
    cover(0x8006E908u);
    if (speed < 0) speed = 0;
    if (maximum < speed) speed = maximum;
    const bool upperBand = stall < speed;
    const s32 numerator = scaled(upperBand ? wrap::sub(speed, stall) : speed, 12);
    const s32 denominator = upperBand ? wrap::sub(maximum, stall) : stall;
    if (!denominator)
        unported(upperBand ? 0x8006E948u : 0x8006E988u, "throttle division by zero");
    if (denominator == -1 && static_cast<u32>(numerator) == 0x80000000u)
        unported(upperBand ? 0x8006E960u : 0x8006E9A0u, "throttle division overflow");
    s32 normalized = static_cast<s32>(wrap::div(numerator, denominator).lo) >> 1;
    if (upperBand) normalized = wrap::add(normalized, 2048);
    if (normalized > 4096) normalized = 4096;
    if (normalized < -4096) normalized = -4096;
    const s32 demand = wrap::sub(wrap::add(throttle, 4096) >> 1, normalized);
    const s32 response = wrap::mul(demand > 0 ? acceleration : deceleration, demand) / 4096;
    s32 height = altitude >> 12;
    if (height < -54822) height = -54822;
    const s32 density = 4096 - (2867 * height) / -54822;
    return wrap::mul(response, density) / 4096;
}

s32 Game::flight_envelope(s32 altitude, s32 speed, u32 definition,
                          u32 stallOut, u32 cruiseOut, u32 topOut) {
    cover(0x8006EA88u);
    (void)speed;
    constexpr u32 kCeiling = 44, kHighStall = 48, kBaseTop = 52, kBaseStall = 56;
    constexpr u32 kHighTop = 60, kTopCap = 64, kBaseCruise = 68;
    const s32 ceiling = m.r32s(definition + kCeiling);
    const s32 highStall = m.r32s(definition + kHighStall), baseTop = m.r32s(definition + kBaseTop);
    const s32 baseStall = m.r32s(definition + kBaseStall), highTop = m.r32s(definition + kHighTop);
    const s32 topCap = m.r32s(definition + kTopCap), baseCruise = m.r32s(definition + kBaseCruise);
    s32 height = altitude >> 12;
    if (height > 0) height = 0;
    if (height < ceiling) height = ceiling;
    auto divide = [&](s32 numerator, s32 divisor, u32 zero, u32 overflow) {
        if (!divisor) unported(zero, "flight envelope division by zero");
        if (divisor == -1 && static_cast<u32>(numerator) == 0x80000000u)
            unported(overflow, "flight envelope division overflow");
        return static_cast<s32>(wrap::div(numerator, divisor).lo);
    };
    const s32 descent = divide(wrap::neg(scaled(wrap::sub(height, ceiling), 14)), ceiling,
                              0x8006EAE4u, 0x8006EAFCu);
    const s32 topDelta = wrap::sub(highTop, baseTop);
    const s32 ascent = divide(scaled(height, 14), ceiling, 0x8006EB2Cu, 0x8006EB44u);
    const s32 stallDelta = wrap::sub(highStall, baseStall) >> 2;
    const s32 liftBias = wrap::mul(stallDelta, ascent) >> 12;
    const s32 stallWave = wrap::mul(sine(m, ascent), stallDelta) >> 14;
    const s32 stallSpeed = wrap::sub(wrap::add(liftBias, baseStall), stallWave);
    s32 topSpeed = wrap::add(wrap::add(wrap::mul(topDelta, descent) >> 14, baseTop),
        wrap::mul(sine(m, wrap::mul(5, descent) >> 2), topDelta >> 2) >> 13);
    const s32 cap = wrap::add(wrap::sub(stallSpeed, baseStall), topCap);
    if (cap < topSpeed) topSpeed = cap;
    const s32 quarter = ceiling >> 2;
    const s32 fade = divide(scaled(wrap::sub(height, wrap::sub(ceiling, quarter)), 12),
                           quarter, 0x8006EBECu, 0x8006EC04u);
    const s32 cruiseDelta = wrap::sub(wrap::add(highStall, baseTop) >> 1, baseCruise) >> 2;
    const s32 cruiseSpeed = wrap::add(wrap::mul(cruiseDelta, ascent) >> 12, baseCruise);
    m.w32(stallOut, static_cast<u32>(stallSpeed));
    m.w32(topOut, static_cast<u32>(topSpeed));
    m.w32(cruiseOut, static_cast<u32>(cruiseSpeed));
    s32 lift = wrap::sub(4096, fade);
    if (lift > 4096) lift = 4096;
    return lift < 0 ? 0 : lift;
}

s32 Game::speed_limits(s32 altitude, u32 stallOut, u32 cruiseOut, u32 topOut) {
    cover(0x8006EC70u);
    constexpr s32 kCeiling = -50000;
    s32 height = altitude >> 12;
    if (height > 0) height = 0;
    if (height < kCeiling) height = kCeiling;
    const s32 ascent = (height * 16384) / kCeiling;
    const s32 descent = (-16384 * (height - kCeiling)) / kCeiling;
    const s32 stallWave = 47104 * sine(m, ascent);
    s32 topSpeed = ((28672 * descent) >> 14) + 335872
        + ((7168 * sine(m, (5 * descent) >> 2)) >> 13);
    const s32 cap = ((47104 * ascent) >> 12) + 356352 - (stallWave >> 14);
    if (cap < topSpeed) topSpeed = cap;
    m.w32(stallOut, static_cast<u32>(((47104 * ascent) >> 12) + 53248 - (stallWave >> 14)));
    m.w32(topOut, static_cast<u32>(topSpeed));
    m.w32(cruiseOut, static_cast<u32>(((41472 * ascent) >> 12) + 122880));
    const s32 power = 4096 - ((height + 37500) * 4096) / -12500;
    return power > 4096 ? 4096 : power;
}
}
