#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <fstream>
#include <iterator>
#include "firmware_image.h"

std::vector<uint8_t> app() {
    std::vector<uint8_t> image(4096, 0);
    image[0] = 0xe9; image[12] = 9;
    image[32] = 0x32; image[33] = 0x54; image[34] = 0xcd; image[35] = 0xab;
    memcpy(image.data()+1000, FIRMWARE_TARGET, sizeof(FIRMWARE_TARGET)-1);
    return image;
}
bool valid(const std::vector<uint8_t>& image, bool staged=false) {
    FirmwareImageCheck check;
    for (size_t pos=0; pos<image.size(); ++pos) check.add(image.data()+pos, 1);
    return check.valid(staged);
}
int main(int argc, char** argv) {
    if (argc > 1) {
        std::ifstream file(argv[1], std::ios::binary);
        assert(file.good());
        std::vector<uint8_t> binary((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        assert(valid(binary));
        puts("Built application passes the OTA target/header checks");
    }
    auto image=app();
    assert(valid(image)); // Marker split across every possible byte boundary.
    for (size_t chunk : {7u, 64u, 1024u, 1460u}) {
        FirmwareImageCheck check;
        for(size_t pos=0; pos<image.size(); pos+=chunk)
            check.add(image.data()+pos, image.size()-pos < chunk ? image.size()-pos : chunk);
        assert(check.valid());
    }
    image[0]=0xff; assert(!valid(image)); assert(valid(image,true));
    image=app(); image[12]=0; assert(!valid(image)); // Other ESP32 variant.
    image=app(); image[32]=0; assert(!valid(image)); assert(!valid(image,true)); // Bootloader.
    image=app(); memset(image.data()+1000,0,sizeof(FIRMWARE_TARGET)); assert(!valid(image)); // Wrong project/PCB.
    image=app(); image.resize(100); assert(!valid(image));
    std::vector<uint8_t> factory(65536,0xff);
    image=app(); factory.insert(factory.end(),image.begin(),image.end());
    assert(!valid(factory)); assert(!valid(factory,true));
    size_t size=42;
    for(const char* bad : {"", "0", "287", "-1", "512x", " 512", "33554432", "9999999999999999999999999999"})
        assert(!firmwareSize(bad,0x330000,size));
    assert(firmwareSize("288",0x330000,size) && size==288);
    assert(firmwareSize("3342336",0x330000,size) && size==0x330000);
    assert(!firmwareSize("3342337",0x330000,size));
    puts("Firmware header, target marker, split chunks, factory-image rejection and size validation passed");
}
