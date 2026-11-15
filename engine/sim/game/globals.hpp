#pragma once

#include "core/types.hpp"

namespace ac3::sim::g {
inline constexpr u32 kGroundSlots = 0x800BCE4Cu;
inline constexpr u32 kExit = 0x800BCE50u;
inline constexpr u32 kWorld = 0x800BCE54u;
inline constexpr u32 kSlots = 0x800BCE5Cu;
inline constexpr u32 kScene = 0x800BCE60u;
inline constexpr u32 kMissionData = 0x800BCEACu;
inline constexpr u32 kMission = 0x800BCEB4u;
inline constexpr u32 kAiInfo = 0x800BCEC0u;
inline constexpr u32 kLinkCount = 0x800BCEF1u;
inline constexpr u32 kSlotRecords = 0x800BCEF8u;
inline constexpr u32 kGroundRecords = 0x800BCEFCu;
inline constexpr u32 kLinks = 0x800BCF0Cu;
inline constexpr u32 kLinkFlags = 0x800BCF18u;
inline constexpr u32 kWeapons = 0x800BD080u;
inline constexpr u32 kPlayer = 0x800BD094u;
inline constexpr u32 kTrails = 0x800BD0B0u;

inline constexpr u32 kPlayerInvRot = 0x800C6EA0u;
inline constexpr u32 kNearDist = 0x800C6EC4u;
inline constexpr u32 kNearAngle = 0x800C6EC0u;
inline constexpr u32 kFarDist = 0x800C6EC8u;
inline constexpr u32 kRelation = 0x800C6ED0u;
inline constexpr u32 kSlotFlags = 0x800C6FD0u;
inline constexpr u32 kWreckTimer = 0x800C70D0u;
inline constexpr u32 kVoices = 0x800C5830u;
inline constexpr u32 kPairs = 0x800C6D80u;
inline constexpr u32 kCurrentTarget = 0x800C6D6Eu;

inline constexpr u32 kSinCos = 0x800104ACu;
inline constexpr u32 kAtanTable = 0x8001A4B0u;
inline constexpr u32 kAtanOctant = 0x800234B8u;
inline constexpr u32 kSqrtTable = 0x800B6DD4u;

inline constexpr u32 kScratch = 0x1F800000u;
}
