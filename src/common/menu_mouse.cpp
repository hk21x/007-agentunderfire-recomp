// The mouse in the menus: the wheel scrolls (d-pad up and down), the left button selects (A), the right goes back (B),
// and Enter selects too. While a menu is up (auf::MenuUp, each image's own test) the mouse's buttons stop playing what
// they are bound to in play and play A and B instead; each notch of the wheel is a press of the d-pad. In play a notch
// cycles the weapon: on foot the d-pad's down a notch down and up a notch up; in the car Black and White. On a mounted
// gun (auf::OnGun) the left button fires and the right zooms. The menus show a gold pointer of their own, and the left button
// selects what it is over (src/bond/pointer.cpp, src/driving/pointer.cpp).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#include "common/auf.h"
#include "core/input.h"
#include "core/log.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/app.h"
#endif

namespace auf {
namespace {
bool KeyHeld(int vk) {
#ifdef _WIN32
    return GetForegroundWindow() == static_cast<HWND>(app::Window()) && (GetAsyncKeyState(vk) & 0x8000);
#else
    (void)vk;
    return false;
#endif
}

// AUF_POINTER=frame:x:y:buttons,... headless: the pointer from each frame of polls on, in 640x480
struct Forced {
    bool on = false;
    float x = 0, y = 0;
    uint32_t buttons = 0;
};
Forced g_forced;
void ForcedPoll() {
    static const char* e = std::getenv("AUF_POINTER");
    static int frame = 0;
    if (!e || !*e) return;
    frame++;
    g_forced.on = true;
    int at, used;
    float x, y;
    uint32_t b;
    while (std::sscanf(e, "%d:%f:%f:%u%n", &at, &x, &y, &b, &used) == 4 && at <= frame) {
        g_forced.x = x, g_forced.y = y, g_forced.buttons = b;
        e += used;
        if (*e == ',') e++;
    }
}

#ifdef _WIN32
// the arrow while the pointer is up: the runtime hides the cursor over the game (WM_SETCURSOR, and its half second
// timer kCursorTimer, sdk/runtime/app/main.cpp)
constexpr uintptr_t kCursorTimerId = 0x5243;  // kCursorTimer
WNDPROC g_proc = nullptr;
std::atomic<bool> g_arrow{false};

// The pointer's own picture in the menus' colours: the 007 logo's gold, light at the tip and deeper at the tail, a dark
// navy edge and a soft shadow, so it shows on the menus' blue and over the world behind the pause menu. An arrow of
// `size` pixels, its tip the hot spot, made for the window's height (32 at 1080 lines) and again when that changes;
// Windows' arrow when it cannot be made.
HCURSOR MenuCursor(HWND hwnd) {
    static HCURSOR cursor = nullptr;
    static int made = 0;
    RECT r;
    int size = 32;
    if (GetClientRect(hwnd, &r) && r.bottom > 0) size = std::clamp(int(r.bottom * 32 / 1080), 24, 96);
    if (cursor && made == size) return cursor;
    // the arrow's outline in a unit square: tip, down the left edge, the notch, the tail, back up the right edge
    static const float kShape[7][2] = {{0.06f, 0.04f}, {0.06f, 0.80f}, {0.25f, 0.63f}, {0.39f, 0.93f},
                                       {0.52f, 0.87f}, {0.38f, 0.58f}, {0.64f, 0.58f}};
    const auto inside = [](float x, float y) {
        bool in = false;
        for (int i = 0, j = 6; i < 7; j = i++) {
            const float xi = kShape[i][0], yi = kShape[i][1], xj = kShape[j][0], yj = kShape[j][1];
            if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) in = !in;
        }
        return in;
    };
    const auto distance = [](float x, float y) {  // to the outline
        float best = 1e9f;
        for (int i = 0, j = 6; i < 7; j = i++) {
            const float ax = kShape[j][0], ay = kShape[j][1], bx = kShape[i][0] - ax, by = kShape[i][1] - ay;
            const float t = std::clamp(((x - ax) * bx + (y - ay) * by) / (bx * bx + by * by), 0.0f, 1.0f);
            best = std::min(best, std::hypot(x - ax - bx * t, y - ay - by * t));
        }
        return best;
    };
    BITMAPV5HEADER bi{};
    bi.bV5Size = sizeof bi;
    bi.bV5Width = size, bi.bV5Height = -size, bi.bV5Planes = 1, bi.bV5BitCount = 32, bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000, bi.bV5GreenMask = 0x0000FF00, bi.bV5BlueMask = 0x000000FF, bi.bV5AlphaMask = 0xFF000000;
    void* bits = nullptr;
    HDC dc = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, dc);
    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
    HCURSOR next = nullptr;
    if (color && mask && bits) {
        auto* px = static_cast<uint32_t*>(bits);
        const float edge = 0.045f, shadow = 0.05f;
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) {
                float fill = 0, line = 0, shade = 0;  // 4 by 4 samples
                for (int sy = 0; sy < 4; sy++)
                    for (int sx = 0; sx < 4; sx++) {
                        const float u = (x + (sx + 0.5f) / 4) / size, v = (y + (sy + 0.5f) / 4) / size;
                        const bool in = inside(u, v);
                        const float d = distance(u, v);
                        if (in && d > edge * 0.5f) fill += 1;
                        else if (d <= edge * (in ? 0.5f : 0.6f)) line += 1;
                        if (inside(u - shadow, v - shadow) || distance(u - shadow, v - shadow) < edge) shade += 1;
                    }
                fill /= 16, line /= 16, shade /= 16;
                const float t = std::clamp(float(y) / size / 0.9f, 0.0f, 1.0f);  // light gold at the tip, deep at the tail
                const float fr = 255 - 50 * t, fg = 228 - 88 * t, fb = 120 - 96 * t;
                const float lr = 8, lg = 18, lb = 52;  // the edge: the menus' darkest blue
                const float a = std::min(1.0f, fill + line + 0.45f * shade * (1 - fill - line));
                const float cr = a > 0 ? (fill * fr + line * lr) / a : 0, cg = a > 0 ? (fill * fg + line * lg) / a : 0,
                            cb = a > 0 ? (fill * fb + line * lb) / a : 0;
                px[y * size + x] = uint32_t(a * 255 + 0.5f) << 24 | uint32_t(cr + 0.5f) << 16 | uint32_t(cg + 0.5f) << 8 |
                                   uint32_t(cb + 0.5f);
            }
        ICONINFO info{};
        info.fIcon = FALSE;
        info.xHotspot = DWORD(0.06f * size), info.yHotspot = DWORD(0.04f * size);
        info.hbmMask = mask, info.hbmColor = color;
        next = reinterpret_cast<HCURSOR>(CreateIconIndirect(&info));
    }
    if (color) DeleteObject(color);
    if (mask) DeleteObject(mask);
    if (!next) return cursor ? cursor : LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    if (cursor) DestroyCursor(cursor);
    cursor = next, made = size;
    LOGR("auf: the menus' pointer picture made, %d pixels", size);
    return cursor;
}

LRESULT CALLBACK PointerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_arrow.load() && !app::OverlayOpen()) {
        if (msg == WM_SETCURSOR && LOWORD(lp) == HTCLIENT) {
            SetCursor(MenuCursor(hwnd));
            return TRUE;
        }
        if (msg == WM_TIMER && wp == kCursorTimerId) return 0;  // the runtime's timer would hide it while it stands still
    }
    return CallWindowProcW(g_proc, hwnd, msg, wp, lp);
}
void ShowArrow(bool show) {
    HWND hwnd = static_cast<HWND>(app::Window());
    if (!hwnd) return;
    if (!g_proc) {
        g_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(PointerProc)));
        LOGR("auf: the menus' pointer is ready");
    }
    if (g_arrow.exchange(show) != show) PostMessageW(hwnd, WM_SETCURSOR, reinterpret_cast<WPARAM>(hwnd), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
}
#endif

// a press of control c (a d-pad bit or an analog button)
void Press(input::Pad& p, int c) {
    if (c < input::kA) p.buttons |= uint16_t(1u << c);
    else p.analog[c - input::kA] = 255;
}
}  // namespace

bool PointerAt(float& x, float& y) {
    if (g_forced.on) {
        x = g_forced.x, y = g_forced.y;
        return true;
    }
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(app::Window());
    POINT c;
    RECT r;
    if (!input::UsingKeyboard() || GetForegroundWindow() != hwnd || !GetCursorPos(&c) || !ScreenToClient(hwnd, &c) ||
        !GetClientRect(hwnd, &r) || r.right <= 0 || r.bottom <= 0)
        return false;
    // the menus' 4:3 stays in the frame's middle at any width
    x = 640 * (0.5f + (float(c.x) / r.right - 0.5f) * FrameWidth());
    y = 480 * float(c.y) / r.bottom;
    return true;
#else
    (void)x, (void)y;
    return false;
#endif
}

uint32_t PointerButtons() { return g_forced.on ? g_forced.buttons : input::MouseButtons(); }

void MenuMouse(int port, input::Pad& p) {
    constexpr int64_t kPress = 70, kGap = 70;  // ms
    constexpr uint16_t kMouseBits[3] = {0x01, 0x02, 0x04};  // VK_LBUTTON VK_RBUTTON VK_MBUTTON
    if (port != 0) return;
    RefreshButtons();  // the button pictures for the keyboard or the controller, as it changes
    static int pending = 0;  // notches to play, up positive
    static int64_t pulse = 0;
    static bool was_menu = false;
    int notches = input::TakeWheel();  // taken in play too, so none wait for the next menu
    ForcedPoll();
    uint32_t held = PointerButtons();
    using namespace std::chrono;
    int64_t now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    bool menu = MenuUp();
#ifdef _WIN32
    if (kImage != kDefault) ShowArrow(PointerWanted() && input::UsingKeyboard());
#endif
    static bool was = false;
    if (menu != was) LOGR("auf: %s", menu ? "a menu is up: the mouse plays A, B and the d-pad" : "out of the menus");
    was = menu;
    if (kImage == kBond && !menu && Zoomed() && notches) {  // through a scope the wheel zooms
        ScopeZoom(notches);
        notches = 0;
    }
    // on foot Enter is the console's A everywhere, and where Bond is not the player's to move (a cutscene, a hint's "Press A") the left
    // button too
    if (kImage == kBond && !menu && input::UsingKeyboard() && (KeyHeld(0x0D) || (!PlayerFree() && (held & 1)))) Press(p, input::kA);
    int next = input::kRight, prev = input::kLeft;
    if (menu) next = input::kDown, prev = input::kUp;
    else if (kImage == kDriving) next = input::kBlack, prev = input::kWhite;
    else if (kImage == kBond) next = input::kDown, prev = input::kUp;  // the d-pad's up and down are the weapon on foot
    if (kImage == kDriving && !menu && OnGun() && input::UsingKeyboard()) {
        // the weapons as on foot: the d-pad's down and up, by 1 and 3, Q and the wheel
        next = input::kDown, prev = input::kUp;
        if (KeyHeld('1') || KeyHeld('Q')) Press(p, input::kDown);
        if (KeyHeld('3')) Press(p, input::kUp);
        for (int c : {input::kA, input::kX, input::kLT, input::kRT}) p.analog[c - input::kA] = 0;
        if (held & 1) Press(p, input::kRT);
        if (held & 2) Press(p, input::kLT);
        if (KeyHeld('R')) Press(p, input::kA);  // reload: povcfg0.def's GAMEACTION_POVRELOAD is +button0, A (X does nothing there)
    }
    if (menu) {
        if (KeyHeld(0x0D)) p.analog[0] = 255;  // Enter: A
        // the arrow keys are the d-pad in any menu (DRIVING.XBE's keys have them on the left stick, once)
        const int vks[4] = {0x26, 0x28, 0x25, 0x27}, pads[4] = {input::kUp, input::kDown, input::kLeft, input::kRight};
        for (int i = 0; i < 4; i++)
            if (KeyHeld(vks[i])) {
                Press(p, pads[i]);
                if (i < 2) p.ly = 0;
                else p.lx = 0;
            }
        // the buttons out of what they play in play
        for (int c = 0; c < input::kA + 8; c++) {
            auto keys = input::ControlKeys(c);
            bool mouse = false;
            for (int b = 0; b < 3; b++)
                if (held >> b & 1) mouse |= keys[0] == kMouseBits[b] || keys[1] == kMouseBits[b];
            if (!mouse) continue;
            if (c < input::kA) p.buttons = uint16_t(p.buttons & ~(1u << c));
            else p.analog[c - input::kA] = 0;
        }
        // A: once the pointer's item is the current one; a press off every item does nothing until it is let go
        // what it plays is read a poll after the press: the game's thread must have seen the pointer where it now is
        static bool was_held = false, arming = false;
        static int armed = kClickNone;
        int click = MenuClick();
        if (arming) {
            armed = click;
            LOGR("auf: menu click plays %d", armed);
        }
        const bool playing = (held & 1) && !arming && was_held;
        arming = (held & 1) && !was_held;
        was_held = held & 1;
        if (playing) {
            if (armed == kClickLeft) Press(p, input::kLeft);
            else if (armed == kClickRight) Press(p, input::kRight);
            else if (armed == kClickDown) Press(p, input::kDown);
            else if (armed != kClickNone && click == kClickGo) p.analog[0] = 255;
        }
    }
    // the wheel: a d-pad press a notch
    if (menu != was_menu) pending = 0;
    was_menu = menu;
    if (notches && !pending) pulse = now;
    pending = std::clamp(pending + notches, -8, 8);
    if (pending) {
        int64_t t = now - pulse;
        if (t < kPress) {
            Press(p, pending > 0 ? prev : next);
        } else if (t >= kPress + kGap) {
            pending += pending > 0 ? -1 : 1;
            pulse = now;
        }
    }
}

}  // namespace auf
