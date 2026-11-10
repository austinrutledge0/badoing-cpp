// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>

namespace badoing {

inline constexpr int frameWidth = 426;
inline constexpr int frameHeight = 240;
inline constexpr int frameCount = 10;
inline constexpr int cropX = 30;
inline constexpr int cropY = 106;
inline constexpr int cropWidth = 318;
inline constexpr int cropHeight = 92;
inline constexpr std::array<int, frameCount> frameDelays{70,70,60,70,70,60,70,70,60,70};
inline constexpr int animationDuration = 670;

struct Point { int x; int y; };
struct Rect { int x; int y; int width; int height; };

int normalizedWidth(int width) noexcept;
int heightForWidth(int width) noexcept;
int frameAtElapsed(std::int64_t milliseconds) noexcept;
std::uint64_t incrementCount(std::uint64_t count) noexcept;
Point clampPosition(Point position, int width, int height, Rect available) noexcept;
Point dockPosition(Point position, int width, int height, Rect available,
                   bool hasPosition) noexcept;
bool nearBottom(Point position, int height, Rect available, int distance = 24) noexcept;

} // namespace badoing
