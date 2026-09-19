#pragma once
#include <Arduino.h>
#include <vector>
enum EOrder { GRB };
struct CRGB { uint8_t r = 0, g = 0, b = 0; };
template<EOrder Order> struct PixelController {
    CRGB* data;
    int count, index = 0;
    int size() const { return count; }
    bool has(int) const { return index < count; }
    uint8_t loadAndScale0() const { return data[index].g; }
    uint8_t loadAndScale1() const { return data[index].r; }
    uint8_t loadAndScale2() const { return data[index].b; }
    void advanceData() { ++index; }
    void stepDithering() {}
};
template<EOrder Order> class CPixelLEDController {
public:
    CRGB* data = nullptr;
    int count = 0;
    virtual void init() = 0;
    virtual void showPixels(PixelController<Order>& pixels) = 0;
    virtual ~CPixelLEDController() = default;
};
struct TestFastLED {
    std::vector<CPixelLEDController<GRB>*> controllers;
    void addLeds(CPixelLEDController<GRB>* controller, CRGB* data, int count) {
        controller->data = data; controller->count = count; controller->init();
        controllers.push_back(controller);
    }
    void show() {
        for (auto* c : controllers) {
            PixelController<GRB> pixels{c->data, c->count};
            c->showPixels(pixels);
        }
    }
};
inline TestFastLED FastLED;
