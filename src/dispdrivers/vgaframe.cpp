// File: vgaframe.cpp - unions that describe DMA buffers for video
// By: Curtis Whitley

#include "vgaframe.h"

void debug_log(const char* fmt, ...);

static VgaTiming VGA_320x200_70Hz {"320x200@70Hz", 12587500, 320, 328, 376, 400, 200, 206, 207, 224, HSNEG, VSNEG, DBL};
static VgaTiming VGA_320x200_75Hz {"320x200@75Hz", 12930000, 320, 352, 376, 408, 200, 208, 211, 229, HSNEG, VSNEG, DBL};
static VgaTiming VGA_320x200_75HzRetro {"320x200@75Hz", 12930000, 320, 352, 376, 408, 200, 208, 211, 229, HSNEG, VSNEG, DBL, MSB};
static VgaTiming QVGA_320x240_60Hz {"320x240@60Hz", 12600000, 320, 328, 376, 400, 240, 245, 246, 262, HSNEG, VSNEG, DBL};
static VgaTiming SVGA_320x256_60Hz {"320x256@60Hz", 27000000, 320, 332, 360, 424, 256, 257, 258, 267, HSNEG, VSNEG, QUAD};
static VgaTiming VGA_400x300_60Hz {"400x300@60Hz", 20000000, 400, 420, 484, 528, 300, 300, 302, 314, HSNEG, VSNEG, DBL};
static VgaTiming VGA_480x300_75Hz {"480x300@75Hz", 31220000, 480, 504, 584, 624, 300, 319, 322, 333, HSNEG, VSNEG, DBL};
static VgaTiming VGA_512x384_60Hz {"512x384@60Hz", 32500000, 512, 524, 592, 672, 384, 385, 388, 403, HSNEG, VSNEG, DBL};
static VgaTiming VGA_512x192_60Hz {"512x192@60Hz", 32500000, 512, 524, 592, 672, 192, 193, 194, 201, HSNEG, VSNEG, QUAD};
static VgaTiming SVGA_640x360_60Hz {"640x360@60Hz", 37240000, 640, 734, 802, 832, 360, 361, 362, 373, HSPOS, VSPOS, DBL};
static VgaTiming VGA_640x480_60Hz {"640x480@60Hz", 25175000, 640, 656, 752, 800, 480, 490, 492, 525, HSNEG, VSNEG};
static VgaTiming VGA_640x240_60Hz {"640x240@60Hz", 25175000, 640, 656, 752, 800, 240, 245, 246, 262, HSNEG, VSNEG, DBL};
static VgaTiming QSVGA_640x512_60Hz {"640x512@60Hz", 54000000, 640, 664, 720, 844, 512, 513, 515, 533, HSNEG, VSNEG, DBL};
static VgaTiming QSVGA_640x256_60Hz {"640x256@60Hz", 54000000, 640, 664, 720, 844, 256, 257, 258, 267, HSNEG, VSNEG, QUAD};
static VgaTiming SVGA_800x600_60Hz {"800x600@60Hz", 40000000, 800, 840, 968, 1056, 600, 601, 605, 628, HSNEG, VSNEG};
static VgaTiming SVGA_960x540_60Hz {"960x540@60Hz", 37260000, 960, 976, 1008, 1104, 540, 542, 548, 563, HSPOS, VSPOS};
static VgaTiming SVGA_1024x768_60Hz {"1024x768@60Hz", 65000000, 1024, 1048, 1184, 1344, 768, 771, 777, 806, HSNEG, VSNEG};
static VgaTiming SVGA_1024x768_70Hz {"1024x768@70Hz", 75000000, 1024, 1048, 1184, 1328, 768, 771, 777, 806, HSNEG, VSNEG};
static VgaTiming SVGA_1024x768_75Hz {"1024x768@75Hz", 78800000, 1024, 1040, 1136, 1312, 768, 769, 772, 800, HSPOS, VSPOS};
static VgaTiming SVGA_1280x720_60Hz {"1280x720@60Hz", 74480000, 1280, 1468, 1604, 1664, 720, 721, 724, 746, HSPOS, VSPOS};

static const VgaSettings vgaSettings[] = {
    {  0,   2, OLD, SGL, BUF_UNION_SIZE(1024, 768, 2), BUF_LEFTOVER_SIZE(1024, 768, 2), SVGA_1024x768_60Hz },
    {  0,  16, NEW, SGL, BUF_UNION_SIZE(640, 480, 16), BUF_LEFTOVER_SIZE(640, 480, 16), VGA_640x480_60Hz },
    {  1,  16, OLD, SGL, BUF_UNION_SIZE(512, 384, 16), BUF_LEFTOVER_SIZE(512, 384, 16), VGA_512x384_60Hz },
    {  1,   4, NEW, SGL, BUF_UNION_SIZE(640, 480, 4), BUF_LEFTOVER_SIZE(640, 480, 4), VGA_640x480_60Hz },
    {  2,  64, OLD, SGL, BUF_UNION_SIZE(320, 200, 64), BUF_LEFTOVER_SIZE(320, 200, 64), VGA_320x200_75Hz },
    {  2,   2, NEW, SGL, BUF_UNION_SIZE(640, 480, 2), BUF_LEFTOVER_SIZE(640, 480, 2), VGA_640x480_60Hz },
    {  3,  16, OLD, SGL, BUF_UNION_SIZE(640, 480, 16), BUF_LEFTOVER_SIZE(640, 480, 16), VGA_640x480_60Hz },
    {  3,  64, NEW, SGL, BUF_UNION_SIZE(640, 240, 64), BUF_LEFTOVER_SIZE(640, 240, 64), VGA_640x240_60Hz },
    {  4,  16, NEW, SGL, BUF_UNION_SIZE(640, 240, 16), BUF_LEFTOVER_SIZE(640, 240, 16), VGA_640x240_60Hz },
    {  5,   4, NEW, SGL, BUF_UNION_SIZE(640, 240, 4), BUF_LEFTOVER_SIZE(640, 240, 4), VGA_640x240_60Hz },
    {  6,   2, NEW, SGL, BUF_UNION_SIZE(640, 240, 2), BUF_LEFTOVER_SIZE(640, 240, 2), VGA_640x240_60Hz },
    {  7,  16, NEW, SGL, BUF_UNION_SIZE(640, 480, 16), BUF_LEFTOVER_SIZE(640, 480, 16), VGA_640x480_60Hz },
    {  8,  64, NEW, SGL, BUF_UNION_SIZE(320, 240, 64), BUF_LEFTOVER_SIZE(320, 240, 64), QVGA_320x240_60Hz },
    {  9,  16, NEW, SGL, BUF_UNION_SIZE(320, 240, 16), BUF_LEFTOVER_SIZE(320, 240, 16), QVGA_320x240_60Hz },
    { 10,   4, NEW, SGL, BUF_UNION_SIZE(320, 240, 4), BUF_LEFTOVER_SIZE(320, 240, 4), QVGA_320x240_60Hz },
    { 11,   2, NEW, SGL, BUF_UNION_SIZE(320, 240, 2), BUF_LEFTOVER_SIZE(320, 240, 2), QVGA_320x240_60Hz },
    { 12,  64, NEW, SGL, BUF_UNION_SIZE(320, 200, 64), BUF_LEFTOVER_SIZE(320, 200, 64), VGA_320x200_70Hz },
    { 13,  16, NEW, SGL, BUF_UNION_SIZE(320, 200, 16), BUF_LEFTOVER_SIZE(320, 200, 16), VGA_320x200_70Hz },
    { 14,   4, NEW, SGL, BUF_UNION_SIZE(320, 200, 4), BUF_LEFTOVER_SIZE(320, 200, 4), VGA_320x200_70Hz },
    { 15,   2, NEW, SGL, BUF_UNION_SIZE(320, 200, 2), BUF_LEFTOVER_SIZE(320, 200, 2), VGA_320x200_70Hz },
    { 16,   4, NEW, SGL, BUF_UNION_SIZE(800, 600, 4), BUF_LEFTOVER_SIZE(800, 600, 4), SVGA_800x600_60Hz },
    { 17,   2, NEW, SGL, BUF_UNION_SIZE(800, 600, 2), BUF_LEFTOVER_SIZE(800, 600, 2), SVGA_800x600_60Hz },
    { 18,   2, NEW, SGL, BUF_UNION_SIZE(1024, 768, 2), BUF_LEFTOVER_SIZE(1024, 768, 2), SVGA_1024x768_60Hz },
    { 19,   4, NEW, SGL, BUF_UNION_SIZE(1024, 768, 4), BUF_LEFTOVER_SIZE(1024, 768, 4), SVGA_1024x768_60Hz },
    { 20,  64, NEW, SGL, BUF_UNION_SIZE(512, 384, 64), BUF_LEFTOVER_SIZE(512, 384, 64), VGA_512x384_60Hz },
    { 21,  16, NEW, SGL, BUF_UNION_SIZE(512, 384, 16), BUF_LEFTOVER_SIZE(512, 384, 16), VGA_512x384_60Hz },
    { 22,   4, NEW, SGL, BUF_UNION_SIZE(512, 384, 4), BUF_LEFTOVER_SIZE(512, 384, 4), VGA_512x384_60Hz },
    { 23,   2, NEW, SGL, BUF_UNION_SIZE(512, 384, 2), BUF_LEFTOVER_SIZE(512, 384, 2), VGA_512x384_60Hz },
    { 129,  4, NEW, DBL, DBL_BUF_UNION_SIZE(640, 480, 4), DBL_BUF_LEFTOVER_SIZE(640, 480, 4), VGA_640x480_60Hz },
    { 130,  2, NEW, DBL, DBL_BUF_UNION_SIZE(640, 480, 2), DBL_BUF_LEFTOVER_SIZE(640, 480, 2), VGA_640x480_60Hz },
    { 132, 16, NEW, DBL, DBL_BUF_UNION_SIZE(640, 240, 16), DBL_BUF_LEFTOVER_SIZE(640, 240, 16), VGA_640x240_60Hz },
    { 133,  4, NEW, DBL, DBL_BUF_UNION_SIZE(640, 240, 4), DBL_BUF_LEFTOVER_SIZE(640, 240, 4), VGA_640x240_60Hz },
    { 134,  2, NEW, DBL, DBL_BUF_UNION_SIZE(640, 240, 2), DBL_BUF_LEFTOVER_SIZE(640, 240, 2), VGA_640x240_60Hz },
    { 136, 64, NEW, DBL, DBL_BUF_UNION_SIZE(320, 240, 64), DBL_BUF_LEFTOVER_SIZE(320, 240, 64), QVGA_320x240_60Hz },
    { 137, 16, NEW, DBL, DBL_BUF_UNION_SIZE(320, 240, 16), DBL_BUF_LEFTOVER_SIZE(320, 240, 16), QVGA_320x240_60Hz },
    { 138,  4, NEW, DBL, DBL_BUF_UNION_SIZE(320, 240, 4), DBL_BUF_LEFTOVER_SIZE(320, 240, 4), QVGA_320x240_60Hz },
    { 139,  2, NEW, DBL, DBL_BUF_UNION_SIZE(320, 240, 2), DBL_BUF_LEFTOVER_SIZE(320, 240, 2), QVGA_320x240_60Hz },
    { 140, 64, NEW, DBL, DBL_BUF_UNION_SIZE(320, 200, 64), DBL_BUF_LEFTOVER_SIZE(320, 200, 64), VGA_320x200_70Hz },
    { 141, 16, NEW, DBL, DBL_BUF_UNION_SIZE(320, 200, 16), DBL_BUF_LEFTOVER_SIZE(320, 200, 16), VGA_320x200_70Hz },
    { 142,  4, NEW, DBL, DBL_BUF_UNION_SIZE(320, 200, 4), DBL_BUF_LEFTOVER_SIZE(320, 200, 4), VGA_320x200_70Hz },
    { 143,  2, NEW, DBL, DBL_BUF_UNION_SIZE(320, 200, 2), DBL_BUF_LEFTOVER_SIZE(320, 200, 2), VGA_320x200_70Hz },
    { 145,  2, NEW, DBL, DBL_BUF_UNION_SIZE(800, 600, 2), DBL_BUF_LEFTOVER_SIZE(800, 600, 2), SVGA_800x600_60Hz },
    { 146,  2, NEW, DBL, DBL_BUF_UNION_SIZE(1024, 768, 2), DBL_BUF_LEFTOVER_SIZE(1024, 768, 2), SVGA_1024x768_60Hz },
    { 149, 16, NEW, DBL, DBL_BUF_UNION_SIZE(512, 384, 16), DBL_BUF_LEFTOVER_SIZE(512, 384, 16), VGA_512x384_60Hz },
    { 150,  4, NEW, DBL, DBL_BUF_UNION_SIZE(512, 384, 4), DBL_BUF_LEFTOVER_SIZE(512, 384, 4), VGA_512x384_60Hz },
    { 151,  2, NEW, DBL, DBL_BUF_UNION_SIZE(512, 384, 2), DBL_BUF_LEFTOVER_SIZE(512, 384, 2), VGA_512x384_60Hz }
};


void VgaTiming::finishInitialization() {
    if (!m_mul_scan) m_mul_scan = 1;

    m_h_active = m_h_fp_at;
    m_h_fp = m_h_sync_at - m_h_fp_at;
    m_h_sync = m_h_bp_at - m_h_sync_at;
    m_h_bp = m_h_total - m_h_bp_at;

    m_v_active = m_v_fp_at;
    m_v_fp = m_v_sync_at - m_v_fp_at;
    m_v_sync = m_v_bp_at - m_v_sync_at;
    m_v_bp = m_v_total - m_v_bp_at;
}


FramePixels frame_pixels_0;
FramePixels frame_pixels_1;
FramePixels frame_pixels_2;
FramePixels frame_pixels_3;
FramePixels frame_pixels_4;
FramePixels frame_pixels_5;
FramePixels frame_pixels_6;
FramePixels frame_pixels_7;

FramePixels* frame_sections[NUM_SECTIONS] = {
    &frame_pixels_0,
    &frame_pixels_1,
    &frame_pixels_2,
    &frame_pixels_3,
    &frame_pixels_4,
    &frame_pixels_5,
    &frame_pixels_6,
    &frame_pixels_7
};

VgaFrame vgaFrame;


VgaFrame::VgaFrame()
{
}

VgaFrame::~VgaFrame()
{
}

int VgaFrame::getNumModes() {
    return sizeof(vgaSettings)/sizeof(VgaSettings);
}

void VgaFrame::finishInitialization() {
    VGA_320x200_70Hz.finishInitialization();
    VGA_320x200_75Hz.finishInitialization();
    VGA_320x200_75HzRetro.finishInitialization();
    QVGA_320x240_60Hz.finishInitialization();
    SVGA_320x256_60Hz.finishInitialization();
    VGA_400x300_60Hz.finishInitialization();
    VGA_480x300_75Hz.finishInitialization();
    VGA_512x384_60Hz.finishInitialization();
    VGA_512x192_60Hz.finishInitialization();
    SVGA_640x360_60Hz.finishInitialization();
    VGA_640x480_60Hz.finishInitialization();
    VGA_640x240_60Hz.finishInitialization();
    QSVGA_640x512_60Hz.finishInitialization();
    QSVGA_640x256_60Hz.finishInitialization();
    SVGA_800x600_60Hz.finishInitialization();
    SVGA_960x540_60Hz.finishInitialization();
    SVGA_1024x768_60Hz.finishInitialization();
    SVGA_1024x768_70Hz.finishInitialization();
    SVGA_1024x768_75Hz.finishInitialization();
    SVGA_1280x720_60Hz.finishInitialization();
}

void VgaFrame::listModes() {
    int n = getNumModes();
    for (int i = 0; i < n; i++) {
        const VgaSettings& s = vgaSettings[i];
        const VgaTiming& t = s.m_timing;
        debug_log("[%03i] mode %3hu: %15s, %2hu colors, section uses %6i, leaves %6i, buffer uses %6i, leaves %6i\n",
            i, s.m_mode, t.m_name, s.m_colors,
            s.m_size, s.m_remain, s.m_size*NUM_SECTIONS, s.m_remain*NUM_SECTIONS);
    }
}

const VgaSettings& VgaFrame::getSettings(uint8_t mode, uint8_t colors, uint8_t legacy) {
    int n = getNumModes();
    for (int i = 0; i < n; i++) {
        const VgaSettings& s = vgaSettings[i];
        if (s.m_mode == mode && s.m_colors == colors && s.m_legacy == legacy) {
            return s;
        }
    }
    return vgaSettings[0]; // fallback to mode 0 settings
}

const VgaTiming& VgaFrame::getTiming(uint8_t mode, uint8_t colors, uint8_t legacy) {
    const VgaSettings& s = getSettings(mode, colors, legacy);
    return s.m_timing;
}

FramePixels& VgaFrame::getSection(int index) {
    return *frame_sections[index];
}
