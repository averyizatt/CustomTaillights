#include "animations.h"
#include "taillight.h"
#include "settings.h"

namespace {
int rows(int seg) { return seg == SEG_MAIN ? MAIN_ROWS : STRIP_ROWS; }
int cols(int seg) { return seg == SEG_MAIN ? MAIN_COLS : STRIP_COLS; }
int smaller(int a, int b) { return a < b ? a : b; }
int larger(int a, int b) { return a > b ? a : b; }
int magnitude(int x) { return x < 0 ? -x : x; }
CRGB scaled(CRGB c, unsigned level) {
    return CRGB(c.r * level / 255u, c.g * level / 255u, c.b * level / 255u);
}
CRGB runColour() {
    const unsigned dim = constrain(g_settings.brightness_dim, 5, RUNNING_BRIGHTNESS_MAX_PERCENT);
    return CRGB(g_settings.run_r * dim / 100u, g_settings.run_g * dim / 100u,
                g_settings.run_b * dim / 100u);
}
CRGB brakeColour() { return CRGB(g_settings.brake_r, g_settings.brake_g, g_settings.brake_b); }
CRGB turnColour() { return CRGB(g_settings.turn_r, g_settings.turn_g, g_settings.turn_b); }
unsigned long turnPeriod() { return constrain(g_settings.turn_blink_ms, 200, 1500); }
unsigned long showPeriod(unsigned long base) {
    return base * 100UL / constrain(g_settings.show_speed, 50, 200);
}
int phase256(unsigned long now, unsigned long period) {
    return static_cast<int>((now % period) * 256UL / period);
}
int edgeDistance(int r, int c, int h, int w) {
    return smaller(smaller(r, h - 1 - r), smaller(c, w - 1 - c));
}
int rectRadius(int r, int c, int h, int w) {
    return larger(magnitude(2 * c - (w - 1)) * 128 / (w - 1),
                  magnitude(2 * r - (h - 1)) * 128 / (h - 1));
}
void turnBase(TailLight& side, LightState state) {
    side.fill(CRGB::Black);
    if (state == LightState::BRAKE_TURN) {
        // Clear top strip signals direction; both red sections hold the brake.
        side.fillSegment(SEG_BOT_STRIP, brakeColour());
        side.fillSegment(SEG_MAIN, brakeColour());
    }
}
}  // namespace

// A stable outline on each segment with a slow highlight along its perimeter.
void AnimRunContour::update(TailLight& side, LightState, unsigned long nowMs) {
    const CRGB colour = runColour();
    for (int seg = 0; seg < NUM_SEGMENTS; ++seg) {
        const int h = rows(seg), w = cols(seg), perimeter = 2 * (w + h) - 4;
        const int head = static_cast<int>((nowMs % 4800UL) * perimeter / 4800UL);
        for (int r = 0; r < h; ++r) for (int c = 0; c < w; ++c) {
            unsigned level = 35;
            if (edgeDistance(r, c, h, w) == 0) {
                const int position = r == 0 ? c : (c == w - 1 ? w - 1 + r
                    : (r == h - 1 ? w + h - 2 + w - 1 - c : perimeter - r));
                const int tail = (head - position + perimeter) % perimeter;
                level = tail < 8 ? 255u - tail * 10u : 170u;
            }
            side.setPixel(seg, r, c, scaled(colour, level));
        }
    }
}

// Three raked blades on the main panel, with matching fine rails on the strips.
void AnimRunLouvers::update(TailLight& side, LightState, unsigned long nowMs) {
    const CRGB colour = runColour();
    const int sheen = phase256(nowMs, 5600UL);
    for (int seg = 0; seg < NUM_SEGMENTS; ++seg) {
        const int h = rows(seg), w = cols(seg);
        for (int r = 0; r < h; ++r) for (int c = 0; c < w; ++c) {
            const bool blade = seg == SEG_MAIN ? ((c + (h - 1 - r) / 3) % 6 < 3)
                                               : (r == 1 || r == h - 2);
            const int distance = magnitude((w - 1 - c) * 255 / (w - 1) - sheen);
            const unsigned level = blade ? (distance < 36 ? 255u - distance * 2u : 180u) : 30u;
            side.setPixel(seg, r, c, scaled(colour, level));
        }
    }
}

void AnimBrakeEdgeLock::begin(TailLight&, LightState) { _startMs = millis(); }
void AnimBrakeEdgeLock::update(TailLight& side, LightState, unsigned long nowMs) {
    const unsigned long elapsed = nowMs - _startMs;
    const CRGB colour = brakeColour();
    for (int seg = 0; seg < NUM_SEGMENTS; ++seg) {
        const int h = rows(seg), w = cols(seg);
        for (int r = 0; r < h; ++r) for (int c = 0; c < w; ++c) {
            // Every pixel lights immediately; the border locks inward to full.
            const unsigned long arrival = edgeDistance(r, c, h, w) * 60UL;
            side.setPixel(seg, r, c, scaled(colour, elapsed >= arrival ? 255 : 192));
        }
    }
}

void AnimTurnArrowhead::begin(TailLight&, LightState) { _startMs = millis(); }
void AnimTurnArrowhead::update(TailLight& side, LightState state, unsigned long nowMs) {
    turnBase(side, state);
    const unsigned long half = turnPeriod() / 2, phase = (nowMs - _startMs) % turnPeriod();
    if (phase >= half) return;
    const int front = static_cast<int>(phase * 400UL / half);
    for (int seg = 0; seg < NUM_SEGMENTS; ++seg) {
        if (state == LightState::BRAKE_TURN && seg != SEG_TOP_STRIP) continue;
        const int h = rows(seg), w = cols(seg);
        for (int r = 0; r < h; ++r) for (int c = 0; c < w; ++c) {
            // Logical column zero is OUTBOARD on both sides; never mirror twice.
            const int travel = (w - 1 - c) * 255 / (w - 1);
            const int wing = magnitude(2 * r - (h - 1)) * 40 / (h - 1);
            if (travel + wing <= front) side.setPixel(seg, r, c, turnColour());
        }
    }
}

void AnimTurnThreeBar::begin(TailLight&, LightState) { _startMs = millis(); }
void AnimTurnThreeBar::update(TailLight& side, LightState state, unsigned long nowMs) {
    turnBase(side, state);
    const unsigned long half = turnPeriod() / 2, phase = (nowMs - _startMs) % turnPeriod();
    if (phase >= half) return;
    const int stage = smaller(2, static_cast<int>(phase * 4UL / half));
    for (int seg = 0; seg < NUM_SEGMENTS; ++seg) {
        if (state == LightState::BRAKE_TURN && seg != SEG_TOP_STRIP) continue;
        const int h = rows(seg), w = cols(seg);
        for (int c = 0; c < w; ++c) {
            const int inward = w - 1 - c, group = inward * 3 / w;
            const bool seam = inward > 0 && group != (inward - 1) * 3 / w;
            if (group <= stage && !seam)
                for (int r = 0; r < h; ++r) side.setPixel(seg, r, c, turnColour());
        }
    }
}

// Main-panel exhaust rings, with amber jets in the clear strip and red below.
void AnimShowAfterburner::update(TailLight& side, LightState, unsigned long nowMs) {
    const int phase = phase256(nowMs, showPeriod(1700UL)) * 96 / 256;
    for (int seg = 0; seg < NUM_SEGMENTS; ++seg) {
        const int h = rows(seg), w = cols(seg);
        for (int r = 0; r < h; ++r) for (int c = 0; c < w; ++c) {
            const int radius = rectRadius(r, c, h, w);
            const int ring = (radius * 3 - phase + 512) % 96;
            const int core = larger(0, 72 - radius);
            const int intensity = smaller(255, 12 + core * 2 + (ring < 24 ? (24 - ring) * 6 : 0));
            const CRGB colour = seg == SEG_TOP_STRIP
                ? CRGB(intensity, intensity * (80 + core) / 255, core / 2)
                : CRGB(intensity, 0, 0);
            side.setPixel(seg, r, c, colour);
        }
    }
}

// Perspective gates radiate from the panel center, with converging guide rails.
void AnimShowTunnel::update(TailLight& side, LightState, unsigned long nowMs) {
    const int phase = phase256(nowMs, showPeriod(2400UL)) * 80 / 256;
    for (int seg = 0; seg < NUM_SEGMENTS; ++seg) {
        const int h = rows(seg), w = cols(seg);
        for (int r = 0; r < h; ++r) for (int c = 0; c < w; ++c) {
            const int x = magnitude(2 * c - (w - 1)) * 128 / (w - 1);
            const int y = magnitude(2 * r - (h - 1)) * 128 / (h - 1);
            const int radius = larger(x, y), gate = (radius * 2 - phase + 512) % 80;
            const bool rail = magnitude(x - y) < 18;
            const int level = gate < 18 ? 240 - gate * 8 : (rail ? 85 : 8);
            side.setPixel(seg, r, c, seg == SEG_TOP_STRIP
                ? CRGB(level / 8, level * 3 / 4, level) : CRGB(level, 0, 0));
        }
    }
}

// Two diagonal ribbons interlace; their alternating crossings give a woven depth.
void AnimShowApexWeave::update(TailLight& side, LightState, unsigned long nowMs) {
    const int phase = phase256(nowMs, showPeriod(3200UL)) * 96 / 256;
    for (int seg = 0; seg < NUM_SEGMENTS; ++seg) {
        const int h = rows(seg), w = cols(seg);
        for (int r = 0; r < h; ++r) for (int c = 0; c < w; ++c) {
            const int x = c * 256 / w, y = r * 128 / h;
            const int a = (x + y - phase + 512) % 96;
            const int b = (x - y + phase + 512) % 96;
            const int first = a < 28 ? 240 - a * 5 : 0;
            const int second = b < 28 ? 210 - b * 4 : 0;
            const bool over = ((c / 4 + r / 2) & 1) == 0;
            CRGB colour;
            if (first && (!second || over)) colour = CRGB(first, first / 6, first / 2);
            else if (second) colour = CRGB(second / 4, second, second / 2);
            else colour = CRGB(6, 0, 0);
            if (seg != SEG_TOP_STRIP) colour = CRGB(larger(first, second) + 6, 0, 0);
            side.setPixel(seg, r, c, colour);
        }
    }
}
