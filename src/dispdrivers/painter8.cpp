/*
  Painter8 - Paints (draws) using 8 colors

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

#include "painter8.h"

#pragma GCC optimize ("O2")

namespace fabgl {

/*
To improve drawing and rendering speed pixels order is a bit oddy because we want to pack pixels (3 bits) into a uint32_t and ESP32 is little-endian.
8 pixels (24 bits) are packed in 3 bytes:
bytes:      0        1        2    ...
bits:   76543210 76543210 76543210 ...
pixels: 55666777 23334445 00011122 ...
bits24: 0                          1...
*/

static inline __attribute__((always_inline)) void VGA8_SETPIXELINROW(uint8_t * row, int x, int value) {
  uint32_t * bits24 = (uint32_t*)(row + (x >> 3) * 3);  // x / 8 * 3
  int shift = 21 - (x & 7) * 3;
  *bits24 ^= ((value << shift) ^ *bits24) & (7 << shift);
}

static inline __attribute__((always_inline)) int VGA8_GETPIXELINROW(uint8_t * row, int x) {
  uint32_t * bits24 = (uint32_t*)(row + (x >> 3) * 3);  // x / 8 * 3
  int shift = 21 - (x & 7) * 3;
  return (*bits24 >> shift) & 7;
}

#define VGA8_INVERTPIXELINROW(row, x)       *((uint32_t*)(row + ((x) >> 3) * 3)) ^= 7 << (21 - ((x) & 7) * 3)

static inline __attribute__((always_inline)) void VGA8_SETPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) Painter8::sgetScanline(y);
  uint32_t * bits24 = (uint32_t*)(row + (x >> 3) * 3);  // x / 8 * 3
  int shift = 21 - (x & 7) * 3;
  *bits24 ^= ((value << shift) ^ *bits24) & (7 << shift);
}

static inline __attribute__((always_inline)) void VGA8_ORPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) Painter8::sgetScanline(y);
  uint32_t * bits24 = (uint32_t*)(row + (x >> 3) * 3);  // x / 8 * 3
  int shift = 21 - (x & 7) * 3;
  *bits24 |= (value << shift);
}

static inline __attribute__((always_inline)) void VGA8_ANDPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) Painter8::sgetScanline(y);
  uint32_t * bits24 = (uint32_t*)(row + (x >> 3) * 3);  // x / 8 * 3
  int shift = 21 - (x & 7) * 3;
  value = (~0x00) ^ (7 << shift) | (value << shift);
  *bits24 &= value;
}

static inline __attribute__((always_inline)) void VGA8_XORPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) Painter8::sgetScanline(y);
  uint32_t * bits24 = (uint32_t*)(row + (x >> 3) * 3);  // x / 8 * 3
  int shift = 21 - (x & 7) * 3;
  *bits24 ^= (value << shift);
}

#define VGA8_GETPIXEL(x, y)                 VGA8_GETPIXELINROW((uint8_t*)Painter8::s_viewPort[(y)], (x))

#define VGA8_INVERT_PIXEL(x, y)             VGA8_INVERTPIXELINROW((uint8_t*)Painter8::s_viewPort[(y)], (x))

#define VGA8_COLUMNSQUANTUM 16

/*************************************************************************************/
/* Painter8 definitions */

Painter8::Painter8() {
}

Painter8::~Painter8() {
  Painter::~Painter();
}

std::function<uint8_t(RGB888 const &)> Painter8::getPixelLambda(PaintMode mode) {
  return [&] (RGB888 const & color) { return RGB888toPaletteIndex(color); };
}

std::function<void(int X, int Y, uint8_t colorIndex)> Painter8::setPixelLambda(PaintMode mode) {
  switch (mode) {
    case PaintMode::Set:
      return VGA8_SETPIXEL;
    case PaintMode::OR:
      return VGA8_ORPIXEL;
    case PaintMode::ORNOT:
      return [&] (int X, int Y, uint8_t colorIndex) { VGA8_ORPIXEL(X, Y, ~colorIndex & 0x07); };
    case PaintMode::AND:
      return VGA8_ANDPIXEL;
    case PaintMode::ANDNOT:
      return [&] (int X, int Y, uint8_t colorIndex) { VGA8_ANDPIXEL(X, Y, ~colorIndex); };
    case PaintMode::XOR:
      return VGA8_XORPIXEL;
    case PaintMode::Invert:
      return [&] (int X, int Y, uint8_t colorIndex) { VGA8_INVERT_PIXEL(X, Y); };
    default:  // PaintMode::NoOp
      return [&] (int X, int Y, uint8_t colorIndex) { return; };
  }
}

std::function<void(uint8_t * row, int x, uint8_t colorIndex)> Painter8::setRowPixelLambda(PaintMode mode) {
  switch (mode) {
    case PaintMode::Set:
      return VGA8_SETPIXELINROW;
    case PaintMode::OR:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) {
        VGA8_SETPIXELINROW(row, x, VGA8_GETPIXELINROW(row, x) | colorIndex);
      };
    case PaintMode::ORNOT:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) {
        VGA8_SETPIXELINROW(row, x, VGA8_GETPIXELINROW(row, x) | (~colorIndex & 0x07));
      };
    case PaintMode::AND:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) {
        VGA8_SETPIXELINROW(row, x, VGA8_GETPIXELINROW(row, x) & colorIndex);
      };
    case PaintMode::ANDNOT:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) {
        VGA8_SETPIXELINROW(row, x, VGA8_GETPIXELINROW(row, x) & ~colorIndex);
      };
    case PaintMode::XOR:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) {
        VGA8_SETPIXELINROW(row, x, VGA8_GETPIXELINROW(row, x) ^ colorIndex);
      };
    case PaintMode::Invert:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) { VGA8_INVERTPIXELINROW(row, x); };
    default:  // PaintMode::NoOp
      return [&] (uint8_t * row, int x, uint8_t colorIndex) { return; };
  }
}

std::function<void(int Y, int X1, int X2, uint8_t colorIndex)> Painter8::fillRowLambda(PaintMode mode) {
  switch (mode) {
    case PaintMode::Set:
      return [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawFillRow(Y, X1, X2, colorIndex); };
    case PaintMode::OR:
      return [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawORRow(Y, X1, X2, colorIndex); };
    case PaintMode::ORNOT:
      return [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawORRow(Y, X1, X2, ~colorIndex & 0x07); };
    case PaintMode::AND:
      return [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawANDRow(Y, X1, X2, colorIndex); };
    case PaintMode::ANDNOT:
      return [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawANDRow(Y, X1, X2, ~colorIndex); };
    case PaintMode::XOR:
      return [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawXORRow(Y, X1, X2, colorIndex); };
    case PaintMode::Invert:
      return [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawInvertRow(Y, X1, X2); };
    default:  // PaintMode::NoOp
      return [&] (int Y, int X1, int X2, uint8_t colorIndex) { return; };
  }
}

void Painter8::setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect) {
  auto paintMode = paintState().paintOptions.mode;
  genericSetPixelAt(pixelDesc, updateRect, getPixelLambda(paintMode), setPixelLambda(paintMode));
}

// coordinates are absolute values (not relative to origin)
// line clipped on current absolute clipping rectangle
void Painter8::absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color) {
  auto paintMode = paintState().paintOptions.NOT ? PaintMode::NOT : paintState().paintOptions.mode;
  genericAbsDrawLine(X1, Y1, X2, Y2, color,
                     getPixelLambda(paintMode),
                     fillRowLambda(paintMode),
                     setPixelLambda(paintMode)
                     );
}

// parameters not checked
void Painter8::fillRow(int y, int x1, int x2, RGB888 color) {
  // pick fill method based on paint mode
  auto paintMode = paintState().paintOptions.mode;
  auto getPixel = getPixelLambda(paintMode);
  auto pixel = getPixel(color);
  auto fill = fillRowLambda(paintMode);
  fill(y, x1, x2, pixel);
}

// parameters not checked
void Painter8::rawFillRow(int y, int x1, int x2, uint8_t colorIndex) {
  uint8_t * row = (uint8_t*) m_viewPort[y];
  for (; x1 <= x2; ++x1)
    VGA8_SETPIXELINROW(row, x1, colorIndex);
}

// parameters not checked
void Painter8::rawORRow(int y, int x1, int x2, uint8_t colorIndex) {
   // naive implementation - just iterate over all pixels
  for (int x = x1; x <= x2; ++x) {
    VGA8_ORPIXEL(x, y, colorIndex);
  }
}

// parameters not checked
void Painter8::rawANDRow(int y, int x1, int x2, uint8_t colorIndex) {
  // naive implementation - just iterate over all pixels
  for (int x = x1; x <= x2; ++x) {
    VGA8_ANDPIXEL(x, y, colorIndex);
  }
}

// parameters not checked
void Painter8::rawXORRow(int y, int x1, int x2, uint8_t colorIndex) {
  // naive implementation - just iterate over all pixels
  for (int x = x1; x <= x2; ++x) {
    VGA8_XORPIXEL(x, y, colorIndex);
  }
}

// parameters not checked
void Painter8::rawInvertRow(int y, int x1, int x2) {
  auto row = m_viewPort[y];
  for (int x = x1; x <= x2; ++x)
    VGA8_INVERTPIXELINROW(row, x);
}

void Painter8::rawCopyRow(int x1, int x2, int srcY, int dstY) {
  auto srcRow = (uint8_t*) m_viewPort[srcY];
  auto dstRow = (uint8_t*) m_viewPort[dstY];
  for (; x1 <= x2; ++x1)
    VGA8_SETPIXELINROW(dstRow, x1, VGA8_GETPIXELINROW(srcRow, x1));
}

void Painter8::swapRows(int yA, int yB, int x1, int x2) {
  auto rowA = (uint8_t*) m_viewPort[yA];
  auto rowB = (uint8_t*) m_viewPort[yB];
  for (; x1 <= x2; ++x1) {
    uint8_t a = VGA8_GETPIXELINROW(rowA, x1);
    uint8_t b = VGA8_GETPIXELINROW(rowB, x1);
    VGA8_SETPIXELINROW(rowA, x1, b);
    VGA8_SETPIXELINROW(rowB, x1, a);
  }
}

void Painter8::drawEllipse(Size const & size, Rect & updateRect) {
  auto mode = paintState().paintOptions.mode;
  genericDrawEllipse(size, updateRect, getPixelLambda(mode), setPixelLambda(mode));
}

void Painter8::drawArc(Rect const & rect, Rect & updateRect) {
  auto mode = paintState().paintOptions.mode;
  genericDrawArc(rect, updateRect, getPixelLambda(mode), setPixelLambda(mode));
}

void Painter8::fillSegment(Rect const & rect, Rect & updateRect) {
  auto mode = paintState().paintOptions.mode;
  genericFillSegment(rect, updateRect, getPixelLambda(mode), fillRowLambda(mode));
}

void Painter8::fillSector(Rect const & rect, Rect & updateRect) {
  auto mode = paintState().paintOptions.mode;
  genericFillSector(rect, updateRect, getPixelLambda(mode), fillRowLambda(mode));
}

void Painter8::clear(Rect & updateRect) {
  hideSprites(updateRect);
  uint8_t paletteIndex = RGB888toPaletteIndex(getActualBrushColor());
  uint32_t pattern8 = (paletteIndex) | (paletteIndex << 3) | (paletteIndex << 6) | (paletteIndex << 9) | (paletteIndex << 12) | (paletteIndex << 15) | (paletteIndex << 18) | (paletteIndex << 21);
  for (int y = 0; y < m_viewPortHeight; ++y) {
    auto dest = (uint8_t*) m_viewPort[y];
    for (int x = 0; x < m_viewPortWidth; x += 8, dest += 3)
      *((uint32_t*)dest) = (*((uint32_t*)dest) & 0xFF000000) | pattern8;
  }
}

// scroll < 0 -> scroll UP
// scroll > 0 -> scroll DOWN
void Painter8::VScroll(int scroll, Rect & updateRect) {
  genericVScroll(scroll, updateRect,
                 [&] (int yA, int yB, int x1, int x2)        { swapRows(yA, yB, x1, x2); },              // swapRowsCopying
                 [&] (int yA, int yB)                        { tswap(m_viewPort[yA], m_viewPort[yB]); }, // swapRowsPointers
                 [&] (int y, int x1, int x2, RGB888 color)   { rawFillRow(y, x1, x2, RGB888toPaletteIndex(color)); }  // rawFillRow
                );
}

// todo: optimize!
void Painter8::HScroll(int scroll, Rect & updateRect) {
  hideSprites(updateRect);
  uint8_t back = RGB888toPaletteIndex(getActualBrushColor());

  int Y1 = paintState().scrollingRegion.Y1;
  int Y2 = paintState().scrollingRegion.Y2;
  int X1 = paintState().scrollingRegion.X1;
  int X2 = paintState().scrollingRegion.X2;

  int width = X2 - X1 + 1;
  bool HScrolllingRegionAligned = ((X1 & 7) == 0 && (width & 7) == 0);  // 8 pixels aligned

  if (scroll < 0) {
    // scroll left
    for (int y = Y1; y <= Y2; ++y) {
      for (int s = -scroll; s > 0;) {
        if (HScrolllingRegionAligned && s >= 8) {
          // scroll left by multiplies of 8
          uint8_t * row = (uint8_t*) (m_viewPort[y]) + X1 / 8 * 3;
          auto sc = s & ~7;
          auto sz = width & ~7;
          memmove(row, row + sc / 8 * 3, (sz - sc) / 8 * 3);
          rawFillRow(y, X2 - sc + 1, X2, back);
          s -= sc;
        } else {
          // unaligned horizontal scrolling region, fallback to slow version
          auto row = (uint8_t*) m_viewPort[y];
          for (int x = X1; x <= X2 - s; ++x)
            VGA8_SETPIXELINROW(row, x, VGA8_GETPIXELINROW(row, x + s));
          // fill right area with brush color
          rawFillRow(y, X2 - s + 1, X2, back);
          s = 0;
        }
      }
    }
  } else if (scroll > 0) {
    // scroll right
    for (int y = Y1; y <= Y2; ++y) {
      for (int s = scroll; s > 0;) {
        if (HScrolllingRegionAligned && s >= 8) {
          // scroll right by multiplies of 8
          uint8_t * row = (uint8_t*) (m_viewPort[y]) + X1 / 8 * 3;
          auto sc = s & ~7;
          auto sz = width & ~7;
          memmove(row + sc / 8 * 3, row, (sz - sc) / 8 * 3);
          rawFillRow(y, X1, X1 + sc - 1, back);
          s -= sc;
        } else {
          // unaligned horizontal scrolling region, fallback to slow version
          auto row = (uint8_t*) m_viewPort[y];
          for (int x = X2 - s; x >= X1; --x)
            VGA8_SETPIXELINROW(row, x + s, VGA8_GETPIXELINROW(row, x));
          // fill left area with brush color
          rawFillRow(y, X1, X1 + s - 1, back);
          s = 0;
        }
      }
    }
  }
}

void Painter8::drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect) {
  auto mode = paintState().paintOptions.mode;
  auto getPixel = getPixelLambda(mode);
  auto setRowPixel = setRowPixelLambda(mode);
  genericDrawGlyph(glyph, glyphOptions, penColor, brushColor, updateRect,
                   getPixel,
                   [&] (int y) { return (uint8_t*) m_viewPort[y]; },
                   setRowPixel
                  );
}

void Painter8::invertRect(Rect const & rect, Rect & updateRect) {
  genericInvertRect(rect, updateRect,
                    [&] (int Y, int X1, int X2) { rawInvertRow(Y, X1, X2); }
                   );
}

void Painter8::swapFGBG(Rect const & rect, Rect & updateRect) {
  genericSwapFGBG(rect, updateRect,
                  [&] (RGB888 const & color)                     { return RGB888toPaletteIndex(color); },
                  [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                  VGA8_GETPIXELINROW,
                  VGA8_SETPIXELINROW
                 );
}

// Slow operation!
// supports overlapping of source and dest rectangles
void Painter8::copyRect(Rect const & source, Rect & updateRect) {
  genericCopyRect(source, updateRect,
                  [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                  VGA8_GETPIXELINROW,
                  VGA8_SETPIXELINROW
                 );
}

// no bounds check is done!
void Painter8::readScreen(Rect const & rect, RGB888 * destBuf) {
  for (int y = rect.Y1; y <= rect.Y2; ++y) {
    auto row = (uint8_t*) m_viewPort[y];
    for (int x = rect.X1; x <= rect.X2; ++x, ++destBuf) {
      const RGB222 v = m_palette[VGA8_GETPIXELINROW(row, x)];
      *destBuf = RGB888(v.R * 85, v.G * 85, v.B * 85);  // 85 x 3 = 255
    }
  }
}

void Painter8::rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount) {
  genericRawDrawBitmap_Native(destX, destY, (uint8_t*) bitmap->data, bitmap->width, X1, Y1, XCount, YCount,
                              [&] (int y) { return (uint8_t*) m_viewPort[y]; },   // rawGetRow
                              VGA8_SETPIXELINROW
                             );
}

void Painter8::rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);
  auto foregroundColorIndex = RGB888toPaletteIndex(paintState().paintOptions.swapFGBG ? paintState().penColor : bitmap->foregroundColor);
  genericRawDrawBitmap_Mask(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                            [&] (int y)                  { return (uint8_t*) m_viewPort[y]; },          // rawGetRow
                            VGA8_GETPIXELINROW,
                            [&] (uint8_t * row, int x)   { setRowPixel(row, x, foregroundColorIndex); } // rawSetPixelInRow
                           );
}

void Painter8::rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);

  if (paintState().paintOptions.swapFGBG) {
    // used for bitmap plots to indicate drawing with BG color instead of bitmap color
    auto bg = RGB888toPaletteIndex(paintState().penColor);
    genericRawDrawBitmap_RGBA2222(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                  [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                  VGA8_GETPIXELINROW,                                                            // rawGetPixelInRow
                                  [&] (uint8_t * row, int x, uint8_t src) { setRowPixel(row, x, bg); }           // rawSetPixelInRow
                                );
    return;
  }

  genericRawDrawBitmap_RGBA2222(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },                 // rawGetRow
                                VGA8_GETPIXELINROW,
                                [&] (uint8_t * row, int x, uint8_t src) { setRowPixel(row, x, RGB2222toPaletteIndex(src)); }  // rawSetPixelInRow
                               );
}

void Painter8::rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);

  if (paintState().paintOptions.swapFGBG) {
    // used for bitmap plots to indicate drawing with BG color instead of bitmap color
    auto bg = RGB888toPaletteIndex(paintState().penColor);
    genericRawDrawBitmap_RGBA8888(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                  [&] (int y)                                      { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                  VGA8_GETPIXELINROW,                                                                     // rawGetPixelInRow
                                  [&] (uint8_t * row, int x, RGBA8888 const & src) { setRowPixel(row, x, bg); }           // rawSetPixelInRow
                                  );
    return;
  }

  genericRawDrawBitmap_RGBA8888(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                 [&] (int y)                                      { return (uint8_t*) m_viewPort[y]; },                 // rawGetRow
                                 VGA8_GETPIXELINROW,                                                                                    // rawGetPixelInRow
                                 [&] (uint8_t * row, int x, RGBA8888 const & src) { setRowPixel(row, x, RGB8888toPaletteIndex(src)); }  // rawSetPixelInRow
                                );
}

void Painter8::rawCopyToBitmap(int srcX, int srcY, int width, void * saveBuffer, int X1, int Y1, int XCount, int YCount) {
  genericRawCopyToBitmap(srcX, srcY, width, (uint8_t*)saveBuffer, X1, Y1, XCount, YCount,
                        [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                        [&] (uint8_t * row, int x) {
                          auto rgb = m_palette[VGA8_GETPIXELINROW(row, x)];
                          return (0xC0 | (rgb.B << VGA_BLUE_BIT) | (rgb.G << VGA_GREEN_BIT) | (rgb.R << VGA_RED_BIT));
                        } // rawGetPixelInRow
                      );
}

void Painter8::rawDrawBitmapWithMatrix_Mask(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);
  auto foregroundColorIndex = RGB888toPaletteIndex(paintState().paintOptions.swapFGBG ? paintState().penColor : bitmap->foregroundColor);
  genericRawDrawTransformedBitmap_Mask(destX, destY, drawingRect, bitmap, invMatrix,
                                          [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                          // VGA8_GETPIXELINROW
                                          [&] (uint8_t * row, int x) { setRowPixel(row, x, foregroundColorIndex); }  // rawSetPixelInRow
                                         );
}

void Painter8::rawDrawBitmapWithMatrix_RGBA2222(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);

  if (paintState().paintOptions.swapFGBG) {
    // used for bitmap plots to indicate drawing with BG color instead of bitmap color
    auto bg = RGB888toPaletteIndex(paintState().penColor);
    genericRawDrawTransformedBitmap_RGBA2222(destX, destY, drawingRect, bitmap, invMatrix,
                                              [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                              // VGA8_GETPIXELINROW
                                              [&] (uint8_t * row, int x, uint8_t src) { setRowPixel(row, x, bg); }  // rawSetPixelInRow
                                             );
    return;
  }

  genericRawDrawTransformedBitmap_RGBA2222(destX, destY, drawingRect, bitmap, invMatrix,
                                          [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                          // VGA8_GETPIXELINROW
                                          [&] (uint8_t * row, int x, uint8_t src) { setRowPixel(row, x, RGB2222toPaletteIndex(src)); }  // rawSetPixelInRow
                                         );
}

void Painter8::rawDrawBitmapWithMatrix_RGBA8888(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);

  if (paintState().paintOptions.swapFGBG) {
    // used for bitmap plots to indicate drawing with BG color instead of bitmap color
    auto bg = RGB888toPaletteIndex(paintState().penColor);
    genericRawDrawTransformedBitmap_RGBA8888(destX, destY, drawingRect, bitmap, invMatrix,
                                            [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                            // VGA8_GETPIXELINROW,                                                                     // rawGetPixelInRow
                                            [&] (uint8_t * row, int x, RGBA8888 const & src) { setRowPixel(row, x, bg); }           // rawSetPixelInRow
                                          );
    return;
  }

  genericRawDrawTransformedBitmap_RGBA8888(destX, destY, drawingRect, bitmap, invMatrix,
                                          [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                          // VGA8_GETPIXELINROW,                                                                      // rawGetPixelInRow
                                          [&] (uint8_t * row, int x, RGBA8888 const & src) { setRowPixel(row, x, RGB8888toPaletteIndex(src)); }   // rawSetPixelInRow
                                         );
}

void Painter8::directSetPixel(int x, int y, int value) {
  VGA8_SETPIXEL(x, y, value);
}

} // end of namespace
