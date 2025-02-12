// File: vgaframe.cpp - unions that describe DMA buffers for video
// By: Curtis Whitley

#include "vgaframe.h"
#include <string.h>
#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "driver/periph_ctrl.h"
#include "soc/rtc.h"
#include "driver/gpio.h"
#include "soc/io_mux_reg.h"

#define MAX_TOTAL_LINES     806     // includes active, vfp, vs, and vbp
#define MAX_ACTIVE_PIXELS   1024
#define MAX_ACTIVE_LINES    768
#define MAX_BLANKING_PIXELS 320
#define MAX_TOTAL_PIXELS    (MAX_ACTIVE_PIXELS + MAX_BLANKING_PIXELS)
#define NUM_OUTPUT_LINES    8       // DMA pixels to be sent out; must be a power of 2!

#define GPIO_RED_0    GPIO_NUM_21
#define GPIO_RED_1    GPIO_NUM_22
#define GPIO_GREEN_0  GPIO_NUM_18
#define GPIO_GREEN_1  GPIO_NUM_19
#define GPIO_BLUE_0   GPIO_NUM_4
#define GPIO_BLUE_1   GPIO_NUM_5
#define GPIO_HSYNC    GPIO_NUM_23
#define GPIO_VSYNC    GPIO_NUM_15

#define VS0 0
#define VS1 1
#define HS0 0
#define HS1 1
#define R0  0
#define R1  1
#define R2  2
#define R3  3
#define G0  0
#define G1  1
#define G2  2
#define G3  3
#define B0  0
#define B1  1
#define B2  2
#define B3  3

#define VGA_RED_BIT    0
#define VGA_GREEN_BIT  2
#define VGA_BLUE_BIT   4
#define VGA_HSYNC_BIT  6
#define VGA_VSYNC_BIT  7

#define MASK_RGB(r,g,b) (((r)<<VGA_RED_BIT)|((g)<<VGA_GREEN_BIT)|((b)<<VGA_BLUE_BIT))

// This formula handles arranging the pixel bytes in the correct DMA order.
// 0x12345678, normally stored as 78, 56, 34, 12, is sent as 34 12 78 56.
#define FIX_INDEX(idx)      ((idx)^2)

struct APLLParams {
  uint8_t sdm0;
  uint8_t sdm1;
  uint8_t sdm2;
  uint8_t o_div;
};

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

template <typename T>
const T & tmax(const T & a, const T & b)
{
  return (a < b) ? b : a;
}

template <typename T>
const T & tmin(const T & a, const T & b)
{
  return !(b < a) ? a : b;
}

// definitions:
//   apll_clk = XTAL * (4 + sdm2 + sdm1 / 256 + sdm0 / 65536) / (2 * o_div + 4)
//     dividend = XTAL * (4 + sdm2 + sdm1 / 256 + sdm0 / 65536)
//     divisor  = (2 * o_div + 4)
//   freq = apll_clk / (2 + b / a)        => assumes  tx_bck_div_num = 1 and clkm_div_num = 2
// Other values range:
//   sdm0  0..255
//   sdm1  0..255
//   sdm2  0..63
//   o_div 0..31
// Assume xtal = FABGLIB_XTAL (40MHz)
// The dividend should be in the range of 350 - 500 MHz (350000000-500000000), so these are the
// actual parameters ranges (so the minimum apll_clk is 5303030 Hz and maximum is 125000000Hz):
//  MIN 87500000Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 0
//  MAX 125000000Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 0
//
//  MIN 58333333Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 1
//  MAX 83333333Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 1
//
//  MIN 43750000Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 2
//  MAX 62500000Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 2
//
//  MIN 35000000Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 3
//  MAX 50000000Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 3
//
//  MIN 29166666Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 4
//  MAX 41666666Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 4
//
//  MIN 25000000Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 5
//  MAX 35714285Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 5
//
//  MIN 21875000Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 6
//  MAX 31250000Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 6
//
//  MIN 19444444Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 7
//  MAX 27777777Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 7
//
//  MIN 17500000Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 8
//  MAX 25000000Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 8
//
//  MIN 15909090Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 9
//  MAX 22727272Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 9
//
//  MIN 14583333Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 10
//  MAX 20833333Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 10
//
//  MIN 13461538Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 11
//  MAX 19230769Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 11
//
//  MIN 12500000Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 12
//  MAX 17857142Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 12
//
//  MIN 11666666Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 13
//  MAX 16666666Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 13
//
//  MIN 10937500Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 14
//  MAX 15625000Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 14
//
//  MIN 10294117Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 15
//  MAX 14705882Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 15
//
//  MIN 9722222Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 16
//  MAX 13888888Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 16
//
//  MIN 9210526Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 17
//  MAX 13157894Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 17
//
//  MIN 8750000Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 18
//  MAX 12500000Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 18
//
//  MIN 8333333Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 19
//  MAX 11904761Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 19
//
//  MIN 7954545Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 20
//  MAX 11363636Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 20
//
//  MIN 7608695Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 21
//  MAX 10869565Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 21
//
//  MIN 7291666Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 22
//  MAX 10416666Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 22
//
//  MIN 7000000Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 23
//  MAX 10000000Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 23
//
//  MIN 6730769Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 24
//  MAX 9615384Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 24
//
//  MIN 6481481Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 25
//  MAX 9259259Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 25
//
//  MIN 6250000Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 26
//  MAX 8928571Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 26
//
//  MIN 6034482Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 27
//  MAX 8620689Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 27
//
//  MIN 5833333Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 28
//  MAX 8333333Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 28
//
//  MIN 5645161Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 29
//  MAX 8064516Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 29
//
//  MIN 5468750Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 30
//  MAX 7812500Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 30
//
//  MIN 5303030Hz - sdm0 = 0 sdm1 = 192 sdm2 = 4 o_div = 31
//  MAX 7575757Hz - sdm0 = 0 sdm1 = 128 sdm2 = 8 o_div = 31
void APLLCalcParams(double freq, APLLParams * params, uint8_t * a, uint8_t * b, double * out_freq, double * error)
{
  double FXTAL = 40000000.0;

  *error = 999999999;

  double apll_freq = freq * 2;

  for (int o_div = 0; o_div <= 31; ++o_div) {

    int idivisor = (2 * o_div + 4);

    for (int sdm2 = 4; sdm2 <= 8; ++sdm2) {

      // from tables above
      int minSDM1 = (sdm2 == 4 ? 192 : 0);
      int maxSDM1 = (sdm2 == 8 ? 128 : 255);
      // apll_freq = XTAL * (4 + sdm2 + sdm1 / 256) / divisor   ->   sdm1 = (apll_freq * divisor - XTAL * 4 - XTAL * sdm2) * 256 / XTAL
      int startSDM1 = ((apll_freq * idivisor - FXTAL * 4.0 - FXTAL * sdm2) * 256.0 / FXTAL);
#if FABGLIB_USE_APLL_AB_COEF
      for (int isdm1 = tmax(minSDM1, startSDM1); isdm1 <= maxSDM1; ++isdm1) {
#else
      int isdm1 = startSDM1; {
#endif

        int sdm1 = isdm1;
        sdm1 = tmax(minSDM1, sdm1);
        sdm1 = tmin(maxSDM1, sdm1);

        // apll_freq = XTAL * (4 + sdm2 + sdm1 / 256 + sdm0 / 65536) / divisor   ->   sdm0 = (apll_freq * divisor - XTAL * 4 - XTAL * sdm2 - XTAL * sdm1 / 256) * 65536 / XTAL
        int sdm0 = ((apll_freq * idivisor - FXTAL * 4.0 - FXTAL * sdm2 - FXTAL * sdm1 / 256.0) * 65536.0 / FXTAL);
        // from tables above
        sdm0 = (sdm2 == 8 && sdm1 == 128 ? 0 : tmin(255, sdm0));
        sdm0 = tmax(0, sdm0);

        // dividend inside 350-500Mhz?
        double dividend = FXTAL * (4.0 + sdm2 + sdm1 / 256.0 + sdm0 / 65536.0);
        if (dividend >= 350000000 && dividend <= 500000000) {
          // adjust output frequency using "b/a"
          double oapll_freq = dividend / idivisor;

          // Calculates "b/a", assuming tx_bck_div_num = 1 and clkm_div_num = 2:
          //   freq = apll_clk / (2 + clkm_div_b / clkm_div_a)
          //     abr = clkm_div_b / clkm_div_a
          //     freq = apll_clk / (2 + abr)    =>    abr = apll_clk / freq - 2
          uint8_t oa = 1, ob = 0;
#if FABGLIB_USE_APLL_AB_COEF
          double abr = oapll_freq / freq - 2.0;
          if (abr > 0 && abr < 1) {
            int num, den;
            floatToFraction(abr, 63, &num, &den);
            ob = tclamp(num, 0, 63);
            oa = tclamp(den, 0, 63);
          }
#endif

          // is this the best?
          double ofreq = oapll_freq / (2.0 + (double)ob / oa);
          double err = freq - ofreq;
          if (abs(err) < abs(*error)) {
            *params = (APLLParams){(uint8_t)sdm0, (uint8_t)sdm1, (uint8_t)sdm2, (uint8_t)o_div};
            *a = oa;
            *b = ob;
            *out_freq = ofreq;
            *error = err;
            if (err == 0.0)
              return;
          }
        }
      }

    }
  }
}

void configureGPIO(gpio_num_t gpio, gpio_mode_t mode)
{
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio], PIN_FUNC_GPIO);
  gpio_set_direction(gpio, mode);
}

// if bit is -1 = clock signal
// gpio = GPIO_UNUSED means not set
void setupGPIO(gpio_num_t gpio, int bit, gpio_mode_t mode)
{
  if (bit == -1) {
    // I2S1 clock out to CLK_OUT1 (fixed to GPIO0)
    WRITE_PERI_REG(PIN_CTRL, 0xF);
    PIN_FUNC_SELECT(GPIO_PIN_REG_0, FUNC_GPIO0_CLK_OUT1);
  } else {
    configureGPIO(gpio, mode);
    gpio_matrix_out(gpio, I2S1O_DATA_OUT0_IDX + bit, false, false);
  }
}

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

    m_h_sync_off = m_h_sync_on ^ HSBIT;
    m_v_sync_off = m_v_sync_on ^ VSBIT;
    m_hv_sync_on = m_h_sync_on | m_v_sync_on;
    m_hv_sync_off = m_h_sync_off | m_v_sync_off;

    m_hv_sync_4_on = 
        ((uint32_t)m_h_sync_on) |
        (((uint32_t)m_h_sync_on) << 8) |
        (((uint32_t)m_h_sync_on) << 16) |
        (((uint32_t)m_h_sync_on) << 24);

    m_hv_sync_4_off = 
        ((uint32_t)m_h_sync_off) |
        (((uint32_t)m_h_sync_off) << 8) |
        (((uint32_t)m_h_sync_off) << 16) |
        (((uint32_t)m_h_sync_off) << 24);
}


__attribute__((section(".frame_pixels_0"))) FramePixels frame_pixels_0;
__attribute__((section(".frame_pixels_1"))) FramePixels frame_pixels_1;
__attribute__((section(".frame_pixels_2"))) FramePixels frame_pixels_2;
__attribute__((section(".frame_pixels_3"))) FramePixels frame_pixels_3;
__attribute__((section(".frame_pixels_4"))) FramePixels frame_pixels_4;
__attribute__((section(".frame_pixels_5"))) FramePixels frame_pixels_5;
__attribute__((section(".frame_pixels_6"))) FramePixels frame_pixels_6;
__attribute__((section(".frame_pixels_7"))) FramePixels frame_pixels_7;

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

DMA_ATTR lldesc_t dmaDescr[MAX_TOTAL_LINES + MAX_ACTIVE_LINES];

DMA_ATTR union {
    uint32_t w[MAX_TOTAL_PIXELS/4];
    uint8_t  b[MAX_TOTAL_PIXELS];
} blankLine;

DMA_ATTR union {
    uint32_t w[MAX_TOTAL_PIXELS/4];
    uint8_t  b[MAX_TOTAL_PIXELS];
} blankLineVS;

DMA_ATTR union {
    uint32_t w[MAX_BLANKING_PIXELS/4];
    uint8_t  b[MAX_BLANKING_PIXELS];
} activePad;

DMA_ATTR union {
    uint32_t w[MAX_ACTIVE_PIXELS/4];
    uint32_t b[MAX_ACTIVE_PIXELS];
} outputLines[NUM_OUTPUT_LINES];

const VgaSettings* curSettings;
const VgaTiming* curTiming;

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
}

void VgaFrame::listModes() {
    int n = getNumModes();
    for (int i = 0; i < n; i++) {
        const VgaSettings& s = vgaSettings[i];
        const VgaTiming& t = s.m_timing;
        debug_log("[%03i] mode %3hu: %15s, %2hu colors,"
            " section uses %6i, leaves %6i, buffer uses %6i, leaves %6i,"
            " H%c %hu/%hu/%hu/%hu, V%c %hu/%hu/%hu/%hu\n",
            i, s.m_mode, t.m_name, s.m_colors,
            s.m_size, s.m_remain, s.m_size*NUM_SECTIONS, s.m_remain*NUM_SECTIONS,
            (t.m_h_sync_on ? '+' : '-'),
            t.m_h_active, t.m_h_fp, t.m_h_sync, t.m_h_bp,
            (t.m_v_sync_on ? '+' : '-'),
            t.m_v_active, t.m_v_fp, t.m_v_sync, t.m_v_bp);
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
    const VgaSettings& s = getSettings(mode, colors, legacy);        const VgaTiming& t = s.m_timing;

    return s.m_timing;
}

FramePixels& VgaFrame::getSection(int index) {
    return *frame_sections[index];
}

void VgaFrame::setMode(uint8_t mode, uint8_t colors, uint8_t legacy) {
    stopVideo();
    curSettings = &getSettings(mode, colors, legacy);
    curTiming = &curSettings->m_timing;
    auto t = curTiming;

    // Initialize the blanking area buffers

    // Entire blank line with VS off
    // [active pixels][hfp][hs][hbp]
    memset(blankLine.b, t->m_hv_sync_off, t->m_h_active);
    memset(&blankLine.b[t->m_h_fp_at], t->m_hv_sync_off, t->m_h_fp);
    memset(&blankLine.b[t->m_h_sync_at], t->m_h_sync_on, t->m_h_sync);
    memset(&blankLine.b[t->m_h_bp_at], t->m_hv_sync_off, t->m_h_bp);

    // Entire blank line with VS on
    // [active pixels][hfp][hs][hbp]
    memset(blankLineVS.b, t->m_v_sync_on, t->m_h_active);
    memset(&blankLineVS.b[t->m_h_fp_at], t->m_v_sync_on, t->m_h_fp);
    memset(&blankLineVS.b[t->m_h_sync_at], t->m_hv_sync_on, t->m_h_sync);
    memset(&blankLineVS.b[t->m_h_bp_at], t->m_v_sync_on, t->m_h_bp);

    // Active pad when VS is off
    // [hfp][hs][hbp]
    memset(activePad.b, t->m_hv_sync_off, t->m_h_fp);
    memset(&activePad.b[t->m_h_fp], t->m_h_sync_on, t->m_h_sync);
    memset(&activePad.b[t->m_h_fp + t->m_h_sync], t->m_hv_sync_off, t->m_h_bp);

    // Output lines
    for (uint16_t line = 0; line < NUM_OUTPUT_LINES; line++) {
        memset(outputLines[line].b, t->m_hv_sync_off | 0x04, t->m_h_active);
    }

    // Drawing frame area
    clearScreen();

    // Link the DMA descriptors
    auto numLinesInSection = curTiming->m_h_active / NUM_SECTIONS;
    auto curDescr = dmaDescr;
    auto nextDescr = curDescr + 1;
    uint16_t scanLine = 0;
    uint16_t outputLine = 0;
    uint16_t lineIndex = 0;

    // The vertical active area
    while (scanLine < curTiming->m_v_active) {
        // Descriptor pointing to active (visible) data
        curDescr->qe.stqe_next = nextDescr;
        curDescr->sosf = 0;
        curDescr->offset = 0;
        curDescr->eof = 0;//1;
        curDescr->owner = 1;
        curDescr->size = curTiming->m_h_active;
        curDescr->length = curDescr->size;
        curDescr->buf = (uint8_t volatile *) outputLines[lineIndex].b;
        curDescr = nextDescr++;

        // Descriptor pointing to blanking (invisible) data, the "active pad"
        curDescr->qe.stqe_next = nextDescr;
        curDescr->sosf = 0;
        curDescr->offset = 0;
        curDescr->eof = 0;
        curDescr->owner = 1;
        curDescr->size = curTiming->m_h_fp + curTiming->m_h_sync + curTiming->m_h_bp;
        curDescr->length = curDescr->size;
        curDescr->buf = (uint8_t volatile *) activePad.b;
        curDescr = nextDescr++;

        // Move to the next line
        scanLine++;
        if (++lineIndex >= NUM_OUTPUT_LINES) {
            lineIndex = 0;
        }
    }

    // The vertical front porch
    while (scanLine < curTiming->m_v_sync_at) {
        // Descriptor pointing to entire blank line without VS
        curDescr->qe.stqe_next = nextDescr;
        curDescr->sosf = 0;
        curDescr->offset = 0;
        curDescr->eof = 0;
        curDescr->owner = 1;
        curDescr->size = curTiming->m_h_total;
        curDescr->length = curDescr->size;
        curDescr->buf = (uint8_t volatile *) blankLine.b;
        curDescr = nextDescr++;

        scanLine++; // Move to the next line
    }

    // The vertical sync
    while (scanLine < curTiming->m_v_bp_at) {
        // Descriptor pointing to entire blank line with VS
        curDescr->qe.stqe_next = nextDescr;
        curDescr->sosf = 0;
        curDescr->offset = 0;
        curDescr->eof = 0;
        curDescr->owner = 1;
        curDescr->size = curTiming->m_h_total;
        curDescr->length = curDescr->size;
        curDescr->buf = (uint8_t volatile *) blankLineVS.b;
        curDescr = nextDescr++;

        scanLine++; // Move to the next line
    }

    // The vertical back porch
    while (scanLine < curTiming->m_v_total) {
        // Descriptor pointing to entire blank line without VS
        curDescr->qe.stqe_next = nextDescr;
        curDescr->sosf = 0;
        curDescr->offset = 0;
        curDescr->eof = 0;//(((scanLine + 1) == curTiming->m_v_total - NUM_OUTPUT_LINES/2) ? 1 : 0);
        curDescr->owner = 1;
        curDescr->size = curTiming->m_h_active;
        curDescr->length = curDescr->size;
        curDescr->buf = (uint8_t volatile *) blankLine.b;
        curDescr = nextDescr++;

        scanLine++; // Move to the next line
    }

    (--curDescr)->qe.stqe_next = dmaDescr; // loop the descriptors

    startVideo();
}

void VgaFrame::stopVideo() {
    I2S1.conf.tx_reset = 1;
    I2S1.conf.tx_fifo_reset = 1;
    I2S1.clkm_conf.clk_en = 0;

    if (curSettings) {
        curSettings = nullptr;
        curTiming = nullptr;
    }
}

void VgaFrame::startVideo() {

    // GPIO configuration for color bits
    setupGPIO(GPIO_RED_0,   VGA_RED_BIT,       GPIO_MODE_OUTPUT);
    setupGPIO(GPIO_RED_1,   VGA_RED_BIT + 1,   GPIO_MODE_OUTPUT);
    setupGPIO(GPIO_GREEN_0, VGA_GREEN_BIT,     GPIO_MODE_OUTPUT);
    setupGPIO(GPIO_GREEN_1, VGA_GREEN_BIT + 1, GPIO_MODE_OUTPUT);
    setupGPIO(GPIO_BLUE_0,  VGA_BLUE_BIT,      GPIO_MODE_OUTPUT);
    setupGPIO(GPIO_BLUE_1,  VGA_BLUE_BIT + 1,  GPIO_MODE_OUTPUT);

    // GPIO configuration for VSync and HSync
    setupGPIO(GPIO_HSYNC, VGA_HSYNC_BIT, GPIO_MODE_OUTPUT);
    setupGPIO(GPIO_VSYNC, VGA_VSYNC_BIT, GPIO_MODE_OUTPUT);

    // Start the DMA

    // Power on device
    periph_module_enable(PERIPH_I2S1_MODULE);

    // Initialize I2S device
    I2S1.conf.tx_reset = 1;
    I2S1.conf.tx_reset = 0;

    // Reset DMA
    I2S1.lc_conf.out_rst = 1;
    I2S1.lc_conf.out_rst = 0;

    // Reset FIFO
    I2S1.conf.tx_fifo_reset = 1;
    I2S1.conf.tx_fifo_reset = 0;

    // Stop DMA clock
    I2S1.clkm_conf.clk_en = 0;

    // LCD mode
    I2S1.conf2.val            = 0;
    I2S1.conf2.lcd_en         = 1;
    I2S1.conf2.lcd_tx_wrx2_en = (curTiming->m_mul_scan >= 2 ? 1 : 0);
    I2S1.conf2.lcd_tx_sdx2_en = 0;

    I2S1.sample_rate_conf.val         = 0;
    I2S1.sample_rate_conf.tx_bits_mod = 8;

    // Start DMA clock
    APLLParams prms = {0, 0, 0, 0};
    double error, out_freq;
    uint8_t a = 1, b = 0;
    APLLCalcParams(((double)curTiming->m_frequency)/1000000.0, &prms, &a, &b, &out_freq, &error);

    I2S1.clkm_conf.val          = 0;
    I2S1.clkm_conf.clkm_div_b   = b;
    I2S1.clkm_conf.clkm_div_a   = a;
    I2S1.clkm_conf.clkm_div_num = 2;  // not less than 2

    I2S1.sample_rate_conf.tx_bck_div_num = 1; // this makes I2S1O_BCK = I2S1_CLK

    rtc_clk_apll_enable(true, prms.sdm0, prms.sdm1, prms.sdm2, prms.o_div);

    I2S1.clkm_conf.clka_en = 1;

    // Setup FIFO
    I2S1.fifo_conf.val                  = 0;
    I2S1.fifo_conf.tx_fifo_mod_force_en = 1;
    I2S1.fifo_conf.tx_fifo_mod          = 1;
    I2S1.fifo_conf.tx_fifo_mod          = 1;
    I2S1.fifo_conf.tx_data_num          = 32;
    I2S1.fifo_conf.dscr_en              = 1;

    I2S1.conf1.val           = 0;
    I2S1.conf1.tx_stop_en    = 0;
    I2S1.conf1.tx_pcm_bypass = 1;

    I2S1.conf_chan.val         = 0;
    I2S1.conf_chan.tx_chan_mod = 1;

    //I2S1.conf.tx_right_first = 0;
    I2S1.conf.tx_right_first = 1;

    I2S1.timing.val = 0;

    // Reset AHB interface of DMA
    I2S1.lc_conf.ahbm_rst      = 1;
    I2S1.lc_conf.ahbm_fifo_rst = 1;
    I2S1.lc_conf.ahbm_rst      = 0;
    I2S1.lc_conf.ahbm_fifo_rst = 0;

    // Prepare to start DMA
    I2S1.lc_conf.val = I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN;
    I2S1.out_link.addr = (uint32_t)(void*) dmaDescr;
    I2S1.int_clr.val = 0xFFFFFFFF;

    // Start DMA
    I2S1.out_link.start = 1;
    I2S1.conf.tx_start  = 1;
}

void VgaFrame::clearScreen() {
    auto sect_size =
        ((uint32_t)curTiming->m_h_active * (uint32_t)curTiming->m_v_active) / NUM_SECTIONS;
    for (uint16_t s = 0; s < NUM_SECTIONS; s++) {
        memset(&getSection(s), 0, sect_size);
    }
}