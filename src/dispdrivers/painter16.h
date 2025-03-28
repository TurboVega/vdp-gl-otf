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


#pragma once

#include "painter.h"

namespace fabgl {

class Painter16 : public Painter {

  public:

  Painter16();
  ~Painter16();

  virtual void readScreen(Rect const & rect, RGB222 * destBuf);

  virtual void readScreen(Rect const & rect, RGB888 * destBuf);

  virtual void writeScreen(Rect const & rect, RGB222 * srcBuf);

  virtual void writeScreen(Rect const & rect, RGB888 * srcBuf);

  virtual void setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect);

  virtual void drawEllipse(Size const & size, Rect & updateRect);

  virtual void drawArc(Rect const & rect, Rect & updateRect);

  virtual void fillSegment(Rect const & rect, Rect & updateRect);

  virtual void fillSector(Rect const & rect, Rect & updateRect);

  virtual void clear(Rect & updateRect);

  virtual void VScroll(int scroll, Rect & updateRect);

  virtual void HScroll(int scroll, Rect & updateRect);

  virtual void drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect);

  virtual void invertRect(Rect const & rect, Rect & updateRect);

  virtual void copyRect(Rect const & source, Rect & updateRect);

  virtual void swapFGBG(Rect const & rect, Rect & updateRect);

  virtual void rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount);

  virtual void rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount);

  virtual void rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount);

  virtual void rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount);

  virtual void rawCopyToBitmap(int srcX, int srcY, int width, void * saveBuffer, int X1, int Y1, int XCount, int YCount);

  virtual void rawDrawBitmapWithMatrix_Mask(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix);

  virtual void rawDrawBitmapWithMatrix_RGBA2222(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix);

  virtual void rawDrawBitmapWithMatrix_RGBA8888(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix);

  virtual void fillRow(int y, int x1, int x2, RGB888 color);

  virtual void rawFillRow(int y, int x1, int x2, uint8_t colorIndex);

  virtual void rawORRow(int y, int x1, int x2, uint8_t colorIndex);

  virtual void rawANDRow(int y, int x1, int x2, uint8_t colorIndex);

  virtual void rawXORRow(int y, int x1, int x2, uint8_t colorIndex);

  virtual void rawInvertRow(int y, int x1, int x2);

  virtual void rawCopyRow(int x1, int x2, int srcY, int dstY);

  virtual void swapRows(int yA, int yB, int x1, int x2);

  virtual void absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color);

  virtual int getPaletteSize() { return (int) NativePixelFormat::PALETTE16; };

  virtual void packSignals(int index, uint8_t packed222, void * signals);

  protected:

  // methods to get lambdas to get/set pixels
  std::function<uint8_t(RGB888 const &)> getPixelLambda(PaintMode mode);
  std::function<void(int X, int Y, uint8_t colorIndex)> setPixelLambda(PaintMode mode);
  std::function<void(uint8_t * row, int x, uint8_t colorIndex)> setRowPixelLambda(PaintMode mode);
  std::function<void(int Y, int X1, int X2, uint8_t colorIndex)> fillRowLambda(PaintMode mode);

};

} // end of namespace
