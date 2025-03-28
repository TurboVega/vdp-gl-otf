/*
  Painter4 - Paints (draws) using 4 colors

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

#include "painter4.h"

#pragma GCC optimize ("O2")

namespace fabgl {

static inline __attribute__((always_inline)) void VGA4_SETPIXELINROW(uint8_t * row, int x, int value) {
  int brow = x >> 2;
  int shift = 6 - (x & 3) * 2;
  row[brow] ^= ((value << shift) ^ row[brow]) & (3 << shift);
}

static inline __attribute__((always_inline)) int VGA4_GETPIXELINROW(uint8_t * row, int x) {
  int brow = x >> 2;
  int shift = 6 - (x & 3) * 2;
  return (row[brow] >> shift) & 3;
}

#define VGA4_INVERTPIXELINROW(row, x)       (row)[(x) >> 2] ^= 3 << (6 - ((x) & 3) * 2)

static inline __attribute__((always_inline)) void VGA4_SETPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) Painter4::sgetScanline(y);
  int brow = x >> 2;
  int shift = 6 - (x & 3) * 2;
  row[brow] ^= ((value << shift) ^ row[brow]) & (3 << shift);
}

static inline __attribute__((always_inline)) void VGA4_ORPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) Painter4::sgetScanline(y);
  int brow = x >> 2;
  int shift = 6 - (x & 3) * 2;
  row[brow] |= (value << shift);
}

static inline __attribute__((always_inline)) void VGA4_ANDPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) Painter4::sgetScanline(y);
  int brow = x >> 2;
  int shift = 6 - (x & 3) * 2;
  // byte to write needs to have 1s in non-masked bits
  value = 0xFF ^ (3 << shift) | (value << shift);
  row[brow] &= value;
}

static inline __attribute__((always_inline)) void VGA4_XORPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) Painter4::sgetScanline(y);
  int brow = x >> 2;
  int shift = 6 - (x & 3) * 2;
  row[brow] ^= (value << shift);
}

#define VGA4_GETPIXEL(x, y)                 VGA4_GETPIXELINROW((uint8_t*)Painter4::m_viewPort[(y)], (x))

#define VGA4_INVERT_PIXEL(x, y)             VGA4_INVERTPIXELINROW((uint8_t*)Painter4::m_viewPort[(y)], (x))

#define VGA4_COLUMNSQUANTUM 16

/*************************************************************************************/
/* Painter4 definitions */

Painter4::Painter4() {
  postConstruct();
}

Painter4::~Painter4() {
  Painter::~Painter();
}

std::function<uint8_t(RGB888 const &)> Painter4::getPixelLambda(PaintMode mode) {
  return [&] (RGB888 const & color) { return RGB888toPaletteIndex(color); };
}

std::function<void(int X, int Y, uint8_t colorIndex)> Painter4::setPixelLambda(PaintMode mode) {
  switch (mode) {
    case PaintMode::Set:
      return VGA4_SETPIXEL;
    case PaintMode::OR:
      return VGA4_ORPIXEL;
    case PaintMode::ORNOT:
      return [&] (int X, int Y, uint8_t colorIndex) { VGA4_ORPIXEL(X, Y, ~colorIndex & 0x03); };
    case PaintMode::AND:
      return VGA4_ANDPIXEL;
    case PaintMode::ANDNOT:
      return [&] (int X, int Y, uint8_t colorIndex) { VGA4_ANDPIXEL(X, Y, ~colorIndex); };
    case PaintMode::XOR:
      return VGA4_XORPIXEL;
    case PaintMode::Invert:
      return [&] (int X, int Y, uint8_t colorIndex) { VGA4_INVERT_PIXEL(X, Y); };
    default:  // PaintMode::NoOp
      return [&] (int X, int Y, uint8_t colorIndex) { return; };
  }
}

std::function<void(uint8_t * row, int x, uint8_t colorIndex)> Painter4::setRowPixelLambda(PaintMode mode) {
  switch (mode) {
    case PaintMode::Set:
      return VGA4_SETPIXELINROW;
    case PaintMode::OR:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) {
        VGA4_SETPIXELINROW(row, x, VGA4_GETPIXELINROW(row, x) | colorIndex);
      };
    case PaintMode::ORNOT:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) {
        VGA4_SETPIXELINROW(row, x, VGA4_GETPIXELINROW(row, x) | (~colorIndex & 0x03));
      };
    case PaintMode::AND:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) {
        VGA4_SETPIXELINROW(row, x, VGA4_GETPIXELINROW(row, x) & colorIndex);
      };
    case PaintMode::ANDNOT:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) {
        VGA4_SETPIXELINROW(row, x, VGA4_GETPIXELINROW(row, x) & ~colorIndex);
      };
    case PaintMode::XOR:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) {
        VGA4_SETPIXELINROW(row, x, VGA4_GETPIXELINROW(row, x) ^ colorIndex);
      };
    case PaintMode::Invert:
      return [&] (uint8_t * row, int x, uint8_t colorIndex) { VGA4_INVERTPIXELINROW(row, x); };
    default:  // PaintMode::NoOp
      return [&] (uint8_t * row, int x, uint8_t colorIndex) { return; };
  }
}

std::function<void(int Y, int X1, int X2, uint8_t colorIndex)> Painter4::fillRowLambda(PaintMode mode) {
  switch (mode) {
    case PaintMode::Set:
      return [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawFillRow(Y, X1, X2, colorIndex); };
    case PaintMode::OR:
      return [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawORRow(Y, X1, X2, colorIndex); };
    case PaintMode::ORNOT:
      return [&] (int Y, int X1, int X2, uint8_t colorIndex) { rawORRow(Y, X1, X2, ~colorIndex & 0x03); };
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

void Painter4::setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect) {
  auto paintMode = paintState().paintOptions.mode;
  genericSetPixelAt(pixelDesc, updateRect, getPixelLambda(paintMode), setPixelLambda(paintMode));
}

// coordinates are absolute values (not relative to origin)
// line clipped on current absolute clipping rectangle
void Painter4::absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color) {
  auto paintMode = paintState().paintOptions.NOT ? PaintMode::NOT : paintState().paintOptions.mode;
  genericAbsDrawLine(X1, Y1, X2, Y2, color,
                     getPixelLambda(paintMode),
                     fillRowLambda(paintMode),
                     setPixelLambda(paintMode)
                     );
}

// parameters not checked
void Painter4::fillRow(int y, int x1, int x2, RGB888 color) {
  // pick fill method based on paint mode
  auto paintMode = paintState().paintOptions.mode;
  auto getPixel = getPixelLambda(paintMode);
  auto pixel = getPixel(color);
  auto fill = fillRowLambda(paintMode);
  fill(y, x1, x2, pixel);
}

// parameters not checked
void Painter4::rawFillRow(int y, int x1, int x2, uint8_t colorIndex) {
  uint8_t * row = (uint8_t*) m_viewPort[y];
  // fill first pixels before full 8 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    VGA4_SETPIXELINROW(row, x, colorIndex);
  }
  // fill whole 8 bits words (4 pixels)
  if (x <= x2) {
    int sz = (x2 & ~3) - x;
    memset((void*)(row + x / 4), colorIndex | (colorIndex << 2) | (colorIndex << 4) | (colorIndex << 6), sz / 4);
    x += sz;
  }
  // fill last unaligned pixels
  for (; x <= x2; ++x) {
    VGA4_SETPIXELINROW(row, x, colorIndex);
  }
}

// parameters not checked
void Painter4::rawORRow(int y, int x1, int x2, uint8_t colorIndex) {
  // naive implementation - just iterate over all pixels
  for (int x = x1; x <= x2; ++x) {
    VGA4_ORPIXEL(x, y, colorIndex);
  }
}

// parameters not checked
void Painter4::rawANDRow(int y, int x1, int x2, uint8_t colorIndex) {
  // naive implementation - just iterate over all pixels
  for (int x = x1; x <= x2; ++x) {
    VGA4_ANDPIXEL(x, y, colorIndex);
  }
}

// parameters not checked
void Painter4::rawXORRow(int y, int x1, int x2, uint8_t colorIndex) {
  // naive implementation - just iterate over all pixels
  for (int x = x1; x <= x2; ++x) {
    VGA4_XORPIXEL(x, y, colorIndex);
  }
}

// parameters not checked
void Painter4::rawInvertRow(int y, int x1, int x2) {
  auto row = m_viewPort[y];
  for (int x = x1; x <= x2; ++x)
    VGA4_INVERTPIXELINROW(row, x);
}

void Painter4::rawCopyRow(int x1, int x2, int srcY, int dstY) {
  auto srcRow = (uint8_t*) m_viewPort[srcY];
  auto dstRow = (uint8_t*) m_viewPort[dstY];
  // copy first pixels before full 8 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    VGA4_SETPIXELINROW(dstRow, x, VGA4_GETPIXELINROW(srcRow, x));
  }
  // copy whole 8 bits words (4 pixels)
  auto src = (uint8_t*)(srcRow + x / 4);
  auto dst = (uint8_t*)(dstRow + x / 4);
  for (int right = (x2 & ~3); x < right; x += 4)
    *dst++ = *src++;
  // copy last unaligned pixels
  for (x = (x2 & ~3); x <= x2; ++x) {
    VGA4_SETPIXELINROW(dstRow, x, VGA4_GETPIXELINROW(srcRow, x));
  }
}

void Painter4::swapRows(int yA, int yB, int x1, int x2) {
  auto rowA = (uint8_t*) m_viewPort[yA];
  auto rowB = (uint8_t*) m_viewPort[yB];
  // swap first pixels before full 8 bits word
  int x = x1;
  for (; x <= x2 && (x & 3) != 0; ++x) {
    uint8_t a = VGA4_GETPIXELINROW(rowA, x);
    uint8_t b = VGA4_GETPIXELINROW(rowB, x);
    VGA4_SETPIXELINROW(rowA, x, b);
    VGA4_SETPIXELINROW(rowB, x, a);
  }
  // swap whole 8 bits words (4 pixels)
  auto a = (uint8_t*)(rowA + x / 4);
  auto b = (uint8_t*)(rowB + x / 4);
  for (int right = (x2 & ~3); x < right; x += 4)
    tswap(*a++, *b++);
  // swap last unaligned pixels
  for (x = (x2 & ~3); x <= x2; ++x) {
    uint8_t a = VGA4_GETPIXELINROW(rowA, x);
    uint8_t b = VGA4_GETPIXELINROW(rowB, x);
    VGA4_SETPIXELINROW(rowA, x, b);
    VGA4_SETPIXELINROW(rowB, x, a);
  }
}

void Painter4::drawEllipse(Size const & size, Rect & updateRect) {
  auto mode = paintState().paintOptions.mode;
  genericDrawEllipse(size, updateRect, getPixelLambda(mode), setPixelLambda(mode));
}

void Painter4::drawArc(Rect const & rect, Rect & updateRect) {
  auto mode = paintState().paintOptions.mode;
  genericDrawArc(rect, updateRect, getPixelLambda(mode), setPixelLambda(mode));
}

void Painter4::fillSegment(Rect const & rect, Rect & updateRect) {
  auto mode = paintState().paintOptions.mode;
  genericFillSegment(rect, updateRect, getPixelLambda(mode), fillRowLambda(mode));
}

void Painter4::fillSector(Rect const & rect, Rect & updateRect) {
  auto mode = paintState().paintOptions.mode;
  genericFillSector(rect, updateRect, getPixelLambda(mode), fillRowLambda(mode));
}

void Painter4::clear(Rect & updateRect) {
  uint8_t paletteIndex = RGB888toPaletteIndex(getActualBrushColor());
  uint8_t pattern4 = paletteIndex | (paletteIndex << 2) | (paletteIndex << 4) | (paletteIndex << 6);
  for (int y = 0; y < m_viewPortHeight; ++y)
    memset((uint8_t*) m_viewPort[y], pattern4, m_viewPortWidth / 4);
}

// scroll < 0 -> scroll UP
// scroll > 0 -> scroll DOWN
void Painter4::VScroll(int scroll, Rect & updateRect) {
  genericVScroll(scroll, updateRect,
                 [&] (int yA, int yB, int x1, int x2)        { swapRows(yA, yB, x1, x2); },              // swapRowsCopying
                 [&] (int yA, int yB)                        { tswap(m_viewPort[yA], m_viewPort[yB]); }, // swapRowsPointers
                 [&] (int y, int x1, int x2, RGB888 color)   { rawFillRow(y, x1, x2, RGB888toPaletteIndex(color)); }  // rawFillRow
                );
}

void Painter4::HScroll(int scroll, Rect & updateRect) {
  uint8_t back  = RGB888toPaletteIndex(getActualBrushColor());
  uint8_t back4 = back | (back << 2) | (back << 4) | (back << 6);

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
        uint8_t * row = (uint8_t*) (m_viewPort[y]) + X1 / 4;
        for (int s = -scroll; s > 0;) {
          if (s < 4) {
            // scroll left by 1..3
            int sz = width / 4;
            uint8_t prev = back4;
            for (int i = sz - 1; i >= 0; --i) {
              uint8_t lowbits = prev >> (8 - s * 2);
              prev = row[i];
              row[i] = (row[i] << (s * 2)) | lowbits;
            }
            s = 0;
          } else {
            // scroll left by multiplies of 4
            auto sc = s & ~3;
            auto sz = width & ~3;
            memmove(row, row + sc / 4, (sz - sc) / 4);
            rawFillRow(y, X2 - sc + 1, X2, back);
            s -= sc;
          }
        }
      } else {
        // unaligned horizontal scrolling region, fallback to slow version
        auto row = (uint8_t*) m_viewPort[y];
        for (int x = X1; x <= X2 + scroll; ++x)
          VGA4_SETPIXELINROW(row, x, VGA4_GETPIXELINROW(row, x - scroll));
        // fill right area with brush color
        rawFillRow(y, X2 + 1 + scroll, X2, back);
      }
    }
  } else if (scroll > 0) {
    // scroll right
    for (int y = Y1; y <= Y2; ++y) {
      if (HScrolllingRegionAligned) {
        // aligned horizontal scrolling region, fast version
        uint8_t * row = (uint8_t*) (m_viewPort[y]) + X1 / 4;
        for (int s = scroll; s > 0;) {
          if (s < 4) {
            // scroll right by 1..3
            int sz = width / 4;
            uint8_t prev = back4;
            for (int i = 0; i < sz; ++i) {
              uint8_t highbits = prev << (8 - s * 2);
              prev = row[i];
              row[i] = (row[i] >> (s * 2)) | highbits;
            }
            s = 0;
          } else {
            // scroll right by multiplies of 4
            auto sc = s & ~3;
            auto sz = width & ~3;
            memmove(row + sc / 4, row, (sz - sc) / 4);
            rawFillRow(y, X1, X1 + sc - 1, back);
            s -= sc;
          }
        }
      } else {
        // unaligned horizontal scrolling region, fallback to slow version
        auto row = (uint8_t*) m_viewPort[y];
        for (int x = X2 - scroll; x >= X1; --x)
          VGA4_SETPIXELINROW(row, x + scroll, VGA4_GETPIXELINROW(row, x));
        // fill left area with brush color
        rawFillRow(y, X1, X1 + scroll - 1, back);
      }
    }

  }
}

void Painter4::drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect) {
  auto mode = paintState().paintOptions.mode;
  auto getPixel = getPixelLambda(mode);
  auto setRowPixel = setRowPixelLambda(mode);
  genericDrawGlyph(glyph, glyphOptions, penColor, brushColor, updateRect,
                   getPixel,
                   [&] (int y) { return (uint8_t*) m_viewPort[y]; },
                   setRowPixel
                  );
}

void Painter4::invertRect(Rect const & rect, Rect & updateRect) {
  genericInvertRect(rect, updateRect,
                    [&] (int Y, int X1, int X2) { rawInvertRow(Y, X1, X2); }
                   );
}

void Painter4::swapFGBG(Rect const & rect, Rect & updateRect) {
  genericSwapFGBG(rect, updateRect,
                  [&] (RGB888 const & color)                     { return RGB888toPaletteIndex(color); },
                  [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                  VGA4_GETPIXELINROW,
                  VGA4_SETPIXELINROW
                 );
}

// Slow operation!
// supports overlapping of source and dest rectangles
void Painter4::copyRect(Rect const & source, Rect & updateRect) {
  genericCopyRect(source, updateRect,
                  [&] (int y)                                    { return (uint8_t*) m_viewPort[y]; },
                  VGA4_GETPIXELINROW,
                  VGA4_SETPIXELINROW
                 );
}

void Painter4::readScreen(Rect const & rect, RGB222 * destBuf) {
}

// no bounds check is done!
void Painter4::readScreen(Rect const & rect, RGB888 * destBuf) {
  for (int y = rect.Y1; y <= rect.Y2; ++y) {
    auto row = (uint8_t*) m_viewPort[y];
    for (int x = rect.X1; x <= rect.X2; ++x, ++destBuf) {
      const RGB222 v = m_palette[VGA4_GETPIXELINROW(row, x)];
      *destBuf = RGB888(v.R * 85, v.G * 85, v.B * 85);  // 85 x 3 = 255
    }
  }
}

void Painter4::writeScreen(Rect const & rect, RGB888 * destBuf) {
}

void Painter4::writeScreen(Rect const & rect, RGB222 * destBuf) {
}

void Painter4::rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount) {
  genericRawDrawBitmap_Native(destX, destY, (uint8_t*) bitmap->data, bitmap->width, X1, Y1, XCount, YCount,
                              [&] (int y) { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                              VGA4_SETPIXELINROW
                             );
}

void Painter4::rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);
  auto foregroundColorIndex = RGB888toPaletteIndex(paintState().paintOptions.swapFGBG ? paintState().penColor : bitmap->foregroundColor);
  genericRawDrawBitmap_Mask(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                            [&] (int y)                  { return (uint8_t*) m_viewPort[y]; },           // rawGetRow
                            VGA4_GETPIXELINROW,
                            [&] (uint8_t * row, int x)   { setRowPixel(row, x, foregroundColorIndex); }  // rawSetPixelInRow
                           );
}

void Painter4::rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);

  if (paintState().paintOptions.swapFGBG) {
    // used for bitmap plots to indicate drawing with BG color instead of bitmap color
    auto bg = RGB888toPaletteIndex(paintState().penColor);
    genericRawDrawBitmap_RGBA2222(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                  [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                  VGA4_GETPIXELINROW,                                                            // rawGetPixelInRow
                                  [&] (uint8_t * row, int x, uint8_t src) { setRowPixel(row, x, bg); }           // rawSetPixelInRow
                                );
    return;
  }

  genericRawDrawBitmap_RGBA2222(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                [&] (int y)                             { return (uint8_t*) m_viewPort[y]; },                 // rawGetRow
                                VGA4_GETPIXELINROW,
                                [&] (uint8_t * row, int x, uint8_t src) { setRowPixel(row, x, RGB2222toPaletteIndex(src)); }  // rawSetPixelInRow
                               );
}

void Painter4::rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);

  if (paintState().paintOptions.swapFGBG) {
    // used for bitmap plots to indicate drawing with BG color instead of bitmap color
    auto bg = RGB888toPaletteIndex(paintState().penColor);
    genericRawDrawBitmap_RGBA8888(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                  [&] (int y)                                      { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                  VGA4_GETPIXELINROW,                                                                     // rawGetPixelInRow
                                  [&] (uint8_t * row, int x, RGBA8888 const & src) { setRowPixel(row, x, bg); }           // rawSetPixelInRow
                                  );
    return;
  }
  genericRawDrawBitmap_RGBA8888(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                 [&] (int y)                                      { return (uint8_t*) m_viewPort[y]; },                  // rawGetRow
                                 VGA4_GETPIXELINROW,                                                                                     // rawGetPixelInRow
                                 [&] (uint8_t * row, int x, RGBA8888 const & src) { setRowPixel(row, x, RGB8888toPaletteIndex(src)); }   // rawSetPixelInRow
                                );
}

void Painter4::rawCopyToBitmap(int srcX, int srcY, int width, void * saveBuffer, int X1, int Y1, int XCount, int YCount) {
  genericRawCopyToBitmap(srcX, srcY, width, (uint8_t*)saveBuffer, X1, Y1, XCount, YCount,
                        [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                        [&] (uint8_t * row, int x) {
                          auto rgb = m_palette[VGA4_GETPIXELINROW(row, x)];
                          return (0xC0 | (rgb.B << VGA_BLUE_BIT) | (rgb.G << VGA_GREEN_BIT) | (rgb.R << VGA_RED_BIT));
                        } // rawGetPixelInRow
                      );
}

void Painter4::rawDrawBitmapWithMatrix_Mask(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);
  auto foregroundColorIndex = RGB888toPaletteIndex(paintState().paintOptions.swapFGBG ? paintState().penColor : bitmap->foregroundColor);
  genericRawDrawTransformedBitmap_Mask(destX, destY, drawingRect, bitmap, invMatrix,
                                          [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                          // VGA4_GETPIXELINROW
                                          [&] (uint8_t * row, int x) { setRowPixel(row, x, foregroundColorIndex); }  // rawSetPixelInRow
                                         );
}

void Painter4::rawDrawBitmapWithMatrix_RGBA2222(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);

  if (paintState().paintOptions.swapFGBG) {
    // used for bitmap plots to indicate drawing with BG color instead of bitmap color
    auto bg = RGB888toPaletteIndex(paintState().penColor);
    genericRawDrawTransformedBitmap_RGBA2222(destX, destY, drawingRect, bitmap, invMatrix,
                                              [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                              // VGA4_GETPIXELINROW
                                              [&] (uint8_t * row, int x, uint8_t src) { setRowPixel(row, x, bg); }  // rawSetPixelInRow
                                             );
    return;
  }

  genericRawDrawTransformedBitmap_RGBA2222(destX, destY, drawingRect, bitmap, invMatrix,
                                          [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                          // VGA4_GETPIXELINROW
                                          [&] (uint8_t * row, int x, uint8_t src) { setRowPixel(row, x, RGB2222toPaletteIndex(src)); }  // rawSetPixelInRow
                                         );
}

void Painter4::rawDrawBitmapWithMatrix_RGBA8888(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix) {
  auto paintMode = paintState().paintOptions.mode;
  auto setRowPixel = setRowPixelLambda(paintMode);

  if (paintState().paintOptions.swapFGBG) {
    // used for bitmap plots to indicate drawing with BG color instead of bitmap color
    auto bg = RGB888toPaletteIndex(paintState().penColor);
    genericRawDrawTransformedBitmap_RGBA8888(destX, destY, drawingRect, bitmap, invMatrix,
                                            [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                            // VGA4_GETPIXELINROW,                                                                     // rawGetPixelInRow
                                            [&] (uint8_t * row, int x, RGBA8888 const & src) { setRowPixel(row, x, bg); }           // rawSetPixelInRow
                                          );
    return;
  }

  genericRawDrawTransformedBitmap_RGBA8888(destX, destY, drawingRect, bitmap, invMatrix,
                                          [&] (int y)                { return (uint8_t*) m_viewPort[y]; },  // rawGetRow
                                          // VGA4_GETPIXELINROW,                                                                      // rawGetPixelInRow
                                          [&] (uint8_t * row, int x, RGBA8888 const & src) { setRowPixel(row, x, RGB8888toPaletteIndex(src)); }   // rawSetPixelInRow
                                         );
}

} // end of namespace
