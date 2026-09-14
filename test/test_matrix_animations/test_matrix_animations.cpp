#include <cassert>
#include <cstdio>
#include <cstring>
#include "taillight.h"
#include "settings.h"

unsigned long testMillis = 1000;
Settings g_settings{};
// Registry dispatch is checked separately; tests call the concrete effects.
Animation* AnimationRegistry::get(LightState, bool) { return nullptr; }

struct Panel {
    CRGB buffer[LEDS_PER_SIDE + 2];
    TailLight light;
    Panel(bool driver) : light(buffer + 1, driver) {
        for (auto& p : buffer) p = CRGB(7, 13, 19);
    }
    void check() {
        assert((buffer[0] == CRGB(7, 13, 19)));
        assert((buffer[LEDS_PER_SIDE + 1] == CRGB(7, 13, 19)));
        for (int i = SEG_OFFSET[1]; i < LEDS_PER_SIDE; ++i) {
            assert(light[i].g == 0 && light[i].b == 0);
        }
    }
};

int index(int seg, int r, int c) {
    const int w = seg == SEG_MAIN ? MAIN_COLS : STRIP_COLS;
    const int wiredRow = seg == SEG_MAIN ? MAIN_ROWS - 1 - r : r;
    return SEG_OFFSET[seg] + wiredRow * w + ((wiredRow & 1) ? w - 1 - c : c);
}
void mirrored(Panel& d, Panel& p) {
    for (int seg = 0; seg < NUM_SEGMENTS; ++seg) {
        const int w = seg == SEG_MAIN ? MAIN_COLS : STRIP_COLS;
        const int h = seg == SEG_MAIN ? MAIN_ROWS : STRIP_ROWS;
        for (int r = 0; r < h; ++r) for (int c = 0; c < w; ++c)
            assert(d.light[index(seg, r, c)] == p.light[index(seg, r, w - 1 - c)]);
    }
}
void defaults() {
    g_settings.brake_r = 255;
    g_settings.turn_r = 255; g_settings.turn_g = 110;
    g_settings.run_r = 255; g_settings.brightness_dim = 25;
    g_settings.turn_blink_ms = 600; g_settings.show_speed = 100;
}
void test_all_effects() {
    AnimRunContour contour; AnimRunLouvers louvers; AnimBrakeEdgeLock brake;
    AnimTurnArrowhead arrow; AnimTurnThreeBar bars;
    AnimShowAfterburner burner; AnimShowTunnel tunnel; AnimShowApexWeave weave;
    Animation* effects[] = {&contour, &louvers, &brake, &arrow, &bars, &burner, &tunnel, &weave};
    for (int i = 0; i < 8; ++i) {
        Panel d(true), p(false);
        const LightState state = i < 2 ? LightState::RUNNING : (i == 2 ? LightState::BRAKE : (i < 5 ? LightState::TURN : LightState::SHOW));
        effects[i]->begin(d.light, state);
        for (unsigned long t = 1000; t < 8000; t += 17) {
            effects[i]->update(d.light, state, t);
            effects[i]->update(p.light, state, t);
            d.check(); p.check(); mirrored(d, p);
            if (i < 2) for (int n = 0; n < LEDS_PER_SIDE; ++n) assert(d.light[n].r <= 63);
            if (i == 2) for (int n = 0; n < LEDS_PER_SIDE; ++n) {
                assert(d.light[n].r >= 192);
                if (t >= 1300) assert(d.light[n].r == 255);
            }
        }
    }
}
template<class Effect> void test_turn() {
    Effect left, right;
    Panel d(true), p(false);
    testMillis = 1000; left.begin(d.light, LightState::BRAKE_TURN);
    testMillis = 1200; right.begin(p.light, LightState::BRAKE_TURN);
    // Starting the right signal must not reset the left signal's phase.
    left.update(d.light, LightState::BRAKE_TURN, 1350);
    right.update(p.light, LightState::BRAKE_TURN, 1350);
    unsigned rightLit = 0;
    for (int i = 0; i < STRIP_LEDS; ++i) { assert(d.light[i] == CRGB::Black); rightLit += p.light[i].r > 0; }
    assert(rightLit > 0);
    for (int t = 1200; t < 2300; t += 10) {
        left.update(d.light, LightState::BRAKE_TURN, t);
        for (int i = SEG_OFFSET[1]; i < LEDS_PER_SIDE; ++i) assert(d.light[i] == CRGB(255, 0, 0));
    }
    testMillis = 1000; left.begin(d.light, LightState::TURN);
    left.update(d.light, LightState::TURN, 1025);
    assert(d.light[index(SEG_TOP_STRIP, 2, STRIP_COLS - 1)].r > 0); // starts inboard
    assert(d.light[index(SEG_TOP_STRIP, 2, 0)] == CRGB::Black);
    left.update(d.light, LightState::TURN, 1290);
    assert(d.light[index(SEG_TOP_STRIP, 2, 0)].r > 0); // finishes outboard
    left.update(d.light, LightState::TURN, 1350);
    for (int i = 0; i < LEDS_PER_SIDE; ++i) assert(d.light[i] == CRGB::Black);
    // Defensive timing bounds even if settings arrive as zero.
    g_settings.turn_blink_ms = 0;
    left.update(d.light, LightState::TURN, 1010);
    g_settings.turn_blink_ms = 600;
}

void preview() {
    AnimRunContour a; AnimRunLouvers b; AnimBrakeEdgeLock c;
    AnimTurnArrowhead d; AnimTurnThreeBar e;
    AnimShowAfterburner f; AnimShowTunnel g; AnimShowApexWeave h;
    Animation* effects[] = {&a, &b, &c, &d, &e, &f, &g, &h};
    const char* names[] = {"Contour Glide", "Fox Louvers", "Edge Lock", "Arrowhead Sweep", "Three-Bar Relay", "Afterburner", "Tunnel Grid", "Apex Weave"};
    const unsigned long periods[] = {4800, 5600, 1200, 600, 600, 1700, 2400, 3200};
    std::printf("[");
    for (int n = 0; n < 8; ++n) {
        Panel panel(true);
        testMillis = 0; effects[n]->begin(panel.light, LightState::OFF);
        std::printf("%s{\"name\":\"%s\",\"period\":%lu,\"frames\":[", n ? "," : "", names[n], periods[n]);
        for (int frame = 0; frame < 48; ++frame) {
            const auto state = n < 2 ? LightState::RUNNING : (n == 2 ? LightState::BRAKE : (n < 5 ? LightState::TURN : LightState::SHOW));
            effects[n]->update(panel.light, state, periods[n] * frame / 48);
            std::printf("%s\"", frame ? "," : "");
            for (int seg = 0; seg < NUM_SEGMENTS; ++seg) {
                const int w = seg == SEG_MAIN ? MAIN_COLS : STRIP_COLS;
                const int h = seg == SEG_MAIN ? MAIN_ROWS : STRIP_ROWS;
                for (int r = 0; r < h; ++r) for (int col = 0; col < w; ++col) {
                    const CRGB px = panel.light[index(seg, r, col)];
                    std::printf("%02x%02x%02x", px.r, px.g, px.b);
                }
            }
            std::printf("\"");
        }
        std::printf("]}");
    }
    std::printf("]\n");
}
int main(int argc, char** argv) {
    defaults();
    if (argc > 1 && std::strcmp(argv[1], "--preview") == 0) { preview(); return 0; }
    testMillis = 1000;
    test_all_effects();
    test_turn<AnimTurnArrowhead>(); test_turn<AnimTurnThreeBar>();
    std::puts("Matrix animation tests passed");
}
