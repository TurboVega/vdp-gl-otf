// vgaotfcontroller.h - controls OTF display mode for VGA
//
// by Curtis Whitley
//

#include "dispdrivers/vgabasecontroller.h"

#define OTF_LINES_COUNT                 2
#define OTF_NUM_PHYSICAL_SCAN_LINES     8

namespace fabgl {

class VGAOtfController : public VGABaseController {
    public:

    VGAOtfController();

    ~VGAOtfController();

    // We override this function, because the number of allocated scan lines
    // will be less than the number of pixel rows on the screen.
    virtual void setNumScanLines();

    virtual void end();
    virtual void onSetupDMABuffer(lldesc_t volatile * buffer, bool isStartOfVertFrontPorch, int scan, bool isVisible, int visibleRow);
    virtual void allocateViewPort();
    virtual void freeViewPort();
    virtual NativePixelFormat nativePixelFormat();
    virtual void readScreen(Rect const & rect, RGB888 * destBuf);
    virtual void setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect);
    virtual void absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color);
    virtual void fillRow(int y, int x1, int x2, RGB888 color);
    virtual void drawEllipse(Size const & size, Rect & updateRect);
    virtual void drawArc(Rect const & rect, Rect & updateRect);
    virtual void fillSegment(Rect const & rect, Rect & updateRect);
    virtual void fillSector(Rect const & rect, Rect & updateRect);
    virtual void clear(Rect & updateRect);
    virtual void VScroll(int scroll, Rect & updateRect);
    virtual void HScroll(int scroll, Rect & updateRect);
    virtual void drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect);
    virtual void invertRect(Rect const & rect, Rect & updateRect);
    virtual void swapFGBG(Rect const & rect, Rect & updateRect);
    virtual void copyRect(Rect const & source, Rect & updateRect);
    virtual int getBitmapSavePixelSize();
    virtual void rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount);
    virtual void rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount);
    virtual void rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount);
    virtual void rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount);
    virtual void rawCopyToBitmap(int srcX, int srcY, int width, void * saveBuffer, int X1, int Y1, int XCount, int YCount);
    virtual void rawDrawBitmapWithMatrix_Mask(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix);
    virtual void rawDrawBitmapWithMatrix_RGBA2222(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix);
    virtual void rawDrawBitmapWithMatrix_RGBA8888(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix);

    static void ISRHandler(void * arg);

protected:
  volatile uint8_t * *        m_lines;

  // optimization: clones of m_viewPort and m_viewPortVisible
  static volatile uint8_t * * s_viewPort;
  static volatile uint8_t * * s_viewPortVisible;

  static lldesc_t volatile *  s_frameResetDesc;

  static volatile int         s_scanLine;
};

} // namespace fabgl
