// The HUD at the frame's edges (src/common/hud.cpp): 0x6B490 draws the HUD (from 0x6BC80 and 0xB2700 under the frame
// 0x7D300, through the 2D layer 0xC4090 and its flush 0x100CC0): screen programs (B0101991's kind, x y in the 640x576
// frame) with depth on and the comparison ALWAYS, so no test: the health ring and the weapon panel moved, the crosshair in
// the middle not, nor the one draw it depth tests (LESS), nor its 256x256 texture's (not a screen).
#include "driving/addr.h"

#include "common/auf.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

X86_EXTERN_FUNC(__imp__sub_0006B490);
X86_FUNC(sub_0006B490) {
    const bool spread = auf::HudSpreadBegin(true);
    __imp__sub_0006B490(ctx, base);
    auf::HudSpreadEnd(spread);
}
