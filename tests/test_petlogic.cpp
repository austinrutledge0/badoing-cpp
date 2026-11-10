// SPDX-License-Identifier: MIT
#include "petlogic.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <numeric>

namespace {
int failures = 0;
void check(bool condition, const char* description)
{
    if (!condition) { std::cerr << "FAIL: " << description << '\n'; ++failures; }
}
}

int main()
{
    using namespace badoing;
    check(std::accumulate(frameDelays.begin(), frameDelays.end(), 0) == animationDuration,
          "animation duration matches the actual frame delays");
    check(frameAtElapsed(-1) == 0 && frameAtElapsed(0) == 0 && frameAtElapsed(69) == 0,
          "first frame and negative elapsed time");
    check(frameAtElapsed(70) == 1 && frameAtElapsed(139) == 1 && frameAtElapsed(140) == 2,
          "frame boundaries do not skip or extend frames");
    check(frameAtElapsed(599) == 8 && frameAtElapsed(600) == 9 && frameAtElapsed(669) == 9,
          "last two frames retain the original timings");
    check(frameAtElapsed(670) == 0 && frameAtElapsed(std::numeric_limits<std::int64_t>::max()) == 0,
          "finished animations return to their resting frame");
    check(normalizedWidth(260) == 260 && normalizedWidth(355) == 355 && normalizedWidth(470) == 470,
          "all three supported sizes remain available");
    check(normalizedWidth(-10) == 355 && normalizedWidth(2000000000) == 355,
          "untrusted saved widths fall back safely");
    check(heightForWidth(260) == 75 && heightForWidth(355) == 103 && heightForWidth(470) == 136,
          "character aspect ratio is preserved");
    check(incrementCount(0) == 1 && incrementCount(std::numeric_limits<std::uint64_t>::max())
                                   == std::numeric_limits<std::uint64_t>::max(),
          "counter increments and saturates without wrapping");
    const Rect primary{0, 0, 1920, 1040};
    auto position = clampPosition({0,0}, 355, 103, primary);
    check(position.x == 0 && position.y == 0, "a saved origin is a valid position");
    position = clampPosition({9999,-10}, 355, 103, primary);
    check(position.x == 1565 && position.y == 0, "offscreen positions return to the work area");
    const Rect leftMonitor{-1920, -100, 1920, 1080};
    position = clampPosition({-1800,150}, 355, 103, leftMonitor);
    check(position.x == -1800 && position.y == 150, "negative monitor coordinates are preserved");
    position = dockPosition({0,0}, 355, 103, primary, false);
    check(position.x == 1525 && position.y == 937, "initial pet sits near the lower right");
    position = dockPosition({0,0}, 355, 103, primary, true);
    check(position.x == 0 && position.y == 937, "docking preserves an intentional x coordinate of zero");
    position = clampPosition({200,200}, 470, 136, {10,20,100,50});
    check(position.x == 10 && position.y == 20, "tiny screens do not produce invalid clamp ranges");
    check(nearBottom({0,913}, 103, primary) && !nearBottom({0,912}, 103, primary),
          "snap threshold includes exactly 24 pixels");
    position = clampPosition({std::numeric_limits<int>::max(), std::numeric_limits<int>::min()},
                             355, 103, {std::numeric_limits<int>::max()-5, -100, 100, 100});
    check(position.x == std::numeric_limits<int>::max()-5 && position.y == -100,
          "geometry arithmetic avoids signed integer overflow");
    if (failures) return EXIT_FAILURE;
    std::cout << "All pet logic checks passed.\n";
    return EXIT_SUCCESS;
}
