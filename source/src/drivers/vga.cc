#include <drivers/vga.h>

using namespace os::common;
using namespace os::drivers;

// Define a fixed-size array based on the maximum resolution
constexpr uint32_t MaxWidth = 1280;
constexpr uint32_t MaxHeight = 1024;
uint8_t pixels[MaxWidth * MaxHeight];

VideoGraphicsArray::VideoGraphicsArray() :
    miscPort(0x3c2),
    crtcIndexPort(0x3d4),
    crtcDataPort(0x3d5),
    sequencerIndexPort(0x3c4),
    sequencerDataPort(0x3c5),
    graphicsControllerIndexPort(0x3ce),
    graphicsControllerDataPort(0x3cf),
    attributeControllerIndexPort(0x3c0),
    attributeControllerReadPort(0x3c1),
    attributeControllerWritePort(0x3c0),
    attributeControllerResetPort(0x3da) {
}

VideoGraphicsArray::~VideoGraphicsArray() {
}

void VideoGraphicsArray::WriteRegisters(uint8_t* registers) {
    // misc
    miscPort.Write(*(registers++));

    // sequencer
    for (uint8_t i = 0; i < 5; i++) {
        sequencerIndexPort.Write(i);
        sequencerDataPort.Write(*(registers++));
    }

    // cathode ray tube controller
    crtcIndexPort.Write(0x03);
    crtcDataPort.Write(crtcDataPort.Read() | 0x80);
    crtcIndexPort.Write(0x11);
    crtcDataPort.Write(crtcDataPort.Read() & ~0x80);

    registers[0x03] = registers[0x03] | 0x80;
    registers[0x11] = registers[0x11] & ~0x80;

    for (uint8_t i = 0; i < 25; i++) {
        crtcIndexPort.Write(i);
        crtcDataPort.Write(*(registers++));
    }

    // graphics controller
    for (uint8_t i = 0; i < 9; i++) {
        graphicsControllerIndexPort.Write(i);
        graphicsControllerDataPort.Write(*(registers++));
    }

    // attribute controller
    for (uint8_t i = 0; i < 21; i++) {
        attributeControllerResetPort.Read();
        attributeControllerIndexPort.Write(i);
        attributeControllerWritePort.Write(*(registers++));
    }

    attributeControllerResetPort.Read();
    attributeControllerIndexPort.Write(0x20);
}

bool VideoGraphicsArray::SupportsMode(uint32_t width, uint32_t height, uint32_t colordepth) {
    return width == 1280 && height == 1024 && colordepth == 8;
}

bool VideoGraphicsArray::SetMode(uint32_t width, uint32_t height, uint32_t colordepth) {
    if (!SupportsMode(width, height, colordepth)) {
        return false;
    }

    unsigned char g_1280x1024[] = {
        /* misc */
        0x63,
        /* seq */
        0x03, 0x01, 0x0F, 0x00, 0x0E,
        /* crtc */
        0x1F, 0x3F, 0x40, 0x82, 0x54, 0x80, 0xBF, 0x1F,
        0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3,
        0xFF,
        /* gc */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF,
        /* ac */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x41, 0x00, 0x0F, 0x00, 0x00
    };

    WriteRegisters(g_1280x1024);

    return true;
}

uint8_t* VideoGraphicsArray::GetFrameBufferSegment() {
    graphicsControllerIndexPort.Write(0x06);
    uint8_t segmentNumber = graphicsControllerDataPort.Read() & (3 << 2);

    switch (segmentNumber) {
        default:
        case 0 << 2: return (uint8_t*)0x00000;
        case 1 << 2: return (uint8_t*)0xa0000;
        case 2 << 2: return (uint8_t*)0xb0000;
        case 3 << 2: return (uint8_t*)0xb8000;
    }
}

void VideoGraphicsArray::PutPixel(int32_t x, int32_t y, uint8_t colorIndex) {
    if (x < 0 || MaxWidth <= x || y < 0 || MaxHeight <= y) {
        return;
    }

    pixels[MaxWidth * y + x] = colorIndex;
}

void VideoGraphicsArray::FillRectangle(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t color) {
    for (int32_t Y = y; Y < y + h; Y++) {
        for (int32_t X = x; X < x + w; X++) {
            this->PutPixel(X, Y, color);
        }
    }
}

void VideoGraphicsArray::DrawToScreen() {
    for (uint32_t y = 0; y < MaxHeight; y++) {
        for (uint32_t x = 0; x < MaxWidth; x++) {
            uint8_t* pixelAddress = this->GetFrameBufferSegment() + ((y << 8) + (y << 6) + x);
            *pixelAddress = pixels[MaxWidth * y + x];
        }
    }
}