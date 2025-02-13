// File: VgaFrame.h - unions that describe DMA buffers for video
// By: Curtis Whitley

#pragma once

#include "rom/lldesc.h"
#include "esp_attr.h"
#include <stdint.h>

#define BUF_UNION_TAG(w, h, c)          tag_PixBuf##w##x##h##x##c
#define BUF_UNION_NAME(w, h, c)         PixBuf##w##x##h##x##c
#define BUF_MEMBER_NAME(w, h, c)        m_##w##x##h##x##c
#define DBL_BUF_MEMBER_NAME(w, h, c)    m_dbl_##w##x##h##x##c
#define STRINGIZE(x)                    #x
#define BUF_MEMBER_NAME_STR(w, h, c)    STRINGIZE(BUF_MEMBER_NAME(w, h, c))
#define BUF_BITS_DIV(c)                 (c==2?8:(c==4?4:(c==16?2:1)))
#define NUM_SECTIONS                    8

// In this union, we represent 1/8th of the lines, so we can
// disperse them across multiple memory areas.
#define PIXEL_BUFFER(w, h, c) \
    typedef union BUF_UNION_TAG(w, h, c) { \
        uint8_t   m_8[w*h/BUF_BITS_DIV(c)/NUM_SECTIONS]; \
        uint16_t  m_16[w*h/BUF_BITS_DIV(c)/2/NUM_SECTIONS]; \
        uint32_t  m_32[w*h/BUF_BITS_DIV(c)/4/NUM_SECTIONS]; \
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
PIXEL_BUFFER(800, 600, 2)
PIXEL_BUFFER(800, 600, 4)
PIXEL_BUFFER(1024, 768, 2)
PIXEL_BUFFER(1024, 768, 4)

#define BUF_UNION_SIZE(w, h, c)         sizeof(BUF_UNION_NAME(w, h, c))
#define BUF_LEFTOVER_SIZE(w, h, c)      (sizeof(FramePixels) - BUF_UNION_SIZE(w, h, c))
#define DBL_BUF_UNION_SIZE(w, h, c)     (BUF_UNION_SIZE(w, h, c)*2)
#define DBL_BUF_LEFTOVER_SIZE(w, h, c)  (sizeof(FramePixels) - DBL_BUF_UNION_SIZE(w, h, c))

// The FramePixels union is used to allocate pixel data buffers statically.
//
typedef union tag_FramePixels {
    // Single-buffered modes
    BUF_UNION_NAME(320, 200, 2)    BUF_MEMBER_NAME(320, 200, 2);
    BUF_UNION_NAME(320, 200, 4)    BUF_MEMBER_NAME(320, 200, 4);
    BUF_UNION_NAME(320, 200, 16)   BUF_MEMBER_NAME(320, 200, 16);
    BUF_UNION_NAME(320, 200, 64)   BUF_MEMBER_NAME(320, 200, 64);
    BUF_UNION_NAME(320, 240, 2)    BUF_MEMBER_NAME(320, 240, 2);
    BUF_UNION_NAME(320, 240, 4)    BUF_MEMBER_NAME(320, 240, 4);
    BUF_UNION_NAME(320, 240, 16)   BUF_MEMBER_NAME(320, 240, 16);
    /*
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
    BUF_UNION_NAME(800, 600, 2)    BUF_MEMBER_NAME(800, 600, 2);
    BUF_UNION_NAME(800, 600, 4)    BUF_MEMBER_NAME(800, 600, 4);
    BUF_UNION_NAME(1024, 768, 2)   BUF_MEMBER_NAME(1024, 768, 2);
    BUF_UNION_NAME(1024, 768, 4)   BUF_MEMBER_NAME(1024, 768, 4);

    // Double-buffered modes
    BUF_UNION_NAME(320, 200, 2)    DBL_BUF_MEMBER_NAME(320, 200, 2)[2];
    BUF_UNION_NAME(320, 200, 4)    DBL_BUF_MEMBER_NAME(320, 200, 4)[2];
    BUF_UNION_NAME(320, 200, 16)   DBL_BUF_MEMBER_NAME(320, 200, 16)[2];
    BUF_UNION_NAME(320, 200, 64)   DBL_BUF_MEMBER_NAME(320, 200, 64)[2];
    BUF_UNION_NAME(320, 240, 2)    DBL_BUF_MEMBER_NAME(320, 240, 2)[2];
    BUF_UNION_NAME(320, 240, 4)    DBL_BUF_MEMBER_NAME(320, 240, 4)[2];
    BUF_UNION_NAME(320, 240, 16)   DBL_BUF_MEMBER_NAME(320, 240, 16)[2];
    BUF_UNION_NAME(320, 240, 64)   DBL_BUF_MEMBER_NAME(320, 240, 64)[2];
    BUF_UNION_NAME(512, 384, 2)    DBL_BUF_MEMBER_NAME(512, 384, 2)[2];
    BUF_UNION_NAME(512, 384, 4)    DBL_BUF_MEMBER_NAME(512, 384, 4)[2];
    BUF_UNION_NAME(512, 384, 16)   DBL_BUF_MEMBER_NAME(512, 384, 16)[2];
    BUF_UNION_NAME(640, 240, 2)    DBL_BUF_MEMBER_NAME(640, 240, 2)[2];
    BUF_UNION_NAME(640, 240, 4)    DBL_BUF_MEMBER_NAME(640, 240, 4)[2];
    BUF_UNION_NAME(640, 240, 16)   DBL_BUF_MEMBER_NAME(640, 240, 16)[2];
    BUF_UNION_NAME(640, 480, 2)    DBL_BUF_MEMBER_NAME(640, 480, 2)[2];
    BUF_UNION_NAME(640, 480, 4)    DBL_BUF_MEMBER_NAME(640, 480, 4)[2];
    BUF_UNION_NAME(800, 600, 2)    DBL_BUF_MEMBER_NAME(800, 600, 2)[2];
    BUF_UNION_NAME(1024, 768, 2)   DBL_BUF_MEMBER_NAME(1024, 768, 2)[2];
    */
} FramePixels;

typedef struct tag_VgaTiming {
    void finishInitialization(); // call this after construction

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
    uint8_t         m_h_sync_on;    // output bit mask for when HS is on
    uint8_t         m_v_sync_on;    // output bit mask for when VS is on
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
    uint8_t         m_h_sync_off;   // output bit mask for when HS is off
    uint8_t         m_v_sync_off;   // output bit mask for when VS is off
    uint8_t         m_hv_sync_on;   // combination of H & V sync bits when on
    uint8_t         m_hv_sync_off;  // combination of H & V sync bits when off
    uint32_t        m_hv_sync_4_on;   // combination of H & V sync bits when on (4 pixels)
    uint32_t        m_hv_sync_4_off;  // combination of H & V sync bits when off (4 pixels)
} VgaTiming;

#define HSBIT       0x40    // bit indicating value of HS
#define VSBIT       0x80    // bit indicating value of VS
#define HSNEG       0x00    // HS negative
#define HSPOS       HSBIT   // HS positive
#define VSNEG       0x00    // VS negative
#define VSPOS       VSBIT   // VS positive
#define SGL         1       // single-scan (or single-buffered)
#define DBL         2       // double-scan (or double-buffered)
#define QUAD        4       // quad-scan
#define DUP         1       // multi-scan duplicate
#define MSB         1       // multi-scan blank
#define OLD         1       // legacy indicator
#define NEW         0       // non-legacy indicator

// The VgaSettings structure identifies key information about each video mode.
//
typedef struct {
    uint8_t             m_mode;     // VDU video mode number
    uint8_t             m_colors;   // number of colors on main screen
    uint8_t             m_legacy;   // whether the mode uses legacy timing
    uint8_t             m_double;   // whether double-buffered
    uint32_t            m_size;     // size of the pixel buffer in FramePixels
    uint32_t            m_remain;   // size leftover in FramePixels
    const VgaTiming&    m_timing;   // timing information for the mode
} VgaSettings;

class VgaFrame
{
public:
    VgaFrame(); // No need to call this from the app! Use vgaFrame, below
    ~VgaFrame(); // This will never be called

    void finishInitialization(); // call this after construction
    int getNumModes(); // Gets the number of distinct video modes
    void listModes(); // Dump a list of all video modes to debug output

    // Get a reference to the settings for a video mode, based on the parameters
    const VgaSettings& getSettings(uint8_t mode, uint8_t colors, uint8_t legacy);

    // Get a reference to the timing for a video mode, based on the parameters
    // Note: if you call get_settings(), you get a reference to the timing, also
    const VgaTiming& getTiming(uint8_t mode, uint8_t colors, uint8_t legacy);

    // Get the size of one section of frame pixels
    uint32_t getSectionSize();

    // Get a reference to one section of frame pixels
    FramePixels& getSection(int index);

    // Set the video mode using the given parameters
    void setMode(uint8_t mode, uint8_t colors, uint8_t legacy);

    // Dump information about DMA descriptors to debug output
    void listDescriptors();

    // Stop the video output
    void stopVideo();

    // Start the video output
    void startVideo();

    // Clear the drawable frame buffer
    void clearScreen();
};

// Use this global variable to access the frame.
extern DMA_ATTR VgaFrame vgaFrame;
