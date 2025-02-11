// File: vgaframe.cpp - unions that describe DMA buffers for video
// By: Curtis Whitley

#include "vgaframe.h"

static VgaTiming VGA_320x200_70Hz {"320x200@70Hz", 12587500, 320, 328, 376, 400, 200, 206, 207, 224, HSNEG, VSNEG, DBL};
static VgaTiming VGA_320x200_75Hz {"320x200@75Hz", 12.93, 320, 352, 376, 408, 200, 208, 211, 229, HSNEG, VSNEG, DBL};
static VgaTiming VGA_320x200_75HzRetro {"320x200@75Hz", 12.93, 320, 352, 376, 408, 200, 208, 211, 229, HSNEG, VSNEG, DBL, MSB};
static VgaTiming QVGA_320x240_60Hz {"320x240@60Hz", 12.6, 320, 328, 376, 400, 240, 245, 246, 262, HSNEG, VSNEG, DBL};
static VgaTiming SVGA_320x256_60Hz {"320x256@60Hz", 27, 320, 332, 360, 424, 256, 257, 258, 267, HSNEG, VSNEG, QUAD};
static VgaTiming VGA_400x300_60Hz {"400x300@60Hz", 20, 400, 420, 484, 528, 300, 300, 302, 314, HSNEG, VSNEG, DBL};
static VgaTiming VGA_480x300_75Hz {"480x300@75Hz", 31.22, 480, 504, 584, 624, 300, 319, 322, 333, HSNEG, VSNEG, DBL};
static VgaTiming VGA_512x384_60Hz {"512x384@60Hz", 32.5, 512, 524, 592, 672, 384, 385, 388, 403, HSNEG, VSNEG, DBL};
static VgaTiming VGA_512x192_60Hz {"512x192@60Hz", 32.5, 512, 524, 592, 672, 192, 193, 194, 201, HSNEG, VSNEG, QUAD};
static VgaTiming SVGA_640x360_60Hz {"640x360@60Hz", 37.24, 640, 734, 802, 832, 360, 361, 362, 373, HSPOS, VSPOS, DBL};
static VgaTiming VGA_640x480_60Hz {"640x480@60Hz", 25.175, 640, 656, 752, 800, 480, 490, 492, 525, HSNEG, VSNEG};
static VgaTiming VGA_640x240_60Hz {"640x240@60Hz", 25.175, 640, 656, 752, 800, 240, 245, 246, 262, HSNEG, VSNEG, DBL};
static VgaTiming QSVGA_640x512_60Hz {"640x512@60Hz", 54, 640, 664, 720, 844, 512, 513, 515, 533, HSNEG, VSNEG, DBL};
static VgaTiming QSVGA_640x256_60Hz {"640x256@60Hz", 54, 640, 664, 720, 844, 256, 257, 258, 267, HSNEG, VSNEG, QUAD};
static VgaTiming SVGA_800x600_60Hz {"800x600@60Hz", 40, 800, 840, 968, 1056, 600, 601, 605, 628, HSNEG, VSNEG};
static VgaTiming SVGA_960x540_60Hz {"960x540@60Hz", 37.26, 960, 976, 1008, 1104, 540, 542, 548, 563, HSPOS, VSPOS};
static VgaTiming SVGA_1024x768_60Hz {"1024x768@60Hz", 65, 1024, 1048, 1184, 1344, 768, 771, 777, 806, HSNEG, VSNEG};
static VgaTiming SVGA_1024x768_70Hz {"1024x768@70Hz", 75, 1024, 1048, 1184, 1328, 768, 771, 777, 806, HSNEG, VSNEG};
static VgaTiming SVGA_1024x768_75Hz {"1024x768@75Hz", 78.80, 1024, 1040, 1136, 1312, 768, 769, 772, 800, HSPOS, VSPOS};
static VgaTiming SVGA_1280x720_60Hz {"1280x720@60Hz", 74.48, 1280, 1468, 1604, 1664, 720, 721, 724, 746, HSPOS, VSPOS};

static const VgaSettings[] vgaSettings = {
    {  0,   2, OLD, SGL, SVGA_1024x768_60Hz },
    {  0,  16, NEW, SGL, VGA_640x480_60Hz },
    {  1,  16, OLD, SGL, VGA_512x384_60Hz },
    {  1,   4, NEW, SGL, VGA_640x480_60Hz },
    {  2,  64, OLD, SGL, VGA_320x200_75Hz },
    {  2,   2, NEW, SGL, VGA_640x480_60Hz },
    {  3,  16, OLD, SGL, VGA_640x480_60Hz },
    {  3,  64, NEW, SGL, VGA_640x240_60Hz },
    {  4,  16, NEW, SGL, VGA_640x240_60Hz },
    {  5,   4, NEW, SGL, VGA_640x240_60Hz },
    {  6,   2, NEW, SGL, VGA_640x240_60Hz },
    {  7,  16, NEW, SGL, VGA_640x480_60Hz },
    {  8,  64, NEW, SGL, QVGA_320x240_60Hz },
    {  9,  16, NEW, SGL, QVGA_320x240_60Hz },
    { 10,   4, NEW, SGL, QVGA_320x240_60Hz },
    { 11,   2, NEW, SGL, QVGA_320x240_60Hz },
    { 12,  64, NEW, SGL, VGA_320x200_70Hz },
    { 13,  16, NEW, SGL, VGA_320x200_70Hz },
    { 14,   4, NEW, SGL, VGA_320x200_70Hz },
    { 15,   2, NEW, SGL, VGA_320x200_70Hz },
    { 16,   4, NEW, SGL, SVGA_800x600_60Hz },
    { 17,   2, NEW, SGL, SVGA_800x600_60Hz },
    { 18,   2, NEW, SGL, SVGA_1024x768_60Hz },
    { 19,   4, NEW, SGL, SVGA_1024x768_60Hz },
    { 20,  64, NEW, SGL, VGA_512x384_60Hz },
    { 21,  16, NEW, SGL, VGA_512x384_60Hz },
    { 22,   4, NEW, SGL, VGA_512x384_60Hz },
    { 23,   2, NEW, SGL, VGA_512x384_60Hz },
    { 129,  4, NEW, DBL, VGA_640x480_60Hz },
    { 130,  2, NEW, DBL, VGA_640x480_60Hz },
    { 132, 16, NEW, DBL, VGA_640x240_60Hz },
    { 133,  4, NEW, DBL, VGA_640x240_60Hz },
    { 134,  2, NEW, DBL, VGA_640x240_60Hz },
    { 136, 64, NEW, DBL, QVGA_320x240_60Hz },
    { 137, 16, NEW, DBL, QVGA_320x240_60Hz },
    { 138,  4, NEW, DBL, QVGA_320x240_60Hz },
    { 139,  2, NEW, DBL, QVGA_320x240_60Hz },
    { 140, 64, NEW, DBL, VGA_320x200_70Hz },
    { 141, 16, NEW, DBL, VGA_320x200_70Hz },
    { 142,  4, NEW, DBL, VGA_320x200_70Hz },
    { 143,  2, NEW, DBL, VGA_320x200_70Hz },
    { 145,  2, NEW, DBL, SVGA_800x600_60Hz },
    { 146,  2, NEW, DBL, SVGA_1024x768_60Hz },
    { 149, 16, NEW, DBL, VGA_512x384_60Hz },
    { 150,  4, NEW, DBL, VGA_512x384_60Hz },
    { 151,  2, NEW, DBL, VGA_512x384_60Hz)
};


VgaTiming::VgaTiming() {
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

DMA_ATTR VgaFrame VgaFrame::vgaFrame;

VgaFrame::VgaFrame()
{
}

VgaFrame::~VgaFrame()
{
}
