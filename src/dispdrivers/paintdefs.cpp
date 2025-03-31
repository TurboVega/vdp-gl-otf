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

#include "paintdefs.h"
#include <math.h>
#include "esp_heap_caps.h"
#include <string.h>

namespace fabgl {

bool RGB222::lowBitOnly = false;

//   0 ..  63 => 0
//  64 .. 127 => 1
// 128 .. 191 => 2
// 192 .. 255 => 3
RGB222::RGB222(RGB888 const & value)
{
  if (lowBitOnly) {
    R = value.R ? 3 : 0;
    G = value.G ? 3 : 0;
    B = value.B ? 3 : 0;
  } else {
    R = value.R >> 6;
    G = value.G >> 6;
    B = value.B >> 6;
  }
}

// Array to convert Color enum to RGB888 struct
const RGB888 COLOR2RGB888[16] = {
  {   0,   0,   0 }, // Black
  { 128,   0,   0 }, // Red
  {   0, 128,   0 }, // Green
  { 128, 128,   0 }, // Yellow
  {   0,   0, 128 }, // Blue
  { 128,   0, 128 }, // Magenta
  {   0, 128, 128 }, // Cyan
  { 128, 128, 128 }, // White
  {  64,  64,  64 }, // BrightBlack
  { 255,   0,   0 }, // BrightRed
  {   0, 255,   0 }, // BrightGreen
  { 255, 255,   0 }, // BrightYellow
  {   0,   0, 255 }, // BrightBlue
  { 255,   0, 255 }, // BrightMagenta
  {   0, 255, 255 }, // BrightCyan
  { 255, 255, 255 }, // BrightWhite
};

RGB888::RGB888(Color color)
{
  *this = COLOR2RGB888[(int)color];
}

uint8_t RGB888toPackedRGB222(RGB888 const & rgb)
{
  // 64 colors
  static const int CONVR64[4] = { 0 << 0,    // 00XXXXXX (0..63)
                                  1 << 0,    // 01XXXXXX (64..127)
                                  2 << 0,    // 10XXXXXX (128..191)
                                  3 << 0, }; // 11XXXXXX (192..255)
  static const int CONVG64[4] = { 0 << 2,    // 00XXXXXX (0..63)
                                  1 << 2,    // 01XXXXXX (64..127)
                                  2 << 2,    // 10XXXXXX (128..191)
                                  3 << 2, }; // 11XXXXXX (192..255)
  static const int CONVB64[4] = { 0 << 4,    // 00XXXXXX (0..63)
                                  1 << 4,    // 01XXXXXX (64..127)
                                  2 << 4,    // 10XXXXXX (128..191)
                                  3 << 4, }; // 11XXXXXX (192..255)
  // 8 colors
  static const int CONVR8[4] = { 0 << 0,    // 00XXXXXX (0..63)
                                 3 << 0,    // 01XXXXXX (64..127)
                                 3 << 0,    // 10XXXXXX (128..191)
                                 3 << 0, }; // 11XXXXXX (192..255)
  static const int CONVG8[4] = { 0 << 2,    // 00XXXXXX (0..63)
                                 3 << 2,    // 01XXXXXX (64..127)
                                 3 << 2,    // 10XXXXXX (128..191)
                                 3 << 2, }; // 11XXXXXX (192..255)
  static const int CONVB8[4] = { 0 << 4,    // 00XXXXXX (0..63)
                                 3 << 4,    // 01XXXXXX (64..127)
                                 3 << 4,    // 10XXXXXX (128..191)
                                 3 << 4, }; // 11XXXXXX (192..255)

  if (RGB222::lowBitOnly)
    return (CONVR8[rgb.R >> 6]) | (CONVG8[rgb.G >> 6]) | (CONVB8[rgb.B >> 6]);
  else
    return (CONVR64[rgb.R >> 6]) | (CONVG64[rgb.G >> 6]) | (CONVB64[rgb.B >> 6]);
}

// Integer square root by Halleck's method, with Legalize's speedup
int isqrt (int x)
{
  if (x < 1)
    return 0;
  int squaredbit = 0x40000000;
  int remainder = x;
  int root = 0;
  while (squaredbit > 0) {
    if (remainder >= (squaredbit | root)) {
      remainder -= (squaredbit | root);
      root >>= 1;
      root |= squaredbit;
    } else
      root >>= 1;
    squaredbit >>= 2;
  }
  return root;
}

uint8_t getCircleQuadrant(int x, int y) {
  if (x < 0) {
    if (y > 0) {
      return 2;
    }
    return 1;
  }
  if (y <= 0) {
    return 0;
  }
  return 3;
}


bool quadrantContainsArcPixel(QuadrantInfo & quadrant, LineInfo & start, LineInfo & end, int16_t x, int16_t y) {
  // Work out whether our arc circumference pixel should be shown in this quadrant
  bool drawing = false;
  if (quadrant.showAll) {
    return true;
  } else if (!quadrant.noArc) {
    if (quadrant.containsStart) {
      auto slopeTest = start.absDeltaY * abs(x);
      if (quadrant.isEven) {
        drawing = slopeTest <= (start.absDeltaX * abs(y));
      } else {
        drawing = slopeTest >= (start.absDeltaX * abs(y));
      }
      if (quadrant.containsEnd) {
        slopeTest = end.absDeltaY * abs(x);
        bool drawingEnd = false;
        if (quadrant.isEven) {
          drawingEnd = (slopeTest >= (end.absDeltaX * abs(y)));
        } else {
          drawingEnd = (slopeTest <= (end.absDeltaX * abs(y)));
        }
        if (quadrant.containsStart && quadrant.containsEnd) {
          if (quadrant.startCloserToHorizontal ^ quadrant.isEven) {
            return drawing || drawingEnd;
          } else {
            return drawing && drawingEnd;
          }
        }
      }
    } else if (quadrant.containsEnd) {
      auto slopeTest = end.absDeltaY * abs(x);
      if (quadrant.isEven) {
        return slopeTest >= (end.absDeltaX * abs(y));
      } else {
        return slopeTest <= (end.absDeltaX * abs(y));
      }
    }
  }
  return drawing;
}

Bitmap::Bitmap(int width_, int height_, void const * data_, PixelFormat format_, RGB888 foregroundColor_, bool copy)
  : width(width_),
    height(height_),
    format(format_),
    foregroundColor(foregroundColor_),
    data((uint8_t*)data_),
    dataAllocated(false)
{
  if (copy) {
    allocate();
    copyFrom(data_);
  }
}


Bitmap::Bitmap(int width_, int height_, void const * data_, PixelFormat format_, bool copy)
  : Bitmap(width_, height_, data_, format_, RGB888(255, 255, 255), copy)
{
}


void Bitmap::allocate()
{
  if (dataAllocated) {
    free((void*)data);
    data = nullptr;
  }
  dataAllocated = true;
  switch (format) {
    case PixelFormat::Undefined:
    case PixelFormat::Native:
      break;
    case PixelFormat::Mask:
      data = (uint8_t*) heap_caps_malloc((width + 7) / 8 * height, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
      break;
    case PixelFormat::RGBA2222:
      data = (uint8_t*) heap_caps_malloc(width * height, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
      break;
    case PixelFormat::RGBA8888:
      data = (uint8_t*) heap_caps_malloc(width * height * 4, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
      break;
  }
}


// data must have the same pixel format
void Bitmap::copyFrom(void const * srcData)
{
  switch (format) {
    case PixelFormat::Undefined:
    case PixelFormat::Native:
      break;
    case PixelFormat::Mask:
      memcpy(data, srcData, (width + 7) / 8 * height);
      break;
    case PixelFormat::RGBA2222:
      memcpy(data, srcData, width * height);
      break;
    case PixelFormat::RGBA8888:
      memcpy(data, srcData, width * height * 4);
      break;
  }
}


void Bitmap::setPixel(int x, int y, int value)
{
  int rowlen = (width + 7) / 8;
  uint8_t * rowptr = data + y * rowlen;
  if (value)
    rowptr[x >> 3] |= 0x80 >> (x & 7);
  else
    rowptr[x >> 3] &= ~(0x80 >> (x & 7));
}


void Bitmap::setPixel(int x, int y, RGBA2222 value)
{
  ((RGBA2222*)data)[y * width + x] = value;
}


void Bitmap::setPixel(int x, int y, RGBA8888 value)
{
  ((RGBA8888*)data)[y * width + x] = value;
}


int Bitmap::getAlpha(int x, int y)
{
  int r = 0;
  switch (format) {
    case PixelFormat::Undefined:
      break;
    case PixelFormat::Native:
      r = 0xff;
      break;
    case PixelFormat::Mask:
    {
      int rowlen = (width + 7) / 8;
      uint8_t * rowptr = data + y * rowlen;
      r = (rowptr[x >> 3] >> (7 - (x & 7))) & 1;
      break;
    }
    case PixelFormat::RGBA2222:
      r = ((RGBA2222*)data)[y * height + x].A;
      break;
    case PixelFormat::RGBA8888:
      r = ((RGBA8888*)data)[y * height + x].A;
      break;
  }
  return r;
}


RGBA2222 Bitmap::getPixel2222(int x, int y) const {
  RGBA2222 value = RGBA2222(0,0,0,0);

  if (x < 0 || x >= width || y < 0 || y >= height) {
    return value;
  }

  switch (format) {
    case PixelFormat::Undefined:
      break;
    case PixelFormat::Native:
      break;
    case PixelFormat::Mask:
    {
      int rowlen = (width + 7) / 8;
      uint8_t * rowptr = data + y * rowlen;
      value.A = (rowptr[x >> 3] >> (7 - (x & 7))) & 1 ? 3 : 0;
      if (value.A) {
        value.R = foregroundColor.R >> 6;
        value.G = foregroundColor.G >> 6;
        value.B = foregroundColor.B >> 6;
      } else {
        value.R = value.G = value.B = 0;
      }
      break;
    }
    case PixelFormat::RGBA2222:
      value = ((RGBA2222*)data)[y * width + x];
      break;
    case PixelFormat::RGBA8888:
    {
      RGBA8888 rgba = ((RGBA8888*)data)[y * width + x];
      value.A = rgba.A >> 6;
      value.B = rgba.B >> 6;
      value.G = rgba.G >> 6;
      value.R = rgba.R >> 6;
      break;
    }
  }

  return value;
}


RGBA8888 Bitmap::getPixel8888(int x, int y) const {
  RGBA8888 value = RGBA8888(0,0,0,0);

  if (x < 0 || x >= width || y < 0 || y >= height) {
    return value;
  }

  switch (format) {
    case PixelFormat::Undefined:
      break;
    case PixelFormat::Native:
      break;
    case PixelFormat::Mask:
    {
      int rowlen = (width + 7) / 8;
      uint8_t * rowptr = data + y * rowlen;
      value.A = (rowptr[x >> 3] >> (7 - (x & 7))) & 1 ? 255 : 0;
      if (value.A) {
        value.R = foregroundColor.R;
        value.G = foregroundColor.G;
        value.B = foregroundColor.B;
      } else {
        value.R = value.G = value.B = 0;
      }
      break;
    }
    case PixelFormat::RGBA2222:
    {
      RGBA2222 rgba = ((RGBA2222*)data)[y * width + x];
      value.A = rgba.A * 85;
      value.B = rgba.B * 85;
      value.G = rgba.G * 85;
      value.R = rgba.R * 85;
      break;
    }
    case PixelFormat::RGBA8888:
      value = ((RGBA8888*)data)[y * width + x];
      break;
  }

  return value;
}


Bitmap::~Bitmap()
{
  if (dataAllocated)
    heap_caps_free((void*)data);
}


////////////////////////////////////////////////////////////////////////////////////////////
// Sutherland-Cohen line clipping algorithm

static int clipLine_code(int x, int y, Rect const & clipRect)
{
  int code = 0;
  if (x < clipRect.X1)
    code = 1;
  else if (x > clipRect.X2)
    code = 2;
  if (y < clipRect.Y1)
    code |= 4;
  else if (y > clipRect.Y2)
    code |= 8;
  return code;
}

// false = line is out of clipping rect
// true = line intersects or is inside the clipping rect (x1, y1, x2, y2 are changed if checkOnly=false)
bool clipLine(int & x1, int & y1, int & x2, int & y2, Rect const & clipRect, bool checkOnly)
{
  int newX1 = x1;
  int newY1 = y1;
  int newX2 = x2;
  int newY2 = y2;
  int topLeftCode     = clipLine_code(newX1, newY1, clipRect);
  int bottomRightCode = clipLine_code(newX2, newY2, clipRect);
  while (true) {
    if ((topLeftCode == 0) && (bottomRightCode == 0)) {
      if (!checkOnly) {
        x1 = newX1;
        y1 = newY1;
        x2 = newX2;
        y2 = newY2;
      }
      return true;
    } else if (topLeftCode & bottomRightCode) {
      break;
    } else {
      int x = 0, y = 0;
      int ncode = topLeftCode != 0 ? topLeftCode : bottomRightCode;
      if (ncode & 8) {
        x = newX1 + (newX2 - newX1) * (clipRect.Y2 - newY1) / (newY2 - newY1);
        y = clipRect.Y2;
      } else if (ncode & 4) {
        x = newX1 + (newX2 - newX1) * (clipRect.Y1 - newY1) / (newY2 - newY1);
        y = clipRect.Y1;
      } else if (ncode & 2) {
        y = newY1 + (newY2 - newY1) * (clipRect.X2 - newX1) / (newX2 - newX1);
        x = clipRect.X2;
      } else if (ncode & 1) {
        y = newY1 + (newY2 - newY1) * (clipRect.X1 - newX1) / (newX2 - newX1);
        x = clipRect.X1;
      }
      if (ncode == topLeftCode) {
        newX1 = x;
        newY1 = y;
        topLeftCode = clipLine_code(newX1, newY1, clipRect);
      } else {
        newX2 = x;
        newY2 = y;
        bottomRightCode = clipLine_code(newX2, newY2, clipRect);
      }
    }
  }
  return false;
}


} // namespace
