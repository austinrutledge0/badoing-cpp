// SPDX-License-Identifier: MIT
#include "petlogic.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace badoing {
namespace {
int boundedInt(std::int64_t value) noexcept
{
    return static_cast<int>(std::clamp(value,
        static_cast<std::int64_t>(std::numeric_limits<int>::min()),
        static_cast<std::int64_t>(std::numeric_limits<int>::max())));
}
}

int normalizedWidth(int width) noexcept
{
    return width == 260 || width == 355 || width == 470 ? width : 355;
}

int heightForWidth(int width) noexcept
{
    return static_cast<int>(std::lround(cropHeight * normalizedWidth(width)
                                       / static_cast<double>(cropWidth)));
}

int frameAtElapsed(std::int64_t milliseconds) noexcept
{
    if (milliseconds < 0 || milliseconds >= animationDuration) return 0;
    int boundary = 0;
    for (int frame = 0; frame < frameCount; ++frame) {
        boundary += frameDelays[static_cast<std::size_t>(frame)];
        if (milliseconds < boundary) return frame;
    }
    return 0;
}

std::uint64_t incrementCount(std::uint64_t count) noexcept
{
    return count == std::numeric_limits<std::uint64_t>::max() ? count : count + 1;
}

Point clampPosition(Point position, int width, int height, Rect available) noexcept
{
    // Work with wide intermediates: malformed settings must not overflow geometry.
    const std::int64_t left = available.x;
    const std::int64_t top = available.y;
    const auto right = std::max(left, left + std::max(0, available.width) - std::max(0, width));
    const auto bottom = std::max(top, top + std::max(0, available.height) - std::max(0, height));
    return {boundedInt(std::clamp<std::int64_t>(position.x, left, right)),
            boundedInt(std::clamp<std::int64_t>(position.y, top, bottom))};
}

Point dockPosition(Point position, int width, int height, Rect available,
                   bool hasPosition) noexcept
{
    if (!hasPosition)
        position.x = boundedInt(static_cast<std::int64_t>(available.x)
                                + available.width - width - 40);
    position.y = boundedInt(static_cast<std::int64_t>(available.y)
                            + available.height - height);
    return clampPosition(position, width, height, available);
}

bool nearBottom(Point position, int height, Rect available, int distance) noexcept
{
    auto difference = static_cast<std::int64_t>(position.y) + height
                    - static_cast<std::int64_t>(available.y) - available.height;
    if (difference < 0) difference = -difference;
    return distance >= 0 && difference <= distance;
}

} // namespace badoing
