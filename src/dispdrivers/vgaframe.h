// File: VgaFrame.h - unions that describe DMA buffers for video
// By: Curtis Whitley

#pragma once

#include "rom/lldesc.h"
#include "esp_attr.h"
#include <stdint.h>

#define BUF_MODE_NAME(w, h, c)          Mode##w##x##h##x##c
#define BUF_UNION_TAG(w, h, c)          tag_PixBuf##w##x##h##x##c
#define BUF_UNION_NAME(w, h, c)         PixBuf##w##x##h##x##c
#define BUF_MEMBER_NAME(w, h, c)        m_##w##x##h##x##c
#define STRINGIZE(x)                    #x
#define BUF_MEMBER_NAME_STR(w, h, c)    STRINGIZE(BUF_MEMBER_NAME(w, h, c))
#define BUF_BITS_DIV(c)                 (c==2?8:(c==4?4:(c==16?2:1)))

typedef enum {
    BUF_MODE_NAME(320, 200, 2),
    BUF_MODE_NAME(320, 200, 4),
    BUF_MODE_NAME(320, 200, 16),
    BUF_MODE_NAME(320, 200, 64),
    BUF_MODE_NAME(320, 240, 2),
    BUF_MODE_NAME(320, 240, 4),
    BUF_MODE_NAME(320, 240, 16),
    BUF_MODE_NAME(320, 240, 64),
    BUF_MODE_NAME(512, 384, 2),
    BUF_MODE_NAME(512, 384, 4),
    BUF_MODE_NAME(512, 384, 16),
    BUF_MODE_NAME(512, 384, 64),
    BUF_MODE_NAME(640, 240, 2),
    BUF_MODE_NAME(640, 240, 4),
    BUF_MODE_NAME(640, 240, 16),
    BUF_MODE_NAME(640, 240, 64),
    BUF_MODE_NAME(640, 480, 2),
    BUF_MODE_NAME(640, 480, 4),
    BUF_MODE_NAME(640, 480, 16),
    BUF_MODE_NAME(640, 480, 64),
    BUF_MODE_NAME(800, 600, 2),
    BUF_MODE_NAME(800, 600, 4),
    BUF_MODE_NAME(1024, 768, 2),
    BUF_MODE_NAME(1024, 768, 4)
} FrameModeEnum;

#define PIXEL_BUFFER(w, h, c) \
    typedef union BUF_UNION_TAG(w, h, c) { \
        uint8_t   m_8[w*h/BUF_BITS_DIV(c)]; \
        uint16_t  m_16[w*h/BUF_BITS_DIV(c)/2]; \
        uint32_t  m_32[w*h/BUF_BITS_DIV(c)/4]; \
    } BUF_UNION_NAME(w, h, c);

PIXEL_BUFFER(320, 200, 2)
PIXEL_BUFFER(320, 200, 4)
PIXEL_BUFFER(320, 200, 16)
PIXEL_BUFFER(320, 200, 64)
PIXEL_BUFFER(320, 240, 2)
PIXEL_BUFFER(320, 240, 4)
PIXEL_BUFFER(320, 240, 16)
PIXEL_BUFFER(320, 240, 64)
PIXEL_BUFFER(512, 384, 2)
PIXEL_BUFFER(512, 384, 4)
PIXEL_BUFFER(512, 384, 16)
PIXEL_BUFFER(512, 384, 64)
PIXEL_BUFFER(640, 240, 2)
PIXEL_BUFFER(640, 240, 4)
PIXEL_BUFFER(640, 240, 16)
PIXEL_BUFFER(640, 240, 64)
PIXEL_BUFFER(640, 480, 2)
PIXEL_BUFFER(640, 480, 4)
PIXEL_BUFFER(640, 480, 16)
PIXEL_BUFFER(640, 480, 64)
PIXEL_BUFFER(800, 600, 2)
PIXEL_BUFFER(800, 600, 4)
PIXEL_BUFFER(1024, 768, 2)
PIXEL_BUFFER(1024, 768, 4)

#define BUF_UNION_SIZE(w, h, c)     sizeof(BUF_UNION_NAME(w, h, c))
#define BUF_LEFTOVER_SIZE(w, h, c)  (sizeof(FramePixels) - BUF_UNION_SIZE(w, h, c))

// The FramePixels union is used to allocate pixel data buffers statically.
//
typedef union {
    BUF_UNION_NAME(320, 200, 2)    BUF_MEMBER_NAME(320, 200, 2);
    BUF_UNION_NAME(320, 200, 4)    BUF_MEMBER_NAME(320, 200, 4);
    BUF_UNION_NAME(320, 200, 16)   BUF_MEMBER_NAME(320, 200, 16);
    BUF_UNION_NAME(320, 200, 64)   BUF_MEMBER_NAME(320, 200, 64);
    BUF_UNION_NAME(320, 240, 2)    BUF_MEMBER_NAME(320, 240, 2);
    BUF_UNION_NAME(320, 240, 4)    BUF_MEMBER_NAME(320, 240, 4);
    BUF_UNION_NAME(320, 240, 16)   BUF_MEMBER_NAME(320, 240, 16);
    BUF_UNION_NAME(320, 240, 64)   BUF_MEMBER_NAME(320, 240, 64);
    BUF_UNION_NAME(512, 384, 2)    BUF_MEMBER_NAME(512, 384, 2);
    BUF_UNION_NAME(512, 384, 4)    BUF_MEMBER_NAME(512, 384, 4);
    BUF_UNION_NAME(512, 384, 16)   BUF_MEMBER_NAME(512, 384, 16);
    BUF_UNION_NAME(512, 384, 64)   BUF_MEMBER_NAME(512, 384, 64);
    BUF_UNION_NAME(640, 240, 2)    BUF_MEMBER_NAME(640, 240, 2);
    BUF_UNION_NAME(640, 240, 4)    BUF_MEMBER_NAME(640, 240, 4);
    BUF_UNION_NAME(640, 240, 16)   BUF_MEMBER_NAME(640, 240, 16);
    BUF_UNION_NAME(640, 240, 64)   BUF_MEMBER_NAME(640, 240, 64);
    BUF_UNION_NAME(640, 480, 2)    BUF_MEMBER_NAME(640, 480, 2);
    BUF_UNION_NAME(640, 480, 4)    BUF_MEMBER_NAME(640, 480, 4);
    BUF_UNION_NAME(640, 480, 16)   BUF_MEMBER_NAME(640, 480, 16);
    BUF_UNION_NAME(640, 480, 64)   BUF_MEMBER_NAME(640, 480, 64);
    BUF_UNION_NAME(800, 600, 2)    BUF_MEMBER_NAME(800, 600, 2);
    BUF_UNION_NAME(800, 600, 4)    BUF_MEMBER_NAME(800, 600, 4);
    BUF_UNION_NAME(1024, 768, 2)   BUF_MEMBER_NAME(1024, 768, 2);
    BUF_UNION_NAME(1024, 768, 4)   BUF_MEMBER_NAME(1024, 768, 4);
} FramePixels;

typedef struct {
    const char*     m_name;         // descriptive name of the mode
    uint32_t        m_frequency;    // clock frequency in Hz
    uint16_t        m_h_fp_at;      // pixel index of horizontal front porch
    uint16_t        m_h_sync_at;    // pixel index of horizontal sync
    uint16_t        m_h_bp_at;      // pixel index of horizontal back porch
    uint16_t        m_h_total;      // total horizontal pixels per scan line
    uint16_t        m_v_fp_at;      // line index of vertical front porch
    uint16_t        m_v_sync_at;    // line index of vertical sync
    uint16_t        m_v_bp_at;      // line index of vertical back porch
    uint16_t        m_v_total;      // total vertical lines per frame
    uint8_t         m_h_sync_bit;   // whether HS is positive (vs negative)
    uint8_t         m_v_sync_bit;   // whether VS is positive (vs negative)
    uint8_t         m_mul_scan;     // multiplier for scan lines
    uint8_t         m_mul_blank;    // whether extra lines are blank
    uint16_t        m_h_active;     // horizontal active pixels
    uint16_t        m_h_fp;         // horizontal front porch pixels
    uint16_t        m_h_sync;       // horizontal sync pixels
    uint16_t        m_h_bp;         // horizontal back porch pixels
    uint16_t        m_v_active;     // vertical active pixels
    uint16_t        m_v_fp;         // vertical front porch pixels
    uint16_t        m_v_sync;       // vertical sync pixels
    uint16_t        m_v_bp;         // vertical back porch pixels
} VgaTiming;

#define HSNEG       0   // HS negative
#define HSPOS       1   // HS positive
#define VSNEG       0   // VS negative
#define VSPOS       1   // VS positive
#define SGL         1   // single-scan (or single-buffered)
#define DBL         2   // double-scan (or double-buffered)
#define QUAD        4   // quad-scan
#define DUP         1   // multi-scan duplicate
#define MSB         1   // multi-scan blank
#define OLD         1   // legacy indicator
#define NEW         0   // non-legacy indicator

// The VgaSettings structure identifies key information about each video mode.
//
typedef struct {
    uint8_t             m_mode;     // VDU video mode number
    uint8_t             m_colors;   // number of colors on main screen
    uint8_t             m_legacy;   // whether the mode uses legacy timing
    uint8_t             m_double;   // whether double-buffered
    const VgaTiming&    m_timing;   // timing information for the mode
} VgaSettings;

class VgaFrame
{
public:
    VgaFrame();
    ~VgaFrame();

private:
    FramePixels     m_frame_pixels;
};

extern DMA_ATTR VgaFrame vgaFrame;
