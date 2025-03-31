/*
  videocontroller - Controls video output

  Created by Curtis Whitley, MAR 2025

  Parts copied from code based on the license reference below.
*/

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
 * @brief This file contains fabgl::VideoController definition.
 */

#include <system_error>
#include <functional>
#include <vector>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <atomic>
#include <mat.h>
#include <dspm_mult.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "fabglconf.h"
#include "fabutils.h"

#include "dispdrivers/painter.h"

#include "driver/gpio.h"

#include "devdrivers/swgenerator.h"

#include <unordered_map>


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

#if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  extern volatile uint64_t s_vgapalctrlcycles;
#endif

/** \ingroup Enumerations
 * @brief Represents one of the four blocks of horizontal or vertical line
 */
enum VGAScanStart {
  FrontPorch,   /**< Horizontal line sequence is: FRONTPORCH -> SYNC -> BACKPORCH -> VISIBLEAREA */
  Sync,         /**< Horizontal line sequence is: SYNC -> BACKPORCH -> VISIBLEAREA -> FRONTPORCH */
  BackPorch,    /**< Horizontal line sequence is: BACKPORCH -> VISIBLEAREA -> FRONTPORCH -> SYNC */
  VisibleArea   /**< Horizontal line sequence is: VISIBLEAREA -> FRONTPORCH -> SYNC -> BACKPORCH */
};

/** @brief Specifies the VGA timings. This is a modeline decoded. */
struct VGATimings {
  char          label[22];       /**< Resolution text description */
  int           frequency;       /**< Pixel frequency (in Hz) */
  int16_t       HVisibleArea;    /**< Horizontal visible area length in pixels */
  int16_t       HFrontPorch;     /**< Horizontal Front Porch duration in pixels */
  int16_t       HSyncPulse;      /**< Horizontal Sync Pulse duration in pixels */
  int16_t       HBackPorch;      /**< Horizontal Back Porch duration in pixels */
  int16_t       VVisibleArea;    /**< Vertical number of visible lines */
  int16_t       VFrontPorch;     /**< Vertical Front Porch duration in lines */
  int16_t       VSyncPulse;      /**< Vertical Sync Pulse duration in lines */
  int16_t       VBackPorch;      /**< Vertical Back Porch duration in lines */
  char          HSyncLogic;      /**< Horizontal Sync polarity '+' or '-' */
  char          VSyncLogic;      /**< Vertical Sync polarity '+' or '-' */
  uint8_t       scanCount;       /**< Scan count. 1 = single scan, 2 = double scan (allowing low resolutions like 320x240...) */
  uint8_t       multiScanBlack;  /**< 0 = Additional rows are the repetition of the first. 1 = Additional rows are blank. */
  VGAScanStart  HStartingBlock;  /**< Horizontal starting block. DetermineshHorizontal order of signals */
};

/**
 * @brief Represents the base abstract class for all display controllers
 */
class VideoController {

public:

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

public:

  VideoController();
  virtual ~VideoController();

  Painter * getPainter() { return m_painter; } // gets a pointer to the painter

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
   * To empty the list of active sprites call VideoController.removeSprites().
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
   * VideoController.refreshSprites() is required also when using the double buffered mode, to paint sprites.
   */
  void refreshSprites();

  /**
   * @brief Determines whether VideoController is on double buffered mode.
   *
   * @return True if VideoController is on double buffered mode.
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

  Painter *              m_painter;

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

  /**
   * @brief This is the 64 colors (8 GPIOs) initializer.
   *
   * Two GPIOs per channel, plus horizontal and vertical sync signals.
   *
   * @param red1GPIO GPIO to use for red channel, MSB bit.
   * @param red0GPIO GPIO to use for red channel, LSB bit.
   * @param green1GPIO GPIO to use for green channel, MSB bit.
   * @param green0GPIO GPIO to use for green channel, LSB bit.
   * @param blue1GPIO GPIO to use for blue channel, MSB bit.
   * @param blue0GPIO GPIO to use for blue channel, LSB bit.
   * @param HSyncGPIO GPIO to use for horizontal sync signal.
   * @param VSyncGPIO GPIO to use for vertical sync signal.
   *
   * Example:
   *
   *     // Use GPIO 22-21 for red, GPIO 19-18 for green, GPIO 5-4 for blue, GPIO 23 for HSync and GPIO 15 for VSync
   *     VGAController.begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_23, GPIO_NUM_15);
   */
  void begin(gpio_num_t red1GPIO, gpio_num_t red0GPIO, gpio_num_t green1GPIO, gpio_num_t green0GPIO, gpio_num_t blue1GPIO, gpio_num_t blue0GPIO, gpio_num_t HSyncGPIO, gpio_num_t VSyncGPIO);

  /**
   * @brief This is the 64 colors (8 GPIOs) initializer using default pinout.
   *
   * Two GPIOs per channel, plus horizontal and vertical sync signals.
   * Use GPIO 22-21 for red, GPIO 19-18 for green, GPIO 5-4 for blue, GPIO 23 for HSync and GPIO 15 for VSync
   *
   * Example:
   *
   *     VGAController.begin();
   */
  void begin();

  virtual void end();

  static bool convertModelineToTimings(char const * modeline, VGATimings * timings);

public:

  /**
   * @brief Sets current resolution using linux-like modeline.
   *
   * Modeline must have following syntax (non case sensitive):
   *
   *     "label" clock_mhz hdisp hsyncstart hsyncend htotal vdisp vsyncstart vsyncend vtotal (+HSync | -HSync) (+VSync | -VSync) [DoubleScan | QuadScan] [FrontPorchBegins | SyncBegins | BackPorchBegins | VisibleBegins] [MultiScanBlank]
   *
   * In fabglconf.h there are macros with some predefined modelines for common resolutions.
   * When MultiScanBlank and DoubleScan is specified then additional rows are not repeated, but just filled with blank lines.
   *
   * @param modeline Linux-like modeline as specified above.
   * @param viewPortWidth Horizontal viewport size in pixels. If less than zero (-1) it is sized to modeline visible area width.
   * @param viewPortHeight Vertical viewport size in pixels. If less then zero (-1) it is sized to maximum allocable.
   * @param doubleBuffered if True allocates another viewport of the same size to use as back buffer. Make sure there is enough free memory.
   *
   * Example:
   *
   *     // Use predefined modeline for 640x480@60Hz
   *     VGAController.setResolution(VGA_640x480_60Hz);
   *
   *     // The same of above using modeline string
   *     VGAController.setResolution("\"640x480@60Hz\" 25.175 640 656 752 800 480 490 492 525 -HSync -VSync");
   *
   *     // Set 640x382@60Hz but limit the viewport to 640x350
   *     VGAController.setResolution(VGA_640x382_60Hz, 640, 350);
   *
   */

  virtual void setResolution(char const * modeline, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false);

  virtual void setResolution(VGATimings const& timings, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false);

  volatile int m_primitiveProcessingSuspended;             // 0 = enabled, >0 suspended

  volatile uint32_t m_frameCounter;

private:

  /**
   * @brief Determines horizontal position of the viewport.
   *
   * @return Horizontal position of the viewport (in case viewport width is less than screen width).
   */
  int getViewPortCol()                             { return m_viewPortCol; }

  /**
   * @brief Determines vertical position of the viewport.
   *
   * @return Vertical position of the viewport (in case viewport height is less than screen height).
   */
  int getViewPortRow()                             { return m_viewPortRow; }

  /**
   * @brief Moves screen by specified horizontal and vertical offset.
   *
   * Screen moving is performed moving horizontal and vertical Front and Back porchs.
   *
   * @param offsetX Horizontal offset in pixels. < 0 goes left, > 0 goes right.
   * @param offsetY Vertical offset in pixels. < 0 goes up, > 0 goes down.
   *
   * Example:
   *
   *     // Move screen 4 pixels right, 1 pixel left
   *     VGAController.moveScreen(4, -1);
   */
  void moveScreen(int offsetX, int offsetY);

  /**
   * @brief Reduces or expands screen size by the specified horizontal and vertical offset.
   *
   * Screen shrinking is performed changing horizontal and vertical Front and Back porchs.
   *
   * @param shrinkX Horizontal offset in pixels. > 0 shrinks, < 0 expands.
   * @param shrinkY Vertical offset in pixels. > 0 shrinks, < 0 expands.
   *
   * Example:
   *
   *     // Shrink screen by 8 pixels and move by 8 pixels to the left
   *     VGAController.shrinkScreen(8, 0);
   *     VGAController.moveScreen(8, 0);
   */
  void shrinkScreen(int shrinkX, int shrinkY);

  VGATimings * getResolutionTimings()             { return &m_timings; }

  /**
   * @brief Gets number of bits allocated for each channel.
   *
   * Number of bits depends by which begin() initializer has been called.
   *
   * @return 1 (8 colors) or 2 (64 colors).
   */
  uint8_t getBitsPerChannel()                     { return m_bitsPerChannel; }

  /**
   * @brief Gets a raw scanline pointer.
   *
   * A raw scanline must be filled with raw pixel colors.
   * A raw pixel (or raw color) is a byte (uint8_t) that contains color information and synchronization signals.
   * Pixels are arranged in 32 bit packes as follows:
   *   pixel 0 = byte 2, pixel 1 = byte 3, pixel 2 = byte 0, pixel 3 = byte 1 :
   *   pixel : 0  1  2  3  4  5  6  7  8  9 10 11 ...etc...
   *   byte  : 2  3  0  1  6  7  4  5 10 11  8  9 ...etc...
   *   dword : 0           1           2          ...etc...
   *
   * @param y Vertical scanline position (0 = top row)
   */
  uint8_t * getScanline(int y)                    { return (uint8_t*) m_viewPort[y]; }

  bool setDMABuffersCount(int buffersCount);

  uint8_t packHVSync(bool HSync, bool VSync);

  uint8_t preparePixelWithSync(RGB222 rgb, bool HSync, bool VSync);

  void fillVertBuffers(int offsetY);

  void fillHorizBuffers(int offsetX);

  int fill(uint8_t volatile * buffer, int startPos, int length, uint8_t red, uint8_t green, uint8_t blue, bool hsync, bool vsync);

  int calcRequiredDMABuffersCount(int viewPortHeight);

  bool isMultiScanBlackLine(int scan);

  void setDMABufferBlank(int index, void volatile * address, int length, int scan, bool isStartOfVertFrontPorch);
  void setDMABufferView(int index, int row, int scan, volatile uint8_t * * viewPort, bool onVisibleDMA);
  void setDMABufferView(int index, int row, int scan, bool isStartOfVertFrontPorch);

  virtual void onSetupDMABuffer(lldesc_t volatile * buffer, bool isStartOfVertFrontPorch, int scan, bool isVisible, int visibleRow);

  void volatile * getDMABuffer(int index, int * length);

  void allocateViewPort(uint32_t allocCaps, int rowlen);
  virtual void allocateViewPort();
  virtual void checkViewPortSize() { };

  virtual void swapBuffers();

  // chance to overwrite a scan line in the output DMA buffer
  virtual void decorateScanLinePixels(uint8_t * pixels, uint16_t scanRow);

  // Processes primitives upon notification
  static void primitiveExecTask(void * arg);

  void calculateAvailableCyclesForDrawings();

protected:

  // when double buffer is enabled the "drawing" view port is always m_viewPort, while the "visible" view port is always m_viewPortVisible
  // when double buffer is not enabled then m_viewPort = m_viewPortVisible
  volatile uint8_t * *   m_viewPort;
  volatile uint8_t * *   m_viewPortVisible;

  // true: double buffering is implemented in DMA
  bool                   m_doubleBufferOverDMA;

  intr_handle_t          m_isr_handle;

  VGATimings             m_timings;
  int16_t                m_HLineSize;

  volatile int16_t       m_viewPortCol;
  volatile int16_t       m_viewPortRow;

  volatile uint8_t * *        m_lines;

  // optimization: clones of m_viewPort and m_viewPortVisible
  static volatile uint8_t * * s_viewPort;
  static volatile uint8_t * * s_viewPortVisible;

  static lldesc_t volatile *  s_frameResetDesc;
  static volatile int         s_scanLine;
  static volatile int         s_scanWidth;
  static volatile int         s_viewPortHeight;

  int                         m_linesCount;     // viewport height must be divisible by m_linesCount

  volatile bool               m_taskProcessingPrimitives;
  TaskHandle_t                m_primitiveExecTask;

  // Maximum time (in CPU cycles) available for primitives drawing
  volatile uint32_t           m_primitiveExecTimeoutCycles;

  // true = allowed time to process primitives is limited to the vertical blank. Slow, but avoid flickering
  // false = allowed time is the half of an entire frame. Fast, but may flick
  bool                        m_processPrimitivesOnBlank;

private:
  // bits per channel on VGA output
  // 1 = 8 colors, 2 = 64 colors, set by begin()
  int                    m_bitsPerChannel;

  GPIOStream             m_GPIOStream;

  lldesc_t volatile *    m_DMABuffers;
  int                    m_DMABuffersCount;

  // when double buffer is enabled at DMA level the running DMA buffer is always m_DMABuffersVisible
  // when double buffer is not enabled then m_DMABuffers = m_DMABuffersVisible
  lldesc_t volatile *    m_DMABuffersHead;
  lldesc_t volatile *    m_DMABuffersVisible;

  // These buffers contains a full line, with FrontPorch, Sync, BackPorch and blank visible area, in the
  // order specified by timings.HStartingBlock
  volatile uint8_t *     m_HBlankLine_withVSync;
  volatile uint8_t *     m_HBlankLine;

  uint8_t * *            m_viewPortMemoryPool;  // array ends with nullptr

  int16_t                m_rawFrameHeight;


  NativePixelFormat nativePixelFormat() { return m_nativePixelFormat; }

  /**
   * @brief Determines the maximum time allowed to process primitives
   *
   * Primitives processing is always started at the beginning of vertical blank.
   * Unfortunately this time is limited and not all primitive may be processed, so processing all primitives may required more frames.
   * This method expands the allowed time to half of a frame. This increase drawing speed but may show some flickering.
   *
   * The default is False (fast drawings, possible flickering).
   *
   * @param value True = allowed time to process primitives is limited to the vertical blank. Slow, but avoid flickering. False = allowed time is the half of an entire frame. Fast, but may flick.
   */
  void setProcessPrimitivesOnBlank(bool value)          { m_processPrimitivesOnBlank = value; }

protected:

  void init();

private:

  // configuration
  int                         m_columnsQuantum; // viewport width must be divisble by m_columnsQuantum
  NativePixelFormat           m_nativePixelFormat;
  int                         m_viewPortRatioDiv;
  int                         m_viewPortRatioMul;
  intr_handler_t              m_isrHandler;

};

} // end of namespace

