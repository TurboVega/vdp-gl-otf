/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2022 Fabrizio Di Vittorio.
  All rights reserved.


* Please contact fdivitto2013@gmail.com if you need a commercial license.


* This library and related software is available under GPL v3.

  FabGL is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  FabGL is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with FabGL.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once



/**
 * @file
 *
 * @brief This file contains fabgl::BitmappedDisplayController definition.
 */



#include <functional>
#include <vector>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <mat.h>
#include <dspm_mult.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "fabglconf.h"
#include "fabutils.h"

#include "dispdrivers/paintdefs.h"


namespace fabgl {



/*
  Notes:
    - all positions can have negative and outofbound coordinates. Shapes are always clipped correctly.
*/
enum PrimitiveCmd : uint8_t {

  // Needed to send the updated area of screen buffer on some displays (ie SSD1306)
  Flush,

  // Refresh display. Some displays (ie SSD1306) aren't repainted if there aren't primitives
  // so posting Refresh allows to resend the screenbuffer
  // params: rect (rectangle to refresh)
  Refresh,

  // Reset paint state
  // params: none
  Reset,

  // Set current pen color
  // params: color
  SetPenColor,

  // Set current brush color
  // params: color
  SetBrushColor,

  // Paint a pixel at specified coordinates, using current pen color
  // params: color
  SetPixel,

  // Paint a pixel at specified coordinates using the specified color
  // params: pixelDesc
  SetPixelAt,

  // Move current position to the specified one
  // params: point
  MoveTo,

  // Draw a line from current position to the specified one, using current pen color. Update current position.
  // params: point
  LineTo,

  // Fill a rectangle using current brush color
  // params: rect
  FillRect,

  // Draw a rectangle using current pen color
  // params: rect
  DrawRect,

  // Fill an ellipse, current position is the center, using current brush color
  // params: size
  FillEllipse,

  // Draw an ellipse, current position is the center, using current pen color
  // params: size
  DrawEllipse,

  // Draw an arc of a circle, current position is the center, using current pen color
  // params: rect (providing a startPoint and endPoint)
  DrawArc,

  // Draw a filled segment from a circle, current position is the center, using current brush color
  // params: rect (providing a startPoint and endPoint)
  FillSegment,

  // Draw a filled sector of a circle, current position is the center, using current brush color
  // params: rect (providing a startPoint and endPoint)
  FillSector,

  // Fill viewport with brush color
  // params: none
  Clear,

  // Scroll vertically without copying buffers
  // params: ivalue (scroll amount, can be negative)
  VScroll,

  // Scroll horizontally (time consuming operation!)
  // params: ivalue (scroll amount, can be negative)
  HScroll,

  // Draw a glyph (BW image)
  // params: glyph
  DrawGlyph,

  // Set paint options
  // params: glyphOptions
  SetGlyphOptions,

  // Set gluph options
  // params: paintOptions
  SetPaintOptions,

  // Invert a rectangle
  // params: rect
  InvertRect,

  // Copy (overlapping) rectangle to current position
  // params: rect (source rectangle)
  CopyRect,

  // Set scrolling region
  // params: rect
  SetScrollingRegion,

  // Swap foreground (pen) and background (brush) colors of all pixels inside the specified rectangles. Other colors remain untaltered.
  // params: rect
  SwapFGBG,

  // Render glyphs buffer
  // params: glyphsBufferRenderInfo
  RenderGlyphsBuffer,

  // Draw a bitmap
  // params: bitmapDrawingInfo
  DrawBitmap,

  // Copy a bitmap
  // params: bitmapDrawingInfo
  CopyToBitmap,

  // Draw a transformed bitmap
  // params: bitmapTransformedDrawingInfo
  DrawTransformedBitmap,

  // Refresh sprites
  // no params
  RefreshSprites,

  // Swap buffers (m_doubleBuffered must be True)
  // params: notifyTask
  SwapBuffers,

  // Fill a path, using current brush color
  // params: path
  FillPath,

  // Draw a path, using current pen color
  // params: path
  DrawPath,

  // Set axis origin
  // params: point
  SetOrigin,

  // Set clipping rectangle
  // params: rect
  SetClippingRect,

  // Set pen width
  // params: ivalue
  SetPenWidth,

  // Set line ends
  // params: lineEnds
  SetLineEnds,

  // Set line pattern
  // params: linePattern
  SetLinePattern,

  // Set line pattern length
  // params: ivalue
  SetLinePatternLength,

  // Set line pattern offset
  // params: ivalue
  SetLinePatternOffset,

  // Set line options
  // params: lineOptions
  SetLineOptions,
};

/** \ingroup Enumerations
 * @brief This enum defines a set of predefined mouse cursors.
 */
enum CursorName : uint8_t {
  CursorPointerAmigaLike,     /**< 11x11 Amiga like colored mouse pointer */
  CursorPointerSimpleReduced, /**< 10x15 mouse pointer */
  CursorPointerSimple,        /**< 11x19 mouse pointer */
  CursorPointerShadowed,      /**< 11x19 shadowed mouse pointer */
  CursorPointer,              /**< 12x17 mouse pointer */
  CursorPen,                  /**< 16x16 pen */
  CursorCross1,               /**< 9x9 cross */
  CursorCross2,               /**< 11x11 cross */
  CursorPoint,                /**< 5x5 point */
  CursorLeftArrow,            /**< 11x11 left arrow */
  CursorRightArrow,           /**< 11x11 right arrow */
  CursorDownArrow,            /**< 11x11 down arrow */
  CursorUpArrow,              /**< 11x11 up arrow */
  CursorMove,                 /**< 19x19 move */
  CursorResize1,              /**< 12x12 resize orientation 1 */
  CursorResize2,              /**< 12x12 resize orientation 2 */
  CursorResize3,              /**< 11x17 resize orientation 3 */
  CursorResize4,              /**< 17x11 resize orientation 4 */
  CursorTextInput,            /**< 7x15 text input */
};


/**
 * @brief Defines a cursor.
 */
struct Cursor {
  int16_t hotspotX;           /**< Cursor horizontal hotspot (0 = left bitmap side) */
  int16_t hotspotY;           /**< Cursor vertical hotspot (0 = upper bitmap side) */
  Bitmap  bitmap;             /**< Cursor bitmap */
};


struct QuadTreeObject;

/**
 * @brief Represents a sprite.
 *
 * A sprite contains one o more bitmaps (fabgl::Bitmap object) and has
 * a position in a scene (fabgl::Scane class). Only one bitmap is displayed at the time.<br>
 * It can be included in a collision detection group (fabgl::CollisionDetector class).
 * Bitmaps can have different sizes.
 */
struct Sprite {
  volatile int16_t   x;
  volatile int16_t   y;
  Bitmap * *         frames;  // array of pointer to Bitmap
  int16_t            framesCount;
  int16_t            currentFrame;
  int16_t            savedX;
  int16_t            savedY;
  int16_t            savedBackgroundWidth;
  int16_t            savedBackgroundHeight;
  uint8_t *          savedBackground;
  QuadTreeObject *   collisionDetectorObject;
  PaintOptions       paintOptions;
  struct {
    uint8_t visible:  1;
    // A static sprite should be positioned before dynamic sprites.
    // It is never re-rendered unless allowDraw is 1. Static sprites always sets allowDraw=0 after drawings.
    uint8_t isStatic:  1;
    // This is always '1' for dynamic sprites and always '0' for static sprites.
    uint8_t allowDraw: 1;
    // This tells the system to draw it as a hardware sprite (on-the-fly).
    uint8_t hardware:  1;
  };

  Sprite();
  ~Sprite();
  Bitmap * getFrame() { return frames ? frames[currentFrame] : nullptr; }
  int getFrameIndex() { return currentFrame; }
  void nextFrame() { currentFrame = (currentFrame  < framesCount - 1) ? currentFrame + 1 : 0; }
  Sprite * setFrame(int frame) { currentFrame = frame; return this; }
  Sprite * addBitmap(Bitmap * bitmap);
  Sprite * addBitmap(Bitmap * bitmap[], int count);
  void clearBitmaps();
  int getWidth()  { return frames[currentFrame]->width; }
  int getHeight() { return frames[currentFrame]->height; }
  Sprite * moveBy(int offsetX, int offsetY);
  Sprite * moveBy(int offsetX, int offsetY, int wrapAroundWidth, int wrapAroundHeight);
  Sprite * moveTo(int x, int y);
};

struct Primitive {
  PrimitiveCmd cmd;
  union {
    int16_t                ivalue;
    RGB888                 color;
    Point                  position;
    Size                   size;
    Glyph                  glyph;
    Rect                   rect;
    GlyphOptions           glyphOptions;
    PaintOptions           paintOptions;
    GlyphsBufferRenderInfo glyphsBufferRenderInfo;
    BitmapDrawingInfo      bitmapDrawingInfo;
    BitmapTransformedDrawingInfo bitmapTransformedDrawingInfo;
    Path                   path;
    PixelDesc              pixelDesc;
    LineEnds               lineEnds;
    LinePattern            linePattern;
    LineOptions            lineOptions;
    TaskHandle_t           notifyTask;
  } __attribute__ ((packed));

  Primitive() { }
  Primitive(PrimitiveCmd cmd_) : cmd(cmd_) { }
  Primitive(PrimitiveCmd cmd_, Rect const & rect_) : cmd(cmd_), rect(rect_) { }
} __attribute__ ((packed));


/** \ingroup Enumerations
 * @brief This enum defines types of display controllers
 */
enum class DisplayControllerType {
  Textual,      /**< The display controller can represents text only */
  Bitmapped,    /**< The display controller can represents text and bitmapped graphics */
};



/**
 * @brief Represents the base abstract class for all display controllers
 */
class BaseDisplayController {

public:

  virtual void setResolution(char const * modeline, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false) = 0;

  virtual void begin() = 0;

  /**
   * @brief Determines the display controller type
   *
   * @return Display controller type.
   */
  virtual DisplayControllerType controllerType() = 0;

  /**
   * @brief Determines the screen width in pixels.
   *
   * @return Screen width in pixels.
   */
  int getScreenWidth()                         { return m_screenWidth; }

  /**
   * @brief Determines the screen height in pixels.
   *
   * @return Screen height in pixels.
   */
  int getScreenHeight()                        { return m_screenHeight; }

  /**
   * @brief Determines horizontal size of the viewport.
   *
   * @return Horizontal size of the viewport.
   */
  int getViewPortWidth()                       { return m_viewPortWidth; }

  /**
   * @brief Determines vertical size of the viewport.
   *
   * @return Vertical size of the viewport.
   */
  int getViewPortHeight()                      { return m_viewPortHeight; }

protected:

  // inherited classes should call setScreenSize once display size is known
  void setScreenSize(int width, int height)    { m_screenWidth = width; m_screenHeight = height; }

  // we store here these info to avoid to have virtual methods (due the -vtables in flash- problem)
  int16_t          m_screenWidth;
  int16_t          m_screenHeight;
  volatile int16_t m_viewPortWidth;
  volatile int16_t m_viewPortHeight;

  // contains H and V signals for visible line
  volatile uint8_t m_HVSync;
};

/**
 * @brief Represents the base abstract class for bitmapped display controllers
 */
class BitmappedDisplayController : public BaseDisplayController {

public:

  BitmappedDisplayController();
  virtual ~BitmappedDisplayController();

  DisplayControllerType controllerType() { return DisplayControllerType::Bitmapped; }

  /**
   * @brief Represents the native pixel format used by this display.
   *
   * @return Display native pixel format
   */
  virtual NativePixelFormat nativePixelFormat() = 0;

  PaintState & paintState() { return m_paintState; }

  void addPrimitive(Primitive & primitive);

  void primitivesExecutionWait();

  /**
   * @brief Enables or disables drawings inside vertical retracing time.
   *
   * Primitives are processed in background (for example in vertical retracing for VGA or in a separated task for SPI/I2C displays).
   * This method can disable (or reenable) this behavior, making drawing instantaneous. Flickering may occur when
   * drawings are executed out of retracing time.<br>
   * When background executing is disabled the queue is emptied executing all pending primitives.
   * Some displays (SPI/I2C) may be not updated at all when enableBackgroundPrimitiveExecution is False.
   *
   * @param value When true drawings are done on background, when false drawings are executed instantly.
   */
  void enableBackgroundPrimitiveExecution(bool value);

  /**
   * @brief Enables or disables execution time limitation inside vertical retracing interrupt
   *
   * Disabling interrupt execution timeout may generate flickering but speedup drawing operations.
   *
   * @param value True enables timeout (default), False disables timeout
   */
  void enableBackgroundPrimitiveTimeout(bool value) { m_backgroundPrimitiveTimeoutEnabled = value; }

  bool backgroundPrimitiveTimeoutEnabled()          { return m_backgroundPrimitiveTimeoutEnabled; }

  /**
   * @brief Suspends drawings.
   *
   * Suspends drawings disabling vertical sync interrupt.<br>
   * After call to suspendBackgroundPrimitiveExecution() adding new primitives may cause a deadlock.<br>
   * To avoid it a call to "processPrimitives()" should be performed very often.<br>
   * This method maintains a counter so can be nested.
   */
  virtual void suspendBackgroundPrimitiveExecution() = 0;

  /**
   * @brief Resumes drawings after suspendBackgroundPrimitiveExecution().
   *
   * Resumes drawings enabling vertical sync interrupt.
   */
  virtual void resumeBackgroundPrimitiveExecution() = 0;

  /**
   * @brief Draws immediately all primitives in the queue.
   *
   * Draws all primitives before they are processed in the vertical sync interrupt.<br>
   * May generate flickering because don't care of vertical sync.
   */
  void processPrimitives();

  /**
   * @brief Sets the list of active sprites.
   *
   * A sprite is an image that keeps background unchanged.<br>
   * There is no limit to the number of active sprites, but flickering and slow
   * refresh happens when a lot of sprites (or large sprites) are visible.<br>
   * To empty the list of active sprites call BitmappedDisplayController.removeSprites().
   *
   * @param sprites The list of sprites to make currently active.
   * @param count Number of sprites in the list.
   *
   * Example:
   *
   *     // define a sprite with user data (velX and velY)
   *     struct MySprite : Sprite {
   *       int  velX;
   *       int  velY;
   *     };
   *
   *     static MySprite sprites[10];
   *
   *     VGAController.setSprites(sprites, 10);
   */
  template <typename T>
  void setSprites(T * sprites, int count) {
    setSprites(sprites, count, sizeof(T));
  }

  /**
   * @brief Empties the list of active sprites.
   *
   * Call this method when you don't need active sprites anymore.
   */
  void removeSprites() { setSprites(nullptr, 0, 0); }

  /**
   * @brief Forces the sprites to be updated.
   *
   * Screen is automatically updated whenever a primitive is painted (look at Canvas).<br>
   * When a sprite updates its image or its position (or any other property) it is required
   * to force a refresh using this method.<br>
   * BitmappedDisplayController.refreshSprites() is required also when using the double buffered mode, to paint sprites.
   */
  void refreshSprites();

  /**
   * @brief Determines whether BitmappedDisplayController is on double buffered mode.
   *
   * @return True if BitmappedDisplayController is on double buffered mode.
   */
  bool isDoubleBuffered() { return m_doubleBuffered; }

  /**
   * @brief Sets mouse cursor and make it visible.
   *
   * @param cursor Cursor to use when mouse pointer need to be painted. nullptr = disable mouse pointer.
   */
  void setMouseCursor(Cursor * cursor);

  /**
   * @brief Sets mouse cursor from a set of predefined cursors.
   *
   * @param cursorName Name (enum) of predefined cursor.
   *
   * Example:
   *
   *     VGAController.setMouseCursor(CursorName::CursorPointerShadowed);
   */
  void setMouseCursor(CursorName cursorName);

  /**
   * @brief Sets mouse cursor position.
   *
   * @param X Mouse cursor horizontal position.
   * @param Y Mouse cursor vertical position.
   */
  void setMouseCursorPos(int X, int Y);

  virtual void readScreen(Rect const & rect, RGB888 * destBuf) = 0;


  // statics (used for common default properties)

  /**
   * @brief Size of display controller primitives queue
   *
   * Application should change before begin() method.
   *
   * Default value is FABGLIB_DEFAULT_DISPLAYCONTROLLER_QUEUE_SIZE (defined in fabglconf.h)
   */
  static int queueSize;


protected:

  //// implemented methods

  void execPrimitive(Primitive const & prim, Rect & updateRect, bool insideISR);

  void updateAbsoluteClippingRect();

  RGB888 getActualPenColor();

  RGB888 getActualBrushColor();

  void lineTo(Point const & position, Rect & updateRect);

  void drawRect(Rect const & rect, Rect & updateRect);

  void drawPath(Path const & path, Rect & updateRect);

  void absDrawThickLine(int X1, int Y1, int X2, int Y2, int penWidth, RGB888 const & color);

  void fillRect(Rect const & rect, RGB888 const & color, Rect & updateRect);

  void fillEllipse(int centerX, int centerY, Size const & size, RGB888 const & color, Rect & updateRect);

  void fillPath(Path const & path, RGB888 const & color, Rect & updateRect);

  void renderGlyphsBuffer(GlyphsBufferRenderInfo const & glyphsBufferRenderInfo, Rect & updateRect);

  void setSprites(Sprite * sprites, int count, int spriteSize);

  Sprite * getSprite(int index);

  inline int spritesCount() { return m_spritesCount; }

  void hideSprites(Rect & updateRect);

  void showSprites(Rect & updateRect);

  void drawSpriteScanLine(uint8_t * pixelData, int scanRow, int scanWidth, int viewportHeight);

  void drawBitmap(BitmapDrawingInfo const & bitmapDrawingInfo, Rect & updateRect);

  void absDrawBitmap(int destX, int destY, Bitmap const * bitmap, void * saveBackground, bool ignoreClippingRect);

  void copyToBitmap(BitmapDrawingInfo const & bitmapCopyingInfo);

  void absCopyToBitmap(int srcX, int srcY, Bitmap const * bitmap);

  void drawBitmapWithTransform(BitmapTransformedDrawingInfo const & drawingInfo, Rect & updateRect);

  void setDoubleBuffered(bool value);

  bool getPrimitive(Primitive * primitive, int timeOutMS = 0);

  bool getPrimitiveISR(Primitive * primitive);

  void waitForPrimitives();

  Sprite * mouseCursor() { return &m_mouseCursor; }

  void resetPaintState();

private:

  void primitiveReplaceDynamicBuffers(Primitive & primitive);


  PaintState             m_paintState;

  volatile bool          m_doubleBuffered;
  volatile QueueHandle_t m_execQueue;

  bool                   m_backgroundPrimitiveExecutionEnabled; // when False primitives are execute immediately
  volatile bool          m_backgroundPrimitiveTimeoutEnabled;   // when False VSyncInterrupt() has not timeout

  volatile void *        m_sprites;       // pointer to array of sprite structures
  volatile int           m_spriteSize;    // size of sprite structure
  volatile int           m_spritesCount;  // number of sprites in m_sprites array
  bool                   m_spritesHidden; // true between hideSprites() and showSprites()

  // mouse cursor (mouse pointer) support
  Sprite                 m_mouseCursor;
  int16_t                m_mouseHotspotX;
  int16_t                m_mouseHotspotY;

  // memory pool used to allocate buffers of primitives
  LightMemoryPool        m_primDynMemPool;

};



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GenericBitmappedDisplayController


class GenericBitmappedDisplayController : public BitmappedDisplayController {

protected:
};


} // end of namespace



