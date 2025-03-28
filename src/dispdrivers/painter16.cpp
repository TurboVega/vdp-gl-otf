/*
  Painter16 - Paints (draws) using 16 colors

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

#include "painter16.h"

#pragma GCC optimize ("O2")

namespace fabgl {

// high nibble is pixel 0, low nibble is pixel 1

static inline __attribute__((always_inline)) void VGA16_SETPIXELINROW(uint8_t * row, int x, int value) {
  int brow = x >> 1;
  row[brow] = (x & 1) ? ((row[brow] & 0xf0) | value) : ((row[brow] & 0x0f) | (value << 4));
}

static inline __attribute__((always_inline)) int VGA16_GETPIXELINROW(uint8_t * row, int x) {
  int brow = x >> 1;
  return ((x & 1) ? (row[brow] & 0x0f) : ((row[brow] & 0xf0) >> 4));
}

#define VGA16_INVERTPIXELINROW(row, x)       (row)[(x) >> 1] ^= (0xf0 >> (((x) & 1) << 2))

static inline __attribute__((always_inline)) void VGA16_SETPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) Painter16::sgetScanline(y);
  int brow = x >> 1;
  row[brow] = (x & 1) ? ((row[brow] & 0xf0) | value) : ((row[brow] & 0x0f) | (value << 4));
}

static inline __attribute__((always_inline)) void VGA16_ORPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) Painter16::sgetScanline(y);
  int brow = x >> 1;
  row[brow] |= (x & 1) ? value : (value << 4);
}

static inline __attribute__((always_inline)) void VGA16_ANDPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) Painter16::sgetScanline(y);
  int brow = x >> 1;
  row[brow] &= (x & 1) ? (value | 0xF0) : ((value << 4) | 0x0F);
}

static inline __attribute__((always_inline)) void VGA16_XORPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) Painter16::sgetScanline(y);
  int brow = x >> 1;
  row[brow] ^= (x & 1) ? value : (value << 4);
}

#define VGA16_GETPIXEL(x, y)                 VGA16_GETPIXELINROW((uint8_t*)Painter16::s_viewPort[(y)], (x))

#define VGA16_INVERT_PIXEL(x, y)             VGA16_INVERTPIXELINROW((uint8_t*)Painter16::s_viewPort[(y)], (x))

#define VGA16_COLUMNSQUANTUM 16

/*************************************************************************************/
/* Painter16 definitions */

Painter16 * Painter16::s_instance = nullptr;

Painter16::Painter16() {
  postConstruct();
}

Painter16::~Painter16() {
  Painter::~Painter();
}

std::function<uint8_t(RGB888 const &)> Painter16::getPixelLambda(PaintMode mode) {
  return [&] (RGB888 const & color) { return RGB888toPaletteIndex(color); };
}

std::function<void(int X, int Y, uint8_t colorIndex)> Painter16::setPixelLambda(PaintMode mode) {
  switch (mode) {
    case PaintMode::Set:
      return VGA16_SETPIXEL;
    case PaintMode::OR:
      return VGA16_ORPIXEL;
    case PaintMode::ORNOT:
      return [&] (int X, int Y, uint8_t colorIndex) { VGA16_ORPIXEL(X, Y, ~colorIndex & 0x0F); };
    case PaintMode::AND:
      return VGA16_ANDPIXEL;
    case PaintMode::ANDNOT:
      return [&] (int X, int Y, uint8_t colorIndex) { VGA16_ANDPIXEL(X, Y, ~colorIndex); };
    case PaintMode::XOR:
      return VGA16_XORPIXEL;
    case PaintMode::Invert:
      return [&] (int X, int Y, uint8_t colorIndex) { VGA16_INVERT_PIXEL(X, Y); };
    default:  // PaintMode::NoOp
      return [&] (int X, int Y, uint8_t colorIndex) { return; };
  }
}

std::function<void(uint8_t * row, int x, uint8_t colorIndex)> Painter16::setRowPixelLambda(PaintMode mode) {
  switch (mode) {
    case PaintMode::Set:
      return VGA16_SETPIXELINROW;
    case PaintMode::OR:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) {
        VGA16_SETPIXELINROW(row, x, VGA16_GETPIXELINROW(row, x) | colorIndex);
      };
    case PaintMode::ORNOT:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) {
        VGA16_SETPIXELINROW(row, x, VGA16_GETPIXELINROW(row, x) | (~colorIndex & 0x0F));
      };
    case PaintMode::AND:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) {
        VGA16_SETPIXELINROW(row, x, VGA16_GETPIXELINROW(row, x) & colorIndex);
      };
    case PaintMode::ANDNOT:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) {
        VGA16_SETPIXELINROW(row, x, VGA16_GETPIXELINROW(row, x) & ~colorIndex);
      };
    case PaintMode::XOR:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) {
        VGA16_SETPIXELINROW(row, x, VGA16_GETPIXELINROW(row, x) ^ colorIndex);
      };
    case PaintMode::Invert:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) { VGA16_INVERTPIXELINROW(row, x); };
    default:  // PaintMode::NoOp
      return [&] (uint8_t * row, int x, uint8_t colorIndex) { return; };
  }
}

std::function<void(int Y, int X1, int X2, uint8_t colorIndex)> Painter16::fillRowLambda(PaintMode mode) {
  switch (mode) {
    case PaintMode::Set:
      return [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawFillRow(Y, X1, X2, colorIndex); };
    case PaintMode::OR:
      return [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawORRow(Y, X1, X2, colorIndex); };
    case PaintMode::ORNOT:
      return [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawORRow(Y, X1, X2, ~colorIndex & 0x0F); };
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

void Painter16::setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect) {
  auto paintMode = paintState().paintOptions.mode;
  genericSetPixelAt(pixelDesc, updateRect, getPixelLambda(paintMode), setPixelLambda(paintMode));
}

// coordinates are absolute values (not relative to origin)
// line clipped on current absolute clipping rectangle
void Painter16::absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color) {
  auto paintMode = paintState().paintOptions.NOT ? PaintMode::NOT : paintState().paintOptions.mode;
  genericAbsDrawLine(X1, Y1, X2, Y2, color,
                     getPixelLambda(paintMode),
                     fillRowLambda(paintMode),
                     setPixelLambda(paintMode)
                     );
}

// parameters not checked
void Painter16::fillRow(int y, int x1, int x2, RGB888 color) {
  // pick fill method based on paint mode
  auto paintMode = paintState().paintOptions.mode;
  auto getPixel = getPixelLambda(paintMode);
  auto pixel = getPixel(color);
  auto fill = fillRowLambda(paintMode);
  fill(y, x1, x2, pixel);
}

// parameters not checked
void Painter16::rawFillRow(int y, int x1, int x2, uint8_t colorIndex) {
  uint8_t * row = (uint8_t*) m_viewPort[y];
  // fill first pixels before full 16 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    VGA16_SETPIXELINROW(row, x, colorIndex);
  }
  // fill whole 16 bits words (4 pixels)
  if (x <= x2) {
    int sz = (x2 & ~3) - x;
    memset((void*)(row + x / 2), colorIndex | (colorIndex << 4), sz / 2);
    x += sz;
  }
  // fill last unaligned pixels
  for (; x <= x2; ++x) {
    VGA16_SETPIXELINROW(row, x, colorIndex);
  }
}

// parameters not checked
void Painter16::rawORRow(int y, int x1, int x2, uint8_t colorIndex) {
  // naive implementation - just iterate over all pixels
  for (int x = x1; x <= x2; ++x) {
    VGA16_ORPIXEL(x, y, colorIndex);
  }
}

// parameters not checked
void Painter16::rawANDRow(int y, int x1, int x2, uint8_t colorIndex) {
  // naive implementation - just iterate over all pixels
  for (int x = x1; x <= x2; ++x) {
    VGA16_ANDPIXEL(x, y, colorIndex);
  }
}

// parameters not checked
void Painter16::rawXORRow(int y, int x1, int x2, uint8_t colorIndex) {
  // naive implementation - just iterate over all pixels
  for (int x = x1; x <= x2; ++x) {
    VGA16_XORPIXEL(x, y, colorIndex);
  }
}

// parameters not checked
void Painter16::rawInvertRow(int y, int x1, int x2) {
  auto row = m_viewPort[y];
  for (int x = x1; x <= x2; ++x)
    VGA16_INVERTPIXELINROW(row, x);
}

void Painter16::rawCopyRow(int x1, int x2, int srcY, int dstY) {
  auto srcRow = (uint8_t*) m_viewPort[srcY];
  auto dstRow = (uint8_t*) m_viewPort[dstY];
  // copy first pixels before full 16 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    VGA16_SETPIXELINROW(dstRow, x, VGA16_GETPIXELINROW(srcRow, x));
  }
  // copy whole 16 bits words (4 pixels)
  auto src = (uint16_t*)(srcRow + x / 2);
  auto dst = (uint16_t*)(dstRow + x / 2);
  for (int right = (x2 & ~3); x < right; x += 4)
    *dst++ = *src++;
  // copy last unaligned pixels
  for (x = (x2 & ~3); x <= x2; ++x) {
    VGA16_SETPIXELINROW(dstRow, x, VGA16_GETPIXELINROW(srcRow, x));
  }
}

void Painter16::swapRows(int yA, int yB, int x1, int x2) {
  auto rowA = (uint8_t*) m_viewPort[yA];
  auto rowB = (uint8_t*) m_viewPort[yB];
  // swap first pixels before full 16 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    uint8_t a = VGA16_GETPIXELINROW(rowA, x);
    uint8_t b = VGA16_GETPIXELINROW(rowB, x);
    VGA16_SETPIXELINROW(rowA, x, b);
    VGA16_SETPIXELINROW(rowB, x, a);
  }
  // swap whole 16 bits words (4 pixels)
  auto a = (uint16_t*)(rowA + x / 2);
  auto b = (uint16_t*)(rowB + x / 2);
  for (int right = (x2 & ~3); x < right; x += 4)
    tswap(*a++, *b++);
  // swap last unaligned pixels
  for (x = (x2 & ~3); x <= x2; ++x) {
    uint8_t a = VGA16_GETPIXELINROW(rowA, x);
    uint8_t b = VGA16_GETPIXELINROW(rowB, x);
    VGA16_SETPIXELINROW(rowA, x, b);
    VGA16_SETPIXELINROW(rowB, x, a);
  }
}

void Painter16::drawEllipse(Size const & size, Rect & updateRect) {
  auto mode = paintState().paintOptions.mode;
  genericDrawEllipse(size, updateRect, getPixelLambda(mode), setPixelLambda(mode));
}

void Painter16::drawArc(Rect const & rect, Rect & updateRect) {
  auto mode = paintState().paintOptions.mode;
  genericDrawArc(rect, updateRect, getPixelLambda(mode), setPixelLambda(mode));
}

void Painter16::fillSegment(Rect const & rect, Rect & updateRect) {
  auto mode = paintState().paintOptions.mode;
  genericFillSegment(rect, updateRect, getPixelLambda(mode), fillRowLambda(mode));
}

void Painter16::fillSector(Rect const & rect, Rect & updateRect) {
  auto mode = paintState().paintOptions.mode;
  genericFillSector(rect, updateRect, getPixelLambda(mode), fillRowLambda(mode));
}

void Painter16::clear(Rect & updateRect) {
  uint8_t paletteIndex = RGB888toPaletteIndex(getActualBrushColor());
  uint8_t pattern = paletteIndex | (paletteIndex << 4);
  for (int y = 0; y < m_viewPortHeight; ++y)
    memset((uint8_t*) m_viewPort[y], pattern, m_viewPortWidth / 2);
}

// scroll < 0 -> scroll UP
// scroll > 0 -> scroll DOWN
void Painter16::VScroll(int scroll, Rect & updateRect) {
  genericVScroll(scroll, updateRect,
                 [&] (int yA, int yB, int x1, int x2)        { swapRows(yA, yB, x1, x2); },              // swapRowsCopying
                 [&] (int yA, int yB)                        { tswap(m_viewPort[yA], m_viewPort[yB]); }, // swapRowsPointers
                 [&] (int y, int x1, int x2, RGB888 color)   { rawFillRow(y, x1, x2, RGB888toPaletteIndex(color)); }  // rawFillRow
                );
}

void Painter16::HScroll(int scroll, Rect & updateRect) {
  uint8_t back4 = RGB888toPaletteIndex(getActualBrushColor());

  int Y1 = paintState().scrollingRegion.Y1;
  int Y2 = paintState().scrollingRegion.Y2;
  int X1 = paintState().scrollingRegion.X1;
  int X2 = paintState().scrollingRegion.X2;

  int width = X2 - X1 + 1;
  bool HScrolllingRegionAligned = ((X1 & 3) == 0 && (width & 3) == 0);  // 4 pixels aligned

  if (scroll < 0) {
    // scroll left
    for (int y = Y1; y <= Y2; ++y) {
      if (HScrolllingRegionAligned) {
        // aligned horizontal scrolling region, fast version
        uint8_t * row = (uint8_t*) (m_viewPort[y]) + X1 / 2;
        for (int s = -scroll; s > 0;) {
          if (s > 1) {
            // scroll left by 2, 4, 6, ... moving bytes
            auto sc = s & ~1;
            auto sz = width & ~1;
            memmove(row, row + sc / 2, (sz - sc) / 2);
            rawFillRow(y, X2 - sc + 1, X2, back4);
            s -= sc;
          } else if (s & 1) {
            // scroll left 1 pixel (uint16_t at the time = 4 pixels)
            // nibbles 0,1,2...  P is prev or background
            // byte                     : 01 23 45 67 -> 12 34 56 7P
            // word (little endian CPU) : 2301 6745  ->  3412 7P56
            auto prev = back4;
            auto w = (uint16_t *) (row + width / 2) - 1;
            for (int i = 0; i < width; i += 4) {
              const uint16_t p4 = *w; // four pixels
              *w-- = (p4 << 4 & 0xf000) | (prev << 8 & 0x0f00) | (p4 << 4 & 0x00f0) | (p4 >> 12 & 0x000f);
              prev = p4 >> 4 & 0x000f;
            }
            --s;
          }
        }
      } else {
        // unaligned horizontal scrolling region, fallback to slow version
        auto row = (uint8_t*) m_viewPort[y];
        for (int x = X1; x <= X2 + scroll; ++x)
          VGA16_SETPIXELINROW(row, x, VGA16_GETPIXELINROW(row, x - scroll));
        // fill right area with brush color
        rawFillRow(y, X2 + 1 + scroll, X2, back4);
      }
    }
  } else if (scroll > 0) {
    // scroll right
    for (int y = Y1; y <= Y2; ++y) {
      if (HScrolllingRegionAligned) {
        // aligned horizontal scrolling region, fast version
        uint8_t * row = (uint8_t*) (m_viewPort[y]) + X1 / 2;
        for (int s = scroll; s > 0;) {
          if (s > 1) {
            // scroll right by 2, 4, 6, ... moving bytes
            auto sc = s & ~1;
            auto sz = width & ~1;
            memmove(row + sc / 2, row, (sz - sc) / 2);
            rawFillRow(y, X1, X1 + sc - 1, back4);
            s -= sc;
          } else if (s & 1) {
            // scroll right 1 pixel (uint16_t at the time = 4 pixels)
            // nibbles 0,1,2...  P is prev or background
            // byte                     : 01 23 45 67 -> P0 12 34 56 7...
            // word (little endian CPU) : 2301 6745  ->  12P0 5634 ...
            auto prev = back4;
            auto w = (uint16_t *) row;
            for (int i = 0; i < width; i += 4) {
              const uint16_t p4 = *w; // four pixels
              *w++ = (p4 << 12 & 0xf000) | (p4 >> 4 & 0x0f00) | (prev << 4) | (p4 >> 4 & 0x000f);
              prev = p4 >> 8 & 0x000f;
            }
            --s;
          }
        }
      } else {
        // unaligned horizontal scrolling region, fallback to slow version
        auto row = (uint8_t*) m_viewPort[y];
        for (int x = X2 - scroll; x >= X1; --x)
          VGA16_SETPIXELINROW(row, x + scroll, VGA16_GETPIXELINROW(row, x));
        // fill left area with brush color
        rawFillRow(y, X1, X1 + scroll - 1, back4);
      }
    }

  }
}

void Painter16::drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect) {
  auto mode = paintState().paintOptions.mode;
  auto getPixel = getPixelLambda(mode);
  auto setRowPixel = setRowPixelLambda(mode);
  genericDrawGlyph(glyph, glyphOptions, penColor, brushColor, updateRect,
                   getPixel,
                   [&] (int y) { return (uint8_t*) m_viewPort[y]; },
                   setRowPixel
                  );
}

void Painter16::invertRect(Rect const & rect, Rect & updateRect) {
  genericInvertRect(rect, updateRect,
                    [&] (int Y, int X1, int X2) { rawInvertRow(Y, X1, X2); }
                   );
}

void Painter16::swapFGBG(Rect const & rect, Rect & updateRect) {
  genericSwapFGBG(rect, updateRect,
                  [&] (RGB888 const & color)                     { return RGB888toPaletteIndex(color); },
                  [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                  VGA16_GETPIXELINROW,
                  VGA16_SETPIXELINROW
                 );
}

// Slow operation!
// supports overlapping of source and dest rectangles
void Painter16::copyRect(Rect const & source, Rect & updateRect) {
  genericCopyRect(source, updateRect,
                  [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                  VGA16_GETPIXELINROW,
                  VGA16_SETPIXELINROW
                 );
}

void Painter16::readScreen(Rect const & rect, RGB222 * destBuf) {
}

// no bounds check is done!
void Painter16::readScreen(Rect const & rect, RGB888 * destBuf) {
  for (int y = rect.Y1; y <= rect.Y2; ++y) {
    auto row = (uint8_t*) m_viewPort[y];
    for (int x = rect.X1; x <= rect.X2; ++x, ++destBuf) {
      const RGB222 v = m_palette[VGA16_GETPIXELINROW(row, x)];
      *destBuf = RGB888(v.R * 85, v.G * 85, v.B * 85);  // 85 x 3 = 255
    }
  }
}

void Painter16::writeScreen(Rect const & rect, RGB888 * destBuf) {
}

void Painter16::writeScreen(Rect const & rect, RGB222 * destBuf) {
}

void Painter16::rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount) {
  genericRawDrawBitmap_Native(destX, destY, (uint8_t*) bitmap->data, bitmap->width, X1, Y1, XCount, YCount,
                              [&] (int y) { return (uint8_t*) m_viewPort[y]; },   // rawGetRow
                              VGA16_SETPIXELINROW
                             );
}

void Painter16::rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);
  auto foregroundColorIndex = RGB888toPaletteIndex(paintState().paintOptions.swapFGBG ? paintState().penColor : bitmap->foregroundColor);
  genericRawDrawBitmap_Mask(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                            [&] (int y)                  { return (uint8_t*) m_viewPort[y]; },           // rawGetRow
                            VGA16_GETPIXELINROW,
                            [&] (uint8_t * row, int x)   { setRowPixel(row, x, foregroundColorIndex); }  // rawSetPixelInRow
                           );
}

void Painter16::rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);

  if (paintState().paintOptions.swapFGBG) {
    // used for bitmap plots to indicate drawing with BG color instead of bitmap color
    auto bg = RGB888toPaletteIndex(paintState().penColor);
    genericRawDrawBitmap_RGBA2222(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                  [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                  VGA16_GETPIXELINROW,                                                           // rawGetPixelInRow
                                  [&] (uint8_t * row, int x, uint8_t src) { setRowPixel(row, x, bg); }           // rawSetPixelInRow
                                );
    return;
  }

  genericRawDrawBitmap_RGBA2222(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },              // rawGetRow
                                VGA16_GETPIXELINROW,                                                                       // rawGetPixelInRow
                                [&] (uint8_t * row, int x, uint8_t src) { setRowPixel(row, x, RGB2222toPaletteIndex(src)); }  // rawSetPixelInRow
                               );
}

void Painter16::rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);

  if (paintState().paintOptions.swapFGBG) {
    // used for bitmap plots to indicate drawing with BG color instead of bitmap color
    auto bg = RGB888toPaletteIndex(paintState().penColor);
    genericRawDrawBitmap_RGBA8888(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                  [&] (int y)                                      { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                  VGA16_GETPIXELINROW,                                                                    // rawGetPixelInRow
                                  [&] (uint8_t * row, int x, RGBA8888 const & src) { setRowPixel(row, x, bg); }           // rawSetPixelInRow
                                  );
    return;
  }

  genericRawDrawBitmap_RGBA8888(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                 [&] (int y)                                      { return (uint8_t*) m_viewPort[y]; },     // rawGetRow
                                 VGA16_GETPIXELINROW,                                                                       // rawGetPixelInRow
                                 [&] (uint8_t * row, int x, RGBA8888 const & src) { setRowPixel(row, x, RGB8888toPaletteIndex(src)); }   // rawSetPixelInRow
                                );
}

void Painter16::rawCopyToBitmap(int srcX, int srcY, int width, void * saveBuffer, int X1, int Y1, int XCount, int YCount) {
  genericRawCopyToBitmap(srcX, srcY, width, (uint8_t*)saveBuffer, X1, Y1, XCount, YCount,
                         [&] (int y)                { return (uint8_t*) m_viewPort[y]; },     // rawGetRow
                         [&] (uint8_t * row, int x) {
                          auto rgb = m_palette[VGA16_GETPIXELINROW(row, x)];
                          return (0xC0 | (rgb.B << VGA_BLUE_BIT) | (rgb.G << VGA_GREEN_BIT) | (rgb.R << VGA_RED_BIT));
                         }   // rawGetPixelInRow
                      );
}

void Painter16::rawDrawBitmapWithMatrix_Mask(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);
  auto foregroundColorIndex = RGB888toPaletteIndex(paintState().paintOptions.swapFGBG ? paintState().penColor : bitmap->foregroundColor);
  genericRawDrawTransformedBitmap_Mask(destX, destY, drawingRect, bitmap, invMatrix,
                                          [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                          // VGA16_GETPIXELINROW
                                          [&] (uint8_t * row, int x) { setRowPixel(row, x, foregroundColorIndex); }  // rawSetPixelInRow
                                         );
}

void Painter16::rawDrawBitmapWithMatrix_RGBA2222(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);

  if (paintState().paintOptions.swapFGBG) {
    // used for bitmap plots to indicate drawing with BG color instead of bitmap color
    auto bg = RGB888toPaletteIndex(paintState().penColor);
    genericRawDrawTransformedBitmap_RGBA2222(destX, destY, drawingRect, bitmap, invMatrix,
                                              [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                              // VGA16_GETPIXELINROW
                                              [&] (uint8_t * row, int x, uint8_t src) { setRowPixel(row, x, bg); }  // rawSetPixelInRow
                                             );
    return;
  }

  genericRawDrawTransformedBitmap_RGBA2222(destX, destY, drawingRect, bitmap, invMatrix,
                                          [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                          // VGA16_GETPIXELINROW
                                          [&] (uint8_t * row, int x, uint8_t src) { setRowPixel(row, x, RGB2222toPaletteIndex(src)); }  // rawSetPixelInRow
                                         );
}

void Painter16::rawDrawBitmapWithMatrix_RGBA8888(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);

  if (paintState().paintOptions.swapFGBG) {
    // used for bitmap plots to indicate drawing with BG color instead of bitmap color
    auto bg = RGB888toPaletteIndex(paintState().penColor);
    genericRawDrawTransformedBitmap_RGBA8888(destX, destY, drawingRect, bitmap, invMatrix,
                                            [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                            // VGA16_GETPIXELINROW,                                                                     // rawGetPixelInRow
                                            [&] (uint8_t * row, int x, RGBA8888 const & src) { setRowPixel(row, x, bg); }           // rawSetPixelInRow
                                          );
    return;
  }

  genericRawDrawTransformedBitmap_RGBA8888(destX, destY, drawingRect, bitmap, invMatrix,
                                          [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                          // VGA16_GETPIXELINROW,                                                                      // rawGetPixelInRow
                                          [&] (uint8_t * row, int x, RGBA8888 const & src) { setRowPixel(row, x, RGB8888toPaletteIndex(src)); }   // rawSetPixelInRow
                                         );
}

} // end of namespace
