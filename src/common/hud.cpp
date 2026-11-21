// The HUD at the frame's edges on a wide screen (issues 10 and 16).
//
// The game lays its 2D out for 4:3 (GameInfo::hud_aspect), so on a wider frame its HUD stayed in a 4:3 part of the
// middle: the health ring well in from the left, the weapon panel well in from the right. While each exe's HUD draw runs
// (src/bond/hud.cpp, src/driving/hud.cpp) State::hud_layout is the 2D's shape: each piece drawn to one side of the middle
// is moved sideways to keep the distance from that edge it had at 4:3 (gpu::HudMove); one about the middle (the
// crosshair, messages), one across the frame (the hit flash), one drawn depth tested (into the world) or into a view
// narrower than the frame (a split screen's) stays. Not while a menu is up: the pause menu is 4:3 2D kept whole.
#include <mutex>

#include "app/app.h"
#include "common/auf.h"
#include "core/log.h"
#include "xdk/d3d8.h"

namespace auf {
namespace {
void Layout(float shape) {
    std::lock_guard<std::recursive_mutex> lock(d3d::Lock());
    d3d::State().hud_layout = shape;
}
}  // namespace

bool HudSpreadBegin(bool allowed) {
    const float layout = app::HudAspect(), frame = app::FrameAspect();
    if (!allowed || !hud_edges || frame <= layout + 1e-3f || MenuUp()) return false;
    static bool said = false;
    if (!said) said = true, LOGR("auf: the HUD at the frame's edges (laid out at %.3f:1, the frame %.3f:1)", layout, frame);
    Layout(layout);
    return true;
}

void HudSpreadEnd(bool began) {
    if (began) Layout(0);
}
}  // namespace auf
