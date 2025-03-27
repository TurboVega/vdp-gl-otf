/*
  paintdefs - Common definitions for use in painting functions

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

#include <stdint.h>

namespace fabgl {

#define TORAD(a) ((a) * M_PI / 180.)


// Integer square root by Halleck's method, with Legalize's speedup
int isqrt (int x);


template <typename T>
const T & tmax(const T & a, const T & b)
{
  return (a < b) ? b : a;
}


constexpr auto imax = tmax<int>;


template <typename T>
const T & tmin(const T & a, const T & b)
{
  return !(b < a) ? a : b;
}


constexpr auto imin = tmin<int>;



template <typename T>
const T & tclamp(const T & v, const T & lo, const T & hi)
{
  return (v < lo ? lo : (v > hi ? hi : v));
}


constexpr auto iclamp = tclamp<int>;


template <typename T>
const T & twrap(const T & v, const T & lo, const T & hi)
{
  return (v < lo ? hi : (v > hi ? lo : v));
}


template <typename T>
void tswap(T & v1, T & v2)
{
  T t = v1;
  v1 = v2;
  v2 = t;
}


constexpr auto iswap = tswap<int>;


template <typename T>
T moveItems(T dest, T src, size_t n)
{
  T pd = dest;
  T ps = src;
  if (pd != ps) {
    if (ps < pd)
      for (pd += n, ps += n; n--;)
        *--pd = *--ps;
    else
      while (n--)
        *pd++ = *ps++;
  }
  return dest;
}


/** \ingroup Enumerations
 * @brief This enum defines named colors.
 *
 * First eight full implement all available colors when 1 bit per channel mode is used (having 8 colors).
 */
enum Color {
  Black,          /**< Equivalent to RGB888(0,0,0) */
  Red,            /**< Equivalent to RGB888(128,0,0) */
  Green,          /**< Equivalent to RGB888(0,128,0) */
  Yellow,         /**< Equivalent to RGB888(128,128,0) */
  Blue,           /**< Equivalent to RGB888(0,0,128) */
  Magenta,        /**< Equivalent to RGB888(128,0,128) */
  Cyan,           /**< Equivalent to RGB888(0,128,128) */
  White,          /**< Equivalent to RGB888(128,128,128) */
  BrightBlack,    /**< Equivalent to RGB888(64,64,64) */
  BrightRed,      /**< Equivalent to RGB888(255,0,0) */
  BrightGreen,    /**< Equivalent to RGB888(0,255,0) */
  BrightYellow,   /**< Equivalent to RGB888(255,255,0) */
  BrightBlue,     /**< Equivalent to RGB888(0,0,255) */
  BrightMagenta,  /**< Equivalent to RGB888(255,0,255) */
  BrightCyan,     /**< Equivalent to RGB888(0,255,255) */
  BrightWhite,    /**< Equivalent to RGB888(255,255,255) */
};



/**
 * @brief Represents a 24 bit RGB color.
 *
 * For each channel minimum value is 0, maximum value is 255.
 */
struct RGB888 {
  uint8_t R;  /**< The Red channel   */
  uint8_t G;  /**< The Green channel */
  uint8_t B;  /**< The Blue channel  */

  RGB888() : R(0), G(0), B(0) { }
  RGB888(Color color);
  RGB888(uint8_t red, uint8_t green, uint8_t blue) : R(red), G(green), B(blue) { }
} __attribute__ ((packed));


inline bool operator==(RGB888 const& lhs, RGB888 const& rhs)
{
  return lhs.R == rhs.R && lhs.G == rhs.G && lhs.B == rhs.B;
}


inline bool operator!=(RGB888 const& lhs, RGB888 const& rhs)
{
  return lhs.R != rhs.R || lhs.G != rhs.G || lhs.B == rhs.B;
}



/**
 * @brief Represents a 32 bit RGBA color.
 *
 * For each channel minimum value is 0, maximum value is 255.
 */
struct RGBA8888 {
  uint8_t R;  /**< The Red channel   */
  uint8_t G;  /**< The Green channel */
  uint8_t B;  /**< The Blue channel  */
  uint8_t A;  /**< The Alpha channel */

  RGBA8888() : R(0), G(0), B(0), A(0) { }
  RGBA8888(int red, int green, int blue, int alpha) : R(red), G(green), B(blue), A(alpha) { }
};



/**
 * @brief Represents a 6 bit RGB color.
 *
 * When 1 bit per channel (8 colors) is used then the maximum value (white) is 1 (R = 1, G = 1, B = 1).
 * When 2 bits per channel (64 colors) are used then the maximum value (white) is 3 (R = 3, G = 3, B = 3).
 */
struct RGB222 {
  uint8_t R : 2;  /**< The Red channel   */
  uint8_t G : 2;  /**< The Green channel */
  uint8_t B : 2;  /**< The Blue channel  */

  RGB222() : R(0), G(0), B(0) { }
  RGB222(uint8_t red, uint8_t green, uint8_t blue) : R(red), G(green), B(blue) { }
  RGB222(RGB888 const & value);

  static bool lowBitOnly;  // true= 8 colors, false 64 colors
};


inline bool operator==(RGB222 const& lhs, RGB222 const& rhs)
{
  return lhs.R == rhs.R && lhs.G == rhs.G && lhs.B == rhs.B;
}


inline bool operator!=(RGB222 const& lhs, RGB222 const& rhs)
{
  return lhs.R != rhs.R || lhs.G != rhs.G || lhs.B == rhs.B;
}


/**
 * @brief Represents an 8 bit ABGR color
 *
 * For each channel minimum value is 0, maximum value is 3.
 */
struct RGBA2222 {
  uint8_t R : 2;  /**< The Red channel (LSB) */
  uint8_t G : 2;  /**< The Green channel */
  uint8_t B : 2;  /**< The Blue channel  */
  uint8_t A : 2;  /**< The Alpha channel (MSB) */

  RGBA2222(int red, int green, int blue, int alpha) : R(red), G(green), B(blue), A(alpha) { }
};


//   0 .. 63  => 0
//  64 .. 127 => 1
// 128 .. 191 => 2
// 192 .. 255 => 3
uint8_t RGB888toPackedRGB222(RGB888 const & rgb);


/**
 * @brief Represents a glyph position, size and binary data.
 *
 * A glyph is a bitmap (1 bit per pixel). The fabgl::Terminal uses glyphs to render characters.
 */
struct Glyph {
  int16_t         X;      /**< Horizontal glyph coordinate */
  int16_t         Y;      /**< Vertical glyph coordinate */
  uint8_t         width;  /**< Glyph horizontal size */
  uint8_t         height; /**< Glyph vertical size */
  uint8_t const * data;   /**< Byte aligned binary data of the glyph. A 0 represents background or a transparent pixel. A 1 represents foreground. */

  Glyph() : X(0), Y(0), width(0), height(0), data(nullptr) { }
  Glyph(int X_, int Y_, int width_, int height_, uint8_t const * data_) : X(X_), Y(Y_), width(width_), height(height_), data(data_) { }
}  __attribute__ ((packed));


/**
 * @brief Specifies various glyph painting options.
 */
union GlyphOptions {
  struct {
    uint16_t fillBackground   : 1;  /**< If enabled glyph background is filled with current background color. */
    uint16_t bold             : 1;  /**< If enabled produces a bold-like style. */
    uint16_t reduceLuminosity : 1;  /**< If enabled reduces luminosity. To implement characters faint. */
    uint16_t italic           : 1;  /**< If enabled skews the glyph on the right. To implement characters italic. */
    uint16_t invert           : 1;  /**< If enabled swaps foreground and background colors. To implement characters inverse (XORed with PaintOptions.swapFGBG) */
    uint16_t blank            : 1;  /**< If enabled the glyph is filled with the background color. To implement characters invisible or blink. */
    uint16_t underline        : 1;  /**< If enabled the glyph is underlined. To implement characters underline. */
    uint16_t doubleWidth      : 2;  /**< If enabled the glyph is doubled. To implement characters double width. 0 = normal, 1 = double width, 2 = double width - double height top, 3 = double width - double height bottom. */
    uint16_t userOpt1         : 1;  /**< User defined option */
    uint16_t userOpt2         : 1;  /**< User defined option */
  };
  uint16_t value;

  /** @brief Helper method to set or reset fillBackground */
  GlyphOptions & FillBackground(bool value) { fillBackground = value; return *this; }

  /** @brief Helper method to set or reset bold */
  GlyphOptions & Bold(bool value) { bold = value; return *this; }

  /** @brief Helper method to set or reset italic */
  GlyphOptions & Italic(bool value) { italic = value; return *this; }

  /** @brief Helper method to set or reset underlined */
  GlyphOptions & Underline(bool value) { underline = value; return *this; }

  /** @brief Helper method to set or reset doubleWidth */
  GlyphOptions & DoubleWidth(uint8_t value) { doubleWidth = value; return *this; }

  /** @brief Helper method to set or reset foreground and background swapping */
  GlyphOptions & Invert(uint8_t value) { invert = value; return *this; }

  /** @brief Helper method to set or reset foreground and background swapping */
  GlyphOptions & Blank(uint8_t value) { blank = value; return *this; }
} __attribute__ ((packed));


// GlyphsBuffer.map support functions
//  0 ..  7 : index
//  8 .. 11 : BG color (Color)
// 12 .. 15 : FG color (Color)
// 16 .. 31 : options (GlyphOptions)
// note: volatile pointer to avoid optimizer to get less than 32 bit from 32 bit access only memory
#define GLYPHMAP_INDEX_BIT    0
#define GLYPHMAP_BGCOLOR_BIT  8
#define GLYPHMAP_FGCOLOR_BIT 12
#define GLYPHMAP_OPTIONS_BIT 16
#define GLYPHMAP_ITEM_MAKE(index, bgColor, fgColor, options) (((uint32_t)(index) << GLYPHMAP_INDEX_BIT) | ((uint32_t)(bgColor) << GLYPHMAP_BGCOLOR_BIT) | ((uint32_t)(fgColor) << GLYPHMAP_FGCOLOR_BIT) | ((uint32_t)((options).value) << GLYPHMAP_OPTIONS_BIT))

inline uint8_t glyphMapItem_getIndex(uint32_t const volatile * mapItem) { return *mapItem >> GLYPHMAP_INDEX_BIT & 0xFF; }
inline uint8_t glyphMapItem_getIndex(uint32_t const & mapItem)          { return mapItem >> GLYPHMAP_INDEX_BIT & 0xFF; }

inline Color glyphMapItem_getBGColor(uint32_t const volatile * mapItem) { return (Color)(*mapItem >> GLYPHMAP_BGCOLOR_BIT & 0x0F); }
inline Color glyphMapItem_getBGColor(uint32_t const & mapItem)          { return (Color)(mapItem >> GLYPHMAP_BGCOLOR_BIT & 0x0F); }

inline Color glyphMapItem_getFGColor(uint32_t const volatile * mapItem) { return (Color)(*mapItem >> GLYPHMAP_FGCOLOR_BIT & 0x0F); }
inline Color glyphMapItem_getFGColor(uint32_t const & mapItem)          { return (Color)(mapItem >> GLYPHMAP_FGCOLOR_BIT & 0x0F); }

inline GlyphOptions glyphMapItem_getOptions(uint32_t const volatile * mapItem) { return (GlyphOptions){.value = (uint16_t)(*mapItem >> GLYPHMAP_OPTIONS_BIT & 0xFFFF)}; }
inline GlyphOptions glyphMapItem_getOptions(uint32_t const & mapItem)          { return (GlyphOptions){.value = (uint16_t)(mapItem >> GLYPHMAP_OPTIONS_BIT & 0xFFFF)}; }

inline void glyphMapItem_setOptions(uint32_t volatile * mapItem, GlyphOptions const & options) { *mapItem = (*mapItem & ~((uint32_t)0xFFFF << GLYPHMAP_OPTIONS_BIT)) | ((uint32_t)(options.value) << GLYPHMAP_OPTIONS_BIT); }

struct GlyphsBuffer {
  int16_t         glyphsWidth;
  int16_t         glyphsHeight;
  uint8_t const * glyphsData;
  int16_t         columns;
  int16_t         rows;
  uint32_t *      map;  // look at glyphMapItem_... inlined functions
};


struct GlyphsBufferRenderInfo {
  int16_t              itemX;  // starts from 0
  int16_t              itemY;  // starts from 0
  GlyphsBuffer const * glyphsBuffer;

  GlyphsBufferRenderInfo(int itemX_, int itemY_, GlyphsBuffer const * glyphsBuffer_) : itemX(itemX_), itemY(itemY_), glyphsBuffer(glyphsBuffer_) { }
} __attribute__ ((packed));


/** \ingroup Enumerations
 * @brief This enum defines the display controller native pixel format
 */
enum class NativePixelFormat : uint8_t {
  Mono,       /**< 1 bit per pixel. 0 = black, 1 = white */
  SBGR2222,   /**< 8 bit per pixel: VHBBGGRR (bit 7=VSync 6=HSync 5=B 4=B 3=G 2=G 1=R 0=R). Each color channel can have values from 0 to 3 (maxmum intensity). */
  RGB565BE,   /**< 16 bit per pixel: RGB565 big endian. */
  PALETTE2,   /**< 1 bit palette (2 colors), packed as 8 pixels per byte. */
  PALETTE4,   /**< 2 bit palette (4 colors), packed as 4 pixels per byte. */
  PALETTE8,   /**< 3 bit palette (8 colors), packed as 8 pixels every 3 bytes. */
  PALETTE16,  /**< 4 bit palette (16 colors), packed as 2 pixels per byte. */
  PALETTE64,  /**< 8 bit palette (64 colors), packed as 1 pixel per byte. */
};


/** \ingroup Enumerations
 * @brief This enum defines a pixel format
 */
enum class PixelFormat : uint8_t {
  Undefined,  /**< Undefined pixel format */
  Native,     /**< Native device pixel format */
  Mask,       /**< 1 bit per pixel. 0 = transparent, 1 = opaque (color must be specified apart) */
  RGBA2222,   /**< 8 bit per pixel: AABBGGRR (bit 7=A 6=A 5=B 4=B 3=G 2=G 1=R 0=R). AA = 0 fully transparent, AA = 3 fully opaque. Each color channel can have values from 0 to 3 (maxmum intensity). */
  RGBA8888    /**< 32 bits per pixel: RGBA (R=byte 0, G=byte1, B=byte2, A=byte3). Minimum value for each channel is 0, maximum is 255. */
};


/** \ingroup Enumerations
* @brief This enum defines line ends when pen width is greater than 1
*/
enum class LineEnds : uint8_t {
  None,    /**< No line ends */
  Circle,  /**< Circle line ends */
};


enum class PaintMode : uint8_t {
  Set = 0,      // Plot colour
  OR = 1,       // OR colour onto screen
  AND = 2,      // AND colour onto screen
  XOR = 3,      // XOR colour onto screen
  Invert = 4,   // Invert colour on screen
  NOT = 4,      // NOT colour onto screen, alias for Invert
  NoOp = 5,     // No operation
  ANDNOT = 6,   // AND colour on screen with NOT colour
  ORNOT = 7,    // OR colour on screen with NOT colour
};

/**
 * @brief Specifies general paint options.
 */
struct PaintOptions {
  uint8_t swapFGBG : 1;  /**< If enabled swaps foreground and background colors */
  uint8_t NOT      : 1;  /**< If enabled performs NOT logical operator on destination. Implemented only for straight lines and non-filled rectangles. */
  PaintMode mode   : 3;  /**< Paint mode */

  PaintOptions() : swapFGBG(false), NOT(false), mode(PaintMode::Set) { }
} __attribute__ ((packed));


/**
 * @brief Represents the coordinate of a point.
 *
 * Coordinates start from 0.
 */
struct Point {
  int16_t X;    /**< Horizontal coordinate */
  int16_t Y;    /**< Vertical coordinate */

  Point() : X(0), Y(0) { }
  Point(int X_, int Y_) : X(X_), Y(Y_) { }
  
  Point add(Point const & p) const { return Point(X + p.X, Y + p.Y); }
  Point sub(Point const & p) const { return Point(X - p.X, Y - p.Y); }
  Point neg() const                { return Point(-X, -Y); }
  bool operator==(Point const & r) { return X == r.X && Y == r.Y; }
  bool operator!=(Point const & r) { return X != r.X || Y != r.Y; }
} __attribute__ ((packed));


/**
 * @brief Represents a bidimensional size.
 */
struct Size {
  int16_t width;   /**< Horizontal size */
  int16_t height;  /**< Vertical size */

  Size() : width(0), height(0) { }
  Size(int width_, int height_) : width(width_), height(height_) { }
  bool operator==(Size const & r) { return width == r.width && height == r.height; }
  bool operator!=(Size const & r) { return width != r.width || height != r.height; }
} __attribute__ ((packed));


struct Path {
  Point const * points;
  int           pointsCount;
  bool          freePoints; // deallocate points after drawing
} __attribute__ ((packed));


struct PixelDesc {
  Point  pos;
  RGB888 color;
} __attribute__ ((packed));


struct LinePattern {
  uint8_t pattern[8] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
  uint8_t offset = 0;  // bit offset in pattern, automatically updated whilst drawing a line

  void setPattern(const uint8_t newPattern[8]) {
    for (int i = 0; i < 8; ++i) {
      pattern[i] = newPattern[i];
    }
  }
} __attribute__ ((packed));


struct LineOptions {
  bool usePattern = false;
  bool omitFirst = false;
  bool omitLast = false;
} __attribute__ ((packed));


/**
 * @brief Represents a rectangle.
 *
 * Top and Left coordinates start from 0.
 */
struct Rect {
  int16_t X1;   /**< Horizontal top-left coordinate */
  int16_t Y1;   /**< Vertical top-left coordinate */
  int16_t X2;   /**< Horizontal bottom-right coordinate */
  int16_t Y2;   /**< Vertical bottom-right coordinate */

  Rect() : X1(0), Y1(0), X2(0), Y2(0) { }
  Rect(int X1_, int Y1_, int X2_, int Y2_) : X1(X1_), Y1(Y1_), X2(X2_), Y2(Y2_) { }
  Rect(Rect const & r) { X1 = r.X1; Y1 = r.Y1; X2 = r.X2; Y2 = r.Y2; }

  bool operator==(Rect const & r)                { return X1 == r.X1 && Y1 == r.Y1 && X2 == r.X2 && Y2 == r.Y2; }
  bool operator!=(Rect const & r)                { return X1 != r.X1 || Y1 != r.Y1 || X2 != r.X2 || Y2 != r.Y2; }
  Point pos() const                              { return Point(X1, Y1); }
  Size size() const                              { return Size(X2 - X1 + 1, Y2 - Y1 + 1); }
  int width() const                              { return X2 - X1 + 1; }
  int height() const                             { return Y2 - Y1 + 1; }
  Rect translate(int offsetX, int offsetY) const { return Rect(X1 + offsetX, Y1 + offsetY, X2 + offsetX, Y2 + offsetY); }
  Rect translate(Point const & offset) const     { return Rect(X1 + offset.X, Y1 + offset.Y, X2 + offset.X, Y2 + offset.Y); }
  Rect move(Point const & position) const        { return Rect(position.X, position.Y, position.X + width() - 1, position.Y + height() - 1); }
  Rect move(int x, int y) const                  { return Rect(x, y, x + width() - 1, y + height() - 1); }
  Rect shrink(int value) const                   { return Rect(X1 + value, Y1 + value, X2 - value, Y2 - value); }
  Rect hShrink(int value) const                  { return Rect(X1 + value, Y1, X2 - value, Y2); }
  Rect vShrink(int value) const                  { return Rect(X1, Y1 + value, X2, Y2 - value); }
  Rect resize(int width, int height) const       { return Rect(X1, Y1, X1 + width - 1, Y1 + height - 1); }
  Rect resize(Size size) const                   { return Rect(X1, Y1, X1 + size.width - 1, Y1 + size.height - 1); }
  Rect intersection(Rect const & rect) const;
  bool intersects(Rect const & rect) const       { return X1 <= rect.X2 && X2 >= rect.X1 && Y1 <= rect.Y2 && Y2 >= rect.Y1; }
  bool contains(Rect const & rect) const         { return (rect.X1 >= X1) && (rect.Y1 >= Y1) && (rect.X2 <= X2) && (rect.Y2 <= Y2); }
  bool contains(Point const & point) const       { return point.X >= X1 && point.Y >= Y1 && point.X <= X2 && point.Y <= Y2; }
  bool contains(int x, int y) const              { return x >= X1 && y >= Y1 && x <= X2 && y <= Y2; }
  Rect merge(Rect const & rect) const;
} __attribute__ ((packed));


struct PaintState {
  RGB888       penColor;
  RGB888       brushColor;
  Point        position;        // value already translated to "origin"
  GlyphOptions glyphOptions;
  PaintOptions paintOptions;
  Rect         scrollingRegion;
  Point        origin;
  Rect         clippingRect;    // relative clipping rectangle
  Rect         absClippingRect; // actual absolute clipping rectangle (calculated when setting "origin" or "clippingRect")
  int16_t      penWidth;
  LineEnds     lineEnds;
  LineOptions  lineOptions;
  LinePattern  linePattern;
  int8_t       linePatternLength;
};


/**
 * @brief Works out which quadrant a relative point is in, from a 0, 0 center.
 * Quadrants are numbered 0-3, top right to bottom right, anticlockwise.
 */
uint8_t getCircleQuadrant(int x, int y);


/**
 * @brief Represents a walkable line, with integer coordinates.
 *
 * Uses Bresenham's algorithm to calculate the points of the line.
 * Used by arc, sector and segment drawing functions.
 * Includes tracking for minimum and maximum X coordinates.
 */
struct LineInfo {
  int16_t X1;         /**< Horizontal start coordinate */
  int16_t Y1;         /**< Vertical start coordinate */
  int16_t X2;         /**< Horizontal end coordinate */
  int16_t Y2;         /**< Vertical end coordinate */
  int16_t CX;         /**< Center horizontal coordinate (for quadrant identification) */
  int16_t CY;         /**< Center vertical coordinate (for quadrant identification) */
  int16_t minX;       /**< Minimum horizontal coordinate for current line */
  int16_t maxX;       /**< Maximum horizontal coordinate for current line */
  int16_t x;          /**< Current horizontal coordinate */
  int16_t y;          /**< Current vertical coordinate */
  int16_t deltaX;     /**< Horizontal distance */
  int16_t deltaY;     /**< Vertical distance */
  int16_t absDeltaX;  /**< Absolute horizontal distance */
  int16_t absDeltaY;  /**< Absolute vertical distance */
  int16_t sx;         /**< Horizontal step */
  int16_t sy;         /**< Vertical step */
  int16_t error;      /**< Base Bresenham Error factor */
  int16_t err;        /**< Working error value */
  uint8_t quadrant;   /**< Quadrant of line (0..3) (top right to bottom right, anticlockwise) */
  bool    hasPixels;  /**< True when currently checked line has pixels */

  LineInfo() : X1(0), Y1(0), X2(0), Y2(0), CX(0), CY(0) { reset(); }
  LineInfo(int16_t X1_, int16_t Y1_, int16_t X2_, int16_t Y2_) : X1(X1_), Y1(Y1_), X2(X2_), Y2(Y2_), CX(X1_), CY(Y1_) { reset(); }
  LineInfo(int16_t X1_, int16_t Y1_, int16_t X2_, int16_t Y2_, int16_t CX_, int16_t CY_) : X1(X1_), Y1(Y1_), X2(X2_), Y2(Y2_), CX(CX_), CY(CY_) { reset(); }
  LineInfo(Point const & p1, Point const & p2) : X1(p1.X), Y1(p1.Y), X2(p2.X), Y2(p2.Y), CX(p1.X), CY(p1.Y) { reset(); }
  LineInfo(Point const & p1, Point const & p2, Point const & cp) : X1(p1.X), Y1(p1.Y), X2(p2.X), Y2(p2.Y), CX(cp.X), CY(cp.Y) { reset(); }

  void reset() {
    x = X1;
    y = Y1;
    deltaX = X2 - X1;
    deltaY = Y2 - Y1;
    absDeltaX = deltaX < 0 ? -deltaX : deltaX;
    absDeltaY = deltaY < 0 ? -deltaY : deltaY;
    // sx = deltaX < 0 ? -1 : 1;
    // sy = deltaY < 0 ? -1 : 1;
    sx = X1 < X2 ? 1 : -1;
    sy = Y1 <= Y2 ? 1 : -1;
    error = absDeltaX - absDeltaY;
    err = error;
    int16_t midX = (X1 + X2) / 2;
    int16_t midY = (Y1 + Y2) / 2;
    quadrant = getCircleQuadrant(midX - CX, midY - CY);
    newRowCheck(Y1);
  }

  void newRowCheck(int16_t _y) {
    minX = x;
    maxX = x;
    hasPixels = (_y >= Y1 && _y <= Y2);
  }

  void sortByY() {
    if (Y1 > Y2) {
      tswap(X1, X2);
      tswap(Y1, Y2);
    }
    reset();
  }

  /* Walk a distance using Bresenham's algorithm, and return a new line */
  LineInfo walkDistance(int16_t distance) {
    int32_t dSquared = distance * distance;
    auto dy = -absDeltaY;
    auto x = 0;
    auto y = 0;
    auto err = error;
    auto e2 = 2 * error;
    while ((x * x + y * y) < dSquared) {
      e2 = 2 * err;
      if (e2 >= dy) {
        err += dy;
        x += sx;
      }
      if (e2 <= absDeltaX) {
        err += absDeltaX;
        y += sy;
      }
    }
    return LineInfo(X1, Y1, X1 + x, Y1 + y, CX, CY);
  }

  void walkToY(int16_t newY) {
    // walks to specific y value - must be used in association with newRowCheck and sortByY
    // can only be used for incremetal y values, as this assumes we're walking downwards
    if (sy < 0) return;
    if (hasPixels && y <= newY) {
      minX = imin(minX, x);
      maxX = imax(maxX, x);
      while (y <= newY) {
        minX = imin(minX, x);
        maxX = imax(maxX, x);
        if (y == Y2 && x == X2) {
          // end of line reached
          break;
        }
        auto e2 = 2 * err;
        if (e2 >= -absDeltaY) {
          err -= absDeltaY;
          x += sx;
        }
        if (e2 <= absDeltaX) {
          err += absDeltaX;
          // y += sy;
          y += 1;
          if (y <= newY) {
            minX = x;
            maxX = x;
          }
        }
      }
    }
  }

  int length() {
    return isqrt(deltaX * deltaX + deltaY * deltaY);
  }
};

/**
 * @brief Quadrant info for arc, sector and segment drawing functions.
 * Used to determine which quadrants to draw in.
 */
struct QuadrantInfo {
  bool  containsStart;  /**< True if the start angle is in this quadrant */
  bool  containsEnd;    /**< True if the end angle is in this quadrant */
  bool  showAll;        /**< True if the all this quadrant should be drawn */
  bool  isEven;         /**< True if the quadrant is even (0 or 2) */
  bool  noArc;          /**< True if there are no arc points in this quadrant */
  bool  containsChord;  /**< True if the chord mid-point is in this quadrant */
  bool  showNothing;    /**< True if nothing should be drawn in this quadrant */
  bool  startCloserToHorizontal; /**< True if the start angle is closer to horizontal than the end angle, and whether start angle is larger */

  QuadrantInfo(uint8_t quadrant, LineInfo & startInfo, LineInfo & endInfo, uint8_t chordQuadrant = 9) {
    // work out all our values
    auto slopeTest = startInfo.absDeltaY * endInfo.absDeltaX;
    startCloserToHorizontal = slopeTest < (startInfo.absDeltaX * endInfo.absDeltaY);
    auto startQuadrant = startInfo.quadrant;
    auto chordMidpointInQuadrant = chordQuadrant == quadrant;

    containsStart = startQuadrant == quadrant;
    containsEnd = endInfo.quadrant == quadrant;

    // get our end quadrant, and tweak for anticlockwise, incremental values
    auto endQuadrant = endInfo.quadrant < startQuadrant ? endInfo.quadrant + 4 : endInfo.quadrant;
    if ((startCloserToHorizontal ^ !(startQuadrant & 1)) && startQuadrant == endQuadrant) {
      endQuadrant += 4;
    }
    if (quadrant < startQuadrant) {
      quadrant += 4;
    }

    showAll = quadrant > startQuadrant && quadrant < endQuadrant;
    isEven = !(quadrant & 1);
    noArc = (quadrant < startQuadrant) || (quadrant > endQuadrant);
    containsChord = chordMidpointInQuadrant || containsStart || containsEnd;
    showNothing = noArc && !containsChord;
  }
};


/**
 * @brief Work out whether our arc circumference pixel should be shown in this quadrant
 */
bool quadrantContainsArcPixel(QuadrantInfo & quadrant, LineInfo & start, LineInfo & end, int16_t x, int16_t y);

/**
 * @brief Represents an image
 */
struct Bitmap {
  int16_t         width;           /**< Bitmap horizontal size */
  int16_t         height;          /**< Bitmap vertical size */
  PixelFormat     format;          /**< Bitmap pixel format */
  RGB888          foregroundColor; /**< Foreground color when format is PixelFormat::Mask */
  uint8_t *       data;            /**< Bitmap binary data */
  bool            dataAllocated;   /**< If true data is released when bitmap is destroyed */

  Bitmap() : width(0), height(0), format(PixelFormat::Undefined), foregroundColor(RGB888(255, 255, 255)), data(nullptr), dataAllocated(false) { }
  Bitmap(int width_, int height_, void const * data_, PixelFormat format_, bool copy = false);
  Bitmap(int width_, int height_, void const * data_, PixelFormat format_, RGB888 foregroundColor_, bool copy = false);
  ~Bitmap();

  void setPixel(int x, int y, int value);       // use with PixelFormat::Mask. value can be 0 or not 0
  void setPixel(int x, int y, RGBA2222 value);  // use with PixelFormat::RGBA2222
  void setPixel(int x, int y, RGBA8888 value);  // use with PixelFormat::RGBA8888

  int getAlpha(int x, int y);

  RGBA2222 getPixel2222(int x, int y) const;
  RGBA8888 getPixel8888(int x, int y) const;

private:
  void allocate();
  void copyFrom(void const * srcData);
};


struct BitmapDrawingInfo {
  int16_t        X;
  int16_t        Y;
  Bitmap const * bitmap;

  BitmapDrawingInfo(int X_, int Y_, Bitmap const * bitmap_) : X(X_), Y(Y_), bitmap(bitmap_) { }
} __attribute__ ((packed));


struct BitmapTransformedDrawingInfo {
  int16_t        X;
  int16_t        Y;
  Bitmap const * bitmap;
  float const *  transformMatrix;
  float const *  transformInverse;
  bool           freeMatrix;

  BitmapTransformedDrawingInfo(int X_, int Y_, Bitmap const * bitmap_, float const * transformMatrix_, float const * transformInverse_) : 
    X(X_), Y(Y_), bitmap(bitmap_), transformMatrix(transformMatrix_), transformInverse(transformInverse_), freeMatrix(false) { }
} __attribute__ ((packed));


struct PaletteListItem {
  uint16_t          endRow;
  void *            signals;
  PaletteListItem * next;
};

} // fabgl
