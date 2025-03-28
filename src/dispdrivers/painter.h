/*
  Painter - Paints (draws) using N colors

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

#include "paintdefs.h"
#include <math.h>

namespace fabgl {

class Painter {

  public:

  Painter();
  ~Painter();

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

  protected:

  // methods to get lambdas to get/set pixels
  std::function<uint8_t(RGB888 const &)> getPixelLambda(PaintMode mode) = 0;
  std::function<virtual void(int X, int Y, uint8_t colorIndex)> setPixelLambda(PaintMode mode) = 0;
  std::function<virtual void(uint8_t * row, int x, uint8_t colorIndex)> setRowPixelLambda(PaintMode mode) = 0;
  std::function<virtual void(int Y, int X1, int X2, uint8_t colorIndex)> fillRowLambda(PaintMode mode) = 0;

  template <typename TPreparePixel, typename TRawSetPixel>
  void genericSetPixelAt(PixelDesc const & pixelDesc, Rect & updateRect, TPreparePixel preparePixel, TRawSetPixel rawSetPixel)
  {
    const int x = pixelDesc.pos.X + paintState().origin.X;
    const int y = pixelDesc.pos.Y + paintState().origin.Y;

    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    if (x >= clipX1 && x <= clipX2 && y >= clipY1 && y <= clipY2) {
      updateRect = updateRect.merge(Rect(x, y, x, y));
      rawSetPixel(x, y, preparePixel(pixelDesc.color));
    }
  }


  // coordinates are absolute values (not relative to origin)
  // line clipped on current absolute clipping rectangle
  template <typename TPreparePixel, typename TRawFillRow, typename TRawSetPixel>
  void genericAbsDrawLine(int X1, int Y1, int X2, int Y2, RGB888 const & color, TPreparePixel preparePixel, TRawFillRow rawFillRow, TRawSetPixel rawSetPixel)
  {
    auto & pState = paintState();
    if (pState.penWidth > 1) {
      absDrawThickLine(X1, Y1, X2, Y2, pState.penWidth, color);
      return;
    }
    auto & lineOptions = pState.lineOptions;
    bool dottedLine = lineOptions.usePattern;
    auto pattern = preparePixel(color);
    if (!dottedLine && (Y1 == Y2)) {
      // horizontal line
      if (Y1 < pState.absClippingRect.Y1 || Y1 > pState.absClippingRect.Y2)
        return;
      if (lineOptions.omitFirst) {
        if (X1 < X2)
          X1 += 1;
        else
          X1 -= 1;
      }
      if (lineOptions.omitLast) {
        if (X1 < X2)
          X2 -= 1;
        else
          X2 += 1;
      }
      if (X1 > X2)
        tswap(X1, X2);
      if (X1 > pState.absClippingRect.X2 || X2 < pState.absClippingRect.X1)
        return;
      X1 = iclamp(X1, pState.absClippingRect.X1, pState.absClippingRect.X2);
      X2 = iclamp(X2, pState.absClippingRect.X1, pState.absClippingRect.X2);
      rawFillRow(Y1, X1, X2, pattern);
    } else if (!dottedLine && (X1 == X2)) {
      // vertical line
      if (X1 < pState.absClippingRect.X1 || X1 > pState.absClippingRect.X2)
        return;
      if (lineOptions.omitFirst) {
        if (Y1 < Y2)
          Y1 += 1;
        else
          Y1 -= 1;
      }
      if (lineOptions.omitLast) {
        if (Y1 < Y2)
          Y2 -= 1;
        else
          Y2 += 1;
      }
      if (Y1 > Y2)
        tswap(Y1, Y2);
      if (Y1 > pState.absClippingRect.Y2 || Y2 < pState.absClippingRect.Y1)
        return;
      Y1 = iclamp(Y1, pState.absClippingRect.Y1, pState.absClippingRect.Y2);
      Y2 = iclamp(Y2, pState.absClippingRect.Y1, pState.absClippingRect.Y2);
      for (int y = Y1; y <= Y2; ++y)
        rawSetPixel(X1, y, pattern);
    } else {
      // other cases (Bresenham's algorithm)
      // TODO: to optimize
      //   Unfortunately here we cannot clip exactly using Sutherland-Cohen algorithm (as done before)
      //   because the starting line (got from clipping algorithm) may not be the same of Bresenham's
      //   line (think to continuing an existing line).
      //   Possible solutions:
      //      - "Yevgeny P. Kuzmin" algorithm:
      //               https://stackoverflow.com/questions/40884680/how-to-use-bresenhams-line-drawing-algorithm-with-clipping
      //               https://github.com/ktfh/ClippedLine/blob/master/clip.hpp
      // For now Sutherland-Cohen algorithm is only used to check the line is actually visible,
      // then test for every point inside the main Bresenham's loop.
      if (!clipLine(X1, Y1, X2, Y2, pState.absClippingRect, true))  // true = do not change line coordinates!
        return;
      auto & linePattern = pState.linePattern;
      auto linePatternLength = pState.linePatternLength;
      const int dx = abs(X2 - X1);
      const int dy = abs(Y2 - Y1);
      const int sx = X1 < X2 ? 1 : -1;
      const int sy = Y1 < Y2 ? 1 : -1;
      int err = (dx > dy ? dx : -dy) / 2;
      bool omittingFirst = lineOptions.omitFirst;
      bool omittingLast = lineOptions.omitLast;
      bool drawPixel = !omittingFirst;
      while (true) {
        bool ending = X1 == X2 && Y1 == Y2;
        if (dottedLine) {
          if (!omittingFirst && !(ending && omittingLast)) {
            drawPixel = getBit(linePattern.pattern, linePattern.offset);
            linePattern.offset = (linePattern.offset + 1) % linePatternLength;
          } else {
            drawPixel = false;
          }
        }
        if (drawPixel && pState.absClippingRect.contains(X1, Y1)) {
          rawSetPixel(X1, Y1, pattern);
        }
        if (omittingFirst)
          omittingFirst = false;
        if (ending)
          break;
        int e2 = err;
        if (e2 > -dx) {
          err -= dy;
          X1 += sx;
        }
        if (e2 < dy) {
          err += dx;
          Y1 += sy;
        }
      }
    }
  }


  // McIlroy's algorithm
  template <typename TPreparePixel, typename TRawSetPixel>
  void genericDrawEllipse(Size const & size, Rect & updateRect, TPreparePixel preparePixel, TRawSetPixel rawSetPixel)
  {
    auto pattern = preparePixel(getActualPenColor());

    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    const int centerX = paintState().position.X;
    const int centerY = paintState().position.Y;

    const int halfWidth  = size.width / 2;
    const int halfHeight = size.height / 2;

    updateRect = updateRect.merge(Rect(centerX - halfWidth, centerY - halfHeight, centerX + halfWidth, centerY + halfHeight));

    const int a2 = halfWidth * halfWidth;
    const int b2 = halfHeight * halfHeight;
    const int crit1 = -(a2 / 4 + halfWidth % 2 + b2);
    const int crit2 = -(b2 / 4 + halfHeight % 2 + a2);
    const int crit3 = -(b2 / 4 + halfHeight % 2);
    const int d2xt = 2 * b2;
    const int d2yt = 2 * a2;
    int x = 0;          // travels from 0 up to halfWidth
    int y = halfHeight; // travels from halfHeight down to 0
    int t = -a2 * y;
    int dxt = 2 * b2 * x;
    int dyt = -2 * a2 * y;

    while (y >= 0 && x <= halfWidth) {
      const int col1 = centerX - x;
      const int col2 = centerX + x;
      const int row1 = centerY - y;
      const int row2 = centerY + y;

      if (col1 >= clipX1 && col1 <= clipX2) {
        if (row1 >= clipY1 && row1 <= clipY2)
          rawSetPixel(col1, row1, pattern);
        if (row2 >= clipY1 && row2 <= clipY2)
          rawSetPixel(col1, row2, pattern);
      }
      if (col2 >= clipX1 && col2 <= clipX2) {
        if (row1 >= clipY1 && row1 <= clipY2)
          rawSetPixel(col2, row1, pattern);
        if (row2 >= clipY1 && row2 <= clipY2)
          rawSetPixel(col2, row2, pattern);
      }

      if (t + b2 * x <= crit1 || t + a2 * y <= crit3) {
        x++;
        dxt += d2xt;
        t += dxt;
      } else if (t - a2 * y > crit2) {
        y--;
        dyt += d2yt;
        t += dyt;
      } else {
        x++;
        dxt += d2xt;
        t += dxt;
        y--;
        dyt += d2yt;
        t += dyt;
      }
    }
  }


  // Arc drawn using modified Alois Zingl's algorith, which is a modified Bresenham's algorithm
  template <typename TPreparePixel, typename TRawSetPixel>
  void genericDrawArc(Rect const & rect, Rect & updateRect, TPreparePixel preparePixel, TRawSetPixel rawSetPixel)
  {
    auto pattern = preparePixel(getActualPenColor());

    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    const int centerX = paintState().position.X;
    const int centerY = paintState().position.Y;

    LineInfo startInfo = LineInfo(centerX, centerY, rect.X1, rect.Y1);
    LineInfo endInfo = LineInfo(centerX, centerY, rect.X2, rect.Y2);

    const int radius = startInfo.length();
    int r = radius;
    QuadrantInfo quadrants[4] = {
      QuadrantInfo(0, startInfo, endInfo),
      QuadrantInfo(1, startInfo, endInfo),
      QuadrantInfo(2, startInfo, endInfo),
      QuadrantInfo(3, startInfo, endInfo)
    };

    // Simplistic updateRect, using whole bounding box of circle
    updateRect = updateRect.merge(Rect(centerX - radius, centerY - radius, centerX + radius, centerY + radius));

    int x = -r;
    int y = 0;
    int err = 2 - 2 * r;
    do {
      if (quadrantContainsArcPixel(quadrants[0], startInfo, endInfo, y, x)) {
        if (centerX + y >= clipX1 && centerX + y <= clipX2 && centerY + x >= clipY1 && centerY + x <= clipY2)
          rawSetPixel(centerX + y, centerY + x, pattern);
      }
      if (quadrantContainsArcPixel(quadrants[1], startInfo, endInfo, x, -y)) {
        if (centerX + x >= clipX1 && centerX + x <= clipX2 && centerY - y >= clipY1 && centerY - y <= clipY2)
          rawSetPixel(centerX + x, centerY - y, pattern);
      }
      if (quadrantContainsArcPixel(quadrants[2], startInfo, endInfo, -y, -x)) {
        if (centerX - y >= clipX1 && centerX - y <= clipX2 && centerY - x >= clipY1 && centerY - x <= clipY2)
          rawSetPixel(centerX - y, centerY - x, pattern);
      }
      if (quadrantContainsArcPixel(quadrants[3], startInfo, endInfo, -x, y)) {
        if (centerX - x >= clipX1 && centerX - x <= clipX2 && centerY + y >= clipY1 && centerY + y <= clipY2)
          rawSetPixel(centerX - x, centerY + y, pattern);
      }
      r = err;
      if (r <= y) {
        err += ++y*2+1;           /* e_xy+e_y < 0 */
      }
      if (r > x || err > y) {
        err += ++x*2+1;
      }/* e_xy+e_x > 0 or no 2nd y-step */
    } while (x < 0);
  }


  // Segment drawn using modified Alois Zingl's algorith, which is a modified Bresenham's algorithm
  template <typename TPreparePixel, typename TRawFillRow>
  void genericFillSegment(Rect const & rect, Rect & updateRect, TPreparePixel preparePixel, TRawFillRow rawFillRow)
  {
    auto pattern = preparePixel(getActualBrushColor());

    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    const int centerX = paintState().position.X;
    const int centerY = paintState().position.Y;

    LineInfo startInfo = LineInfo(centerX, centerY, rect.X1, rect.Y1);
    LineInfo endInfo = LineInfo(centerX, centerY, rect.X2, rect.Y2);
    const int radius = startInfo.length();
    LineInfo endLine = endInfo.walkDistance(radius);

    LineInfo chordDeltaInfo = LineInfo(startInfo.deltaX, startInfo.deltaY, endLine.deltaX, endLine.deltaY, 0, 0);
    chordDeltaInfo.sortByY();
    // get chord mid-point
    const int chordMidX = (startInfo.deltaX + endLine.deltaX) / 2;
    const int chordMidY = (startInfo.deltaY + endLine.deltaY) / 2;
    const int chordQuadrant = getCircleQuadrant(chordMidX, chordMidY);

    QuadrantInfo quadrants[4] = {
      QuadrantInfo(0, startInfo, endLine, chordQuadrant),
      QuadrantInfo(1, startInfo, endLine, chordQuadrant),
      QuadrantInfo(2, startInfo, endLine, chordQuadrant),
      QuadrantInfo(3, startInfo, endLine, chordQuadrant)
    };

    // Simplistic updateRect, using whole bounding box of circle
    updateRect = updateRect.merge(Rect(centerX - radius, centerY - radius, centerX + radius, centerY + radius));

    int r = radius;
    int x = 0;
    int y = -r;
    int err = 2 - 2*r;
    int minX = 999999;
    int maxX = -999999;
    chordDeltaInfo.newRowCheck(y);

    std::function<void(int, int, int)> drawRow = [&rawFillRow, &clipX1, &clipX2, &pattern] (int row, int minX, int maxX) {
      if (minX <= clipX2 && maxX >= clipX1) {
        const int X1 = iclamp(minX, clipX1, clipX2);
        const int X2 = iclamp(maxX, clipX1, clipX2);
        rawFillRow(row, X1, X2, pattern);
      }
    };

    std::function<void()> finishRow = [&minX, &maxX, &y, &err, &clipX1, &clipX2, &clipY1, &clipY2, &chordDeltaInfo, &centerX, &centerY, &drawRow] () {
      const int row = centerY + y;
      if (minX <= maxX && row >= clipY1 && row <= clipY2) {
        if (chordDeltaInfo.hasPixels) {
          drawRow(row, centerX + imin(minX, chordDeltaInfo.minX), centerX + imax(maxX, chordDeltaInfo.maxX));
        } else {
          drawRow(row, centerX + minX, centerX + maxX);
        }
      }
      err += ++y*2+1;
      minX = 999999;
      maxX = -999999;
      chordDeltaInfo.newRowCheck(y);
    };

    auto minMaxQuadrant = [&startInfo, &endInfo, &minX, &maxX] (QuadrantInfo & quadrant, int x, int y) {
      if (quadrantContainsArcPixel(quadrant, startInfo, endInfo, x, y)) {
        if (x < minX)
          minX = x;
        if (x > maxX)
          maxX = x;
      }
    };

    // we can skip showing top half of circle if segment is entirely in the bottom half
    if (quadrants[0].showNothing && quadrants[1].showNothing) {
      // Skip top half
      y = 0;
    } else {
      do {
        minMaxQuadrant(quadrants[0], x, y);
        minMaxQuadrant(quadrants[1], -x, y);
        chordDeltaInfo.walkToY(y);

        r = err;
        if (r <= x) {
          x += 1;
          err += x*2+1;           /* e_xy+e_x < 0 */
        }
        if (r > y || err > x) {
          finishRow();
        }/* e_xy+e_y > 0 or no 2nd y-step */
      } while (y < 0);
    }

    // draw lower half - this time walking from x = -r to x = 0
    if (quadrants[2].showNothing && quadrants[3].showNothing) {
      // skip the bottom
      y = radius;
    } else {
      r = radius;
      x = -radius;
      y = 0;
      err = 2 - 2*r; /* II. Quadrant */ 
      do {
        minMaxQuadrant(quadrants[2], x, y);
        minMaxQuadrant(quadrants[3], -x, y);
        chordDeltaInfo.walkToY(y);

        r = err;
        if (r <= y) {
          finishRow();
        }
        if (r > x || err > y) {
          err += ++x*2+1;
        }/* e_xy+e_x > 0 or no 2nd y-step */
      } while (x < 0);
      finishRow();
    }
  }


  // Sector drawn using modified Alois Zingl's algorith, which is a modified Bresenham's algorithm
  template <typename TPreparePixel, typename TRawFillRow>
  void genericFillSector(Rect const & rect, Rect & updateRect, TPreparePixel preparePixel, TRawFillRow rawFillRow)
  {
    auto pattern = preparePixel(getActualBrushColor());

    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    const int centerX = paintState().position.X;
    const int centerY = paintState().position.Y;

    LineInfo startInfo = LineInfo(centerX, centerY, rect.X1, rect.Y1);
    LineInfo endInfo = LineInfo(centerX, centerY, rect.X2, rect.Y2);
    const int radius = startInfo.length();

    QuadrantInfo quadrants[4] = {
      QuadrantInfo(0, startInfo, endInfo),
      QuadrantInfo(1, startInfo, endInfo),
      QuadrantInfo(2, startInfo, endInfo),
      QuadrantInfo(3, startInfo, endInfo)
    };

    LineInfo startLine = LineInfo(0, 0, startInfo.deltaX, startInfo.deltaY);
    LineInfo endWalked = endInfo.walkDistance(radius);
    LineInfo endLine = LineInfo(0, 0, endWalked.deltaX, endWalked.deltaY);
    const bool startLeftmost = startLine.deltaX < endLine.deltaX;
    startLine.sortByY();
    endLine.sortByY();

    // Simplistic updateRect, using whole bounding box of circle
    updateRect = updateRect.merge(Rect(centerX - radius, centerY - radius, centerX + radius, centerY + radius));

    int r = radius;
    int x = 0;
    int y = -r;
    int err = 2 - 2*r;
    int lMinX = 999999;
    int lMaxX = -999999;
    int rMinX = 999999;
    int rMaxX = -999999;
    startLine.newRowCheck(y);
    endLine.newRowCheck(y);
    bool shownLeftEdge = false;
    bool shownRightEdge = false;
    bool shownStartLine = false;
    bool shownEndLine = false;

    std::function<void(int, int, int)> drawRow = [&rawFillRow, &clipX1, &clipX2, &pattern] (int row, int minX, int maxX) {
      if (minX <= clipX2 && maxX >= clipX1) {
        const int X1 = iclamp(minX, clipX1, clipX2);
        const int X2 = iclamp(maxX, clipX1, clipX2);
        rawFillRow(row, X1, X2, pattern);
      }
    };

    // finishRow will draw a row if it's inside the clipping rectangle, and advance to the next row
    // for drawing, the most important line is the circle outline
    // start/end lines will only be drawn if they are inside the outline
    std::function<void(bool const)> finishRow = [
      &startLeftmost, &shownLeftEdge, &shownRightEdge, &shownStartLine, &shownEndLine,
      &lMinX, &lMaxX, &rMinX, &rMaxX, &y, &err, 
      &clipX1, &clipX2, &clipY1, &clipY2, &startLine, &endLine, 
      &centerX, &centerY, &pattern, &drawRow
    ] (bool const upperHalf) {
      int const row = centerY + y;
      bool const hasLeftEdge = lMinX <= 0;
      bool const hasRightEdge = rMaxX >= 0;
      std::vector<int> rowPixels;

      if (row >= clipY1 && row <= clipY2 && (hasLeftEdge || hasRightEdge || startLine.hasPixels || endLine.hasPixels)) {
        if (hasLeftEdge) {
          rowPixels.push_back(lMinX);
        }

        bool hasStartLine = startLine.hasPixels;
        bool hasEndLine = endLine.hasPixels;        

        // keep track of whether we've shown the left or right edge of circle
        shownLeftEdge = shownLeftEdge || hasLeftEdge;
        shownRightEdge = shownRightEdge || hasRightEdge;

        // work out if we should _really_ be drawing start/end lines
        // lines can be slightly too long owing to integer rounding issues
        if (upperHalf) {
          // in the upper half, we need to prevent start/end lines from being drawn if they are outside the circle
          // so for new lines only we check to see if there is an edge on that side of the circle
          if (hasStartLine && !shownStartLine) {
            hasStartLine = startLine.x < 0 ? hasLeftEdge : hasRightEdge;
          }
          if (hasEndLine && !shownEndLine) {
            hasEndLine = endLine.x < 0 ? hasLeftEdge : hasRightEdge;
          }
        } else {
          // in the lower half we need to stop drawing start/end lines if they are outside the circle
          // care must be taken tho to ensure we draw lines that just fill between start and end lines
          // so this means stopping lines when corresponding edge had been shown and no longer exists
          if (shownLeftEdge && !hasLeftEdge) {
            if (hasEndLine && endLine.x < 0 && (!hasRightEdge || !startLeftmost)) {
              hasEndLine = false;
            }
            if (hasStartLine && startLine.x < 0 && (!hasRightEdge || startLeftmost)) {
              hasStartLine = false;
            }
          }
          
          if (shownRightEdge && !hasRightEdge) {
            if (hasEndLine && startLeftmost && endLine.x > 0) {
              hasEndLine = false;
            }
            if (hasStartLine && !startLeftmost && startLine.x > 0) {
              hasStartLine = false;
            }
          }
        }
        
        shownStartLine = shownStartLine || hasStartLine;
        shownEndLine = shownEndLine || hasEndLine;

        if (hasStartLine) {
          if (hasEndLine) {
            // if we're in the upper half, then having start line leftmost means we have two parts to the row
            // or in the bottom half, rightmost means we have two parts to the row
            auto const firstLine = startLeftmost ? startLine : endLine;
            auto const secondLine = startLeftmost ? endLine : startLine;

            if (hasStartLine && hasEndLine && hasLeftEdge && hasRightEdge && (startLeftmost ^ !upperHalf)) {
              // there are two parts to the row
              // we may have started a line, but not finished it
              if (!hasLeftEdge) {
                // first line
                rowPixels.push_back(firstLine.minX);
              }
              // close off first line
              rowPixels.push_back(firstLine.maxX);

              // then push left edge of right-most line
              rowPixels.push_back(hasRightEdge ? imin(secondLine.minX, rMinX) : secondLine.minX);
              rowPixels.push_back(hasRightEdge ? rMaxX : secondLine.maxX);
            } else {
              // there is only one part to the row
              // which should be first line to second line
              if (!hasLeftEdge) {
                rowPixels.push_back(firstLine.minX);
              }
              rowPixels.push_back(hasRightEdge ? rMaxX : secondLine.maxX);
            }
          } else {
            // have start line, but no end line
            if (!hasLeftEdge) {
              rowPixels.push_back(startLine.minX);
            }

            rowPixels.push_back(hasRightEdge ? rMaxX : startLine.maxX);
          }
        } else if (hasEndLine) {
          if (!hasLeftEdge) {
            rowPixels.push_back(endLine.minX);
          }
          rowPixels.push_back(hasRightEdge ? rMaxX : endLine.maxX);
        } else {
          // no start or end lines
          if (hasRightEdge) {
            if (!hasLeftEdge) {
              rowPixels.push_back(rMinX);
            }
            rowPixels.push_back(rMaxX);
          }
        }

        if (rowPixels.size() % 2 == 1) {
          if (hasLeftEdge) {
            rowPixels.push_back(lMaxX);
          } else {
            // double up on last pixel just to make sure we have a "line"
            // in principle this shouldn't be possible, but just in case
            rowPixels.push_back(rowPixels[rowPixels.size() - 1]);
          }
        }
        
        if (rowPixels.size() >= 2) {
          if (rowPixels.size() == 4 && rowPixels[1] == rowPixels[2]) {
            // conjoining lines, so just draw one line
            drawRow(row, centerX + rowPixels[0], centerX + rowPixels[3]);
          } else {
            drawRow(row, centerX + rowPixels[0], centerX + rowPixels[1]);
            if (rowPixels.size() == 4) {
              drawRow(row, centerX + rowPixels[2], centerX + rowPixels[3]);
            }
          }
        }
      }

      err += ++y*2+1;
      lMinX = 999999;
      lMaxX = -999999;
      rMinX = 999999;
      rMaxX = -999999;
      startLine.newRowCheck(y);
      endLine.newRowCheck(y);
    };

    // we can skip showing top half of circle if segment is entirely in the bottom half
    if (quadrants[0].showNothing && quadrants[1].showNothing) {
      // Skip top half
      y = 0;
      startLine.newRowCheck(y);
      endLine.newRowCheck(y);
    } else {
      do {
        if (quadrantContainsArcPixel(quadrants[0], startInfo, endInfo, x, y)) {
          rMinX = imin(rMinX, x);
          rMaxX = imax(rMaxX, x);
        }
        if (quadrantContainsArcPixel(quadrants[1], startInfo, endInfo, -x, y)) {
          lMinX = imin(lMinX, -x);
          lMaxX = imax(lMaxX, -x);
        }
        startLine.walkToY(y);
        endLine.walkToY(y);

        r = err;
        if (r <= x) {
          x += 1;
          err += x*2+1;           /* e_xy+e_x < 0 */
        }
        if (r > y || err > x) {
          finishRow(true);
        }/* e_xy+e_y > 0 or no 2nd y-step */
      } while (y < 0);
    }

    shownLeftEdge = false;
    shownRightEdge = false;

    // draw lower half - this time walking from x = -r to x = 0
    if (quadrants[2].showNothing && quadrants[3].showNothing) {
      // skip the bottom
      // y = radius;
      finishRow(true);
    } else {
      r = radius;
      x = -radius;
      y = 0;
      err = 2 - 2*r; /* II. Quadrant */ 
      do {
        if (quadrantContainsArcPixel(quadrants[2], startInfo, endInfo, x, y)) {
          lMinX = imin(lMinX, x);
          lMaxX = imax(lMaxX, x);
        }
        if (quadrantContainsArcPixel(quadrants[3], startInfo, endInfo, -x, y)) {
          rMinX = imin(rMinX, -x);
          rMaxX = imax(rMaxX, -x);
        }
        startLine.walkToY(y);
        endLine.walkToY(y);

        r = err;
        if (r <= y) {
          finishRow(false);
        }
        if (r > x || err > y) {
          err += ++x*2+1;
        }/* e_xy+e_x > 0 or no 2nd y-step */
      } while (x < 0);
      finishRow(false);
    }
  }


  template <typename TPreparePixel, typename TRawGetRow, typename TRawSetPixelInRow>
  void genericDrawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect, TPreparePixel preparePixel, TRawGetRow rawGetRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    if (!glyphOptions.bold && !glyphOptions.italic && !glyphOptions.blank && !glyphOptions.underline && !glyphOptions.doubleWidth && glyph.width <= 32)
      genericDrawGlyph_light(glyph, glyphOptions, penColor, brushColor, updateRect, preparePixel, rawGetRow, rawSetPixelInRow);
    else
      genericDrawGlyph_full(glyph, glyphOptions, penColor, brushColor, updateRect, preparePixel, rawGetRow, rawSetPixelInRow);
  }


  // TODO: Italic doesn't work well when clipping rect is specified
  template <typename TPreparePixel, typename TRawGetRow, typename TRawSetPixelInRow>
  void genericDrawGlyph_full(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect, TPreparePixel preparePixel, TRawGetRow rawGetRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    const int origX = paintState().origin.X;
    const int origY = paintState().origin.Y;

    const int glyphX = glyph.X + origX;
    const int glyphY = glyph.Y + origY;

    if (glyphX > clipX2 || glyphY > clipY2)
      return;

    int16_t glyphWidth        = glyph.width;
    int16_t glyphHeight       = glyph.height;
    uint8_t const * glyphData = glyph.data;
    int16_t glyphWidthByte    = (glyphWidth + 7) / 8;
    int16_t glyphSize         = glyphHeight * glyphWidthByte;

    bool fillBackground = glyphOptions.fillBackground;
    bool bold           = glyphOptions.bold;
    bool italic         = glyphOptions.italic;
    bool blank          = glyphOptions.blank;
    bool underline      = glyphOptions.underline;
    int doubleWidth     = glyphOptions.doubleWidth;

    // modify glyph to handle top half and bottom half double height
    // doubleWidth = 1 is handled directly inside drawing routine
    if (doubleWidth > 1) {
      uint8_t * newGlyphData = (uint8_t*) alloca(glyphSize);
      // doubling top-half or doubling bottom-half?
      int offset = (doubleWidth == 2 ? 0 : (glyphHeight >> 1));
      for (int y = 0; y < glyphHeight ; ++y)
        for (int x = 0; x < glyphWidthByte; ++x)
          newGlyphData[x + y * glyphWidthByte] = glyphData[x + (offset + (y >> 1)) * glyphWidthByte];
      glyphData = newGlyphData;
    }

    // a very simple and ugly skew (italic) implementation!
    int skewAdder = 0, skewH1 = 0, skewH2 = 0;
    if (italic) {
      skewAdder = 2;
      skewH1 = glyphHeight / 3;
      skewH2 = skewH1 * 2;
    }

    int16_t X1 = 0;
    int16_t XCount = glyphWidth;
    int16_t destX = glyphX;

    if (destX < clipX1) {
      X1 = (clipX1 - destX) / (doubleWidth ? 2 : 1);
      destX = clipX1;
    }
    if (X1 >= glyphWidth)
      return;

    if (destX + XCount + skewAdder > clipX2 + 1)
      XCount = clipX2 + 1 - destX - skewAdder;
    if (X1 + XCount > glyphWidth)
      XCount = glyphWidth - X1;

    int16_t Y1 = 0;
    int16_t YCount = glyphHeight;
    int destY = glyphY;

    if (destY < clipY1) {
      Y1 = clipY1 - destY;
      destY = clipY1;
    }
    if (Y1 >= glyphHeight)
      return;

    if (destY + YCount > clipY2 + 1)
      YCount = clipY2 + 1 - destY;
    if (Y1 + YCount > glyphHeight)
      YCount = glyphHeight - Y1;

    updateRect = updateRect.merge(Rect(destX, destY, destX + XCount + skewAdder - 1, destY + YCount - 1));

    if (glyphOptions.invert ^ paintState().paintOptions.swapFGBG)
      tswap(penColor, brushColor);

    // a very simple and ugly reduce luminosity (faint) implementation!
    if (glyphOptions.reduceLuminosity) {
      if (penColor.R > 128) penColor.R = 128;
      if (penColor.G > 128) penColor.G = 128;
      if (penColor.B > 128) penColor.B = 128;
    }

    auto penPattern   = preparePixel(penColor);
    auto brushPattern = preparePixel(brushColor);
    auto boldPattern  = bold ? preparePixel(RGB888(penColor.R / 2 + 1,
                                                   penColor.G / 2 + 1,
                                                   penColor.B / 2 + 1))
                             : preparePixel(RGB888(0, 0, 0));

    for (int y = Y1; y < Y1 + YCount; ++y, ++destY) {

      // true if previous pixel has been set
      bool prevSet = false;

      auto dstrow = rawGetRow(destY);
      auto srcrow = glyphData + y * glyphWidthByte;

      if (underline && y == glyphHeight - FABGLIB_UNDERLINE_POSITION - 1) {

        for (int x = X1, adestX = destX + skewAdder; x < X1 + XCount && adestX <= clipX2; ++x, ++adestX) {
          rawSetPixelInRow(dstrow, adestX, blank ? brushPattern : penPattern);
          if (doubleWidth) {
            ++adestX;
            if (adestX > clipX2)
              break;
            rawSetPixelInRow(dstrow, adestX, blank ? brushPattern : penPattern);
          }
        }

      } else {

        for (int x = X1, adestX = destX + skewAdder; x < X1 + XCount && adestX <= clipX2; ++x, ++adestX) {
          if ((srcrow[x >> 3] << (x & 7)) & 0x80 && !blank) {
            rawSetPixelInRow(dstrow, adestX, penPattern);
            prevSet = true;
          } else if (bold && prevSet) {
            rawSetPixelInRow(dstrow, adestX, boldPattern);
            prevSet = false;
          } else if (fillBackground) {
            rawSetPixelInRow(dstrow, adestX, brushPattern);
            prevSet = false;
          } else {
            prevSet = false;
          }
          if (doubleWidth) {
            ++adestX;
            if (adestX > clipX2)
              break;
            if (fillBackground)
              rawSetPixelInRow(dstrow, adestX, prevSet ? penPattern : brushPattern);
            else if (prevSet)
              rawSetPixelInRow(dstrow, adestX, penPattern);
          }
        }

      }

      if (italic && (y == skewH1 || y == skewH2))
        --skewAdder;

    }
  }


  // assume:
  //   glyph.width <= 32
  //   glyphOptions.fillBackground = 0 or 1
  //   glyphOptions.invert : 0 or 1
  //   glyphOptions.reduceLuminosity: 0 or 1
  //   glyphOptions.... others = 0
  //   paintState().paintOptions.swapFGBG: 0 or 1
  template <typename TPreparePixel, typename TRawGetRow, typename TRawSetPixelInRow>
  void genericDrawGlyph_light(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect, TPreparePixel preparePixel, TRawGetRow rawGetRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    const int origX = paintState().origin.X;
    const int origY = paintState().origin.Y;

    const int glyphX = glyph.X + origX;
    const int glyphY = glyph.Y + origY;

    if (glyphX > clipX2 || glyphY > clipY2)
      return;

    int16_t glyphWidth        = glyph.width;
    int16_t glyphHeight       = glyph.height;
    uint8_t const * glyphData = glyph.data;
    int16_t glyphWidthByte    = (glyphWidth + 7) / 8;

    int16_t X1 = 0;
    int16_t XCount = glyphWidth;
    int16_t destX = glyphX;

    int16_t Y1 = 0;
    int16_t YCount = glyphHeight;
    int destY = glyphY;

    if (destX < clipX1) {
      X1 = clipX1 - destX;
      destX = clipX1;
    }
    if (X1 >= glyphWidth)
      return;

    if (destX + XCount > clipX2 + 1)
      XCount = clipX2 + 1 - destX;
    if (X1 + XCount > glyphWidth)
      XCount = glyphWidth - X1;

    if (destY < clipY1) {
      Y1 = clipY1 - destY;
      destY = clipY1;
    }
    if (Y1 >= glyphHeight)
      return;

    if (destY + YCount > clipY2 + 1)
      YCount = clipY2 + 1 - destY;
    if (Y1 + YCount > glyphHeight)
      YCount = glyphHeight - Y1;

    updateRect = updateRect.merge(Rect(destX, destY, destX + XCount - 1, destY + YCount - 1));

    if (glyphOptions.invert ^ paintState().paintOptions.swapFGBG)
      tswap(penColor, brushColor);

    // a very simple and ugly reduce luminosity (faint) implementation!
    if (glyphOptions.reduceLuminosity) {
      if (penColor.R > 128) penColor.R = 128;
      if (penColor.G > 128) penColor.G = 128;
      if (penColor.B > 128) penColor.B = 128;
    }

    bool fillBackground = glyphOptions.fillBackground;

    auto penPattern   = preparePixel(penColor);
    auto brushPattern = preparePixel(brushColor);

    for (int y = Y1; y < Y1 + YCount; ++y, ++destY) {
      auto dstrow = rawGetRow(destY);
      uint8_t const * srcrow = glyphData + y * glyphWidthByte;

      uint32_t src = (srcrow[0] << 24) | (srcrow[1] << 16) | (srcrow[2] << 8) | (srcrow[3]);
      src <<= X1;
      if (fillBackground) {
        // filled background
        for (int x = X1, adestX = destX; x < X1 + XCount; ++x, ++adestX, src <<= 1)
          rawSetPixelInRow(dstrow, adestX, src & 0x80000000 ? penPattern : brushPattern);
      } else {
        // transparent background
        for (int x = X1, adestX = destX; x < X1 + XCount; ++x, ++adestX, src <<= 1)
          if (src & 0x80000000)
            rawSetPixelInRow(dstrow, adestX, penPattern);
      }
    }
  }


  template <typename TRawInvertRow>
  void genericInvertRect(Rect const & rect, Rect & updateRect, TRawInvertRow rawInvertRow)
  {
    const int origX = paintState().origin.X;
    const int origY = paintState().origin.Y;

    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    const int x1 = iclamp(rect.X1 + origX, clipX1, clipX2);
    const int y1 = iclamp(rect.Y1 + origY, clipY1, clipY2);
    const int x2 = iclamp(rect.X2 + origX, clipX1, clipX2);
    const int y2 = iclamp(rect.Y2 + origY, clipY1, clipY2);

    updateRect = updateRect.merge(Rect(x1, y1, x2, y2));

    for (int y = y1; y <= y2; ++y)
      rawInvertRow(y, x1, x2);
  }


  template <typename TPreparePixel, typename TRawGetRow, typename TRawGetPixelInRow, typename TRawSetPixelInRow>
  void genericSwapFGBG(Rect const & rect, Rect & updateRect, TPreparePixel preparePixel, TRawGetRow rawGetRow, TRawGetPixelInRow rawGetPixelInRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    auto penPattern   = preparePixel(paintState().penColor);
    auto brushPattern = preparePixel(paintState().brushColor);

    int origX = paintState().origin.X;
    int origY = paintState().origin.Y;

    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    const int x1 = iclamp(rect.X1 + origX, clipX1, clipX2);
    const int y1 = iclamp(rect.Y1 + origY, clipY1, clipY2);
    const int x2 = iclamp(rect.X2 + origX, clipX1, clipX2);
    const int y2 = iclamp(rect.Y2 + origY, clipY1, clipY2);

    updateRect = updateRect.merge(Rect(x1, y1, x2, y2));

    for (int y = y1; y <= y2; ++y) {
      auto row = rawGetRow(y);
      for (int x = x1; x <= x2; ++x) {
        auto px = rawGetPixelInRow(row, x);
        if (px == penPattern)
          rawSetPixelInRow(row, x, brushPattern);
        else if (px == brushPattern)
          rawSetPixelInRow(row, x, penPattern);
      }
    }
  }


  template <typename TRawGetRow, typename TRawGetPixelInRow, typename TRawSetPixelInRow>
  void genericCopyRect(Rect const & source, Rect & updateRect, TRawGetRow rawGetRow, TRawGetPixelInRow rawGetPixelInRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    const int clipX1 = paintState().absClippingRect.X1;
    const int clipY1 = paintState().absClippingRect.Y1;
    const int clipX2 = paintState().absClippingRect.X2;
    const int clipY2 = paintState().absClippingRect.Y2;

    int origX = paintState().origin.X;
    int origY = paintState().origin.Y;

    int srcX = source.X1 + origX;
    int srcY = source.Y1 + origY;
    int width  = source.X2 - source.X1 + 1;
    int height = source.Y2 - source.Y1 + 1;
    int destX = paintState().position.X;
    int destY = paintState().position.Y;
    int deltaX = destX - srcX;
    int deltaY = destY - srcY;

    int incX = deltaX < 0 ? 1 : -1;
    int incY = deltaY < 0 ? 1 : -1;

    int startX = deltaX < 0 ? destX : destX + width - 1;
    int startY = deltaY < 0 ? destY : destY + height - 1;

    updateRect = updateRect.merge(Rect(srcX, srcY, srcX + width - 1, srcY + height - 1));
    updateRect = updateRect.merge(Rect(destX, destY, destX + width - 1, destY + height - 1));

    for (int y = startY, i = 0; i < height; y += incY, ++i) {
      if (y >= clipY1 && y <= clipY2) {
        auto srcRow = rawGetRow(y - deltaY);
        auto dstRow = rawGetRow(y);
        for (int x = startX, j = 0; j < width; x += incX, ++j) {
          if (x >= clipX1 && x <= clipX2)
            rawSetPixelInRow(dstRow, x, rawGetPixelInRow(srcRow, x - deltaX));
        }
      }
    }
  }


  template <typename TRawGetRow, typename TRawSetPixelInRow, typename TDataType>
  void genericRawDrawBitmap_Native(int destX, int destY, TDataType * data, int width, int X1, int Y1, int XCount, int YCount,
                                   TRawGetRow rawGetRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    const int yEnd = Y1 + YCount;
    const int xEnd = X1 + XCount;
    for (int y = Y1; y < yEnd; ++y, ++destY) {
      auto dstrow = rawGetRow(destY);
      auto src = data + y * width + X1;
      for (int x = X1, adestX = destX; x < xEnd; ++x, ++adestX, ++src)
        rawSetPixelInRow(dstrow, adestX, *src);
    }
  }



  template <typename TRawGetRow, typename TRawGetPixelInRow, typename TRawSetPixelInRow, typename TBackground>
  void genericRawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, TBackground * saveBackground, int X1, int Y1, int XCount, int YCount,
                                 TRawGetRow rawGetRow, TRawGetPixelInRow rawGetPixelInRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    const int width = bitmap->width;
    const int yEnd  = Y1 + YCount;
    const int xEnd  = X1 + XCount;
    auto data = bitmap->data;
    const int rowlen = (bitmap->width + 7) / 8;

    if (saveBackground) {

      // save background and draw the bitmap
      for (int y = Y1; y < yEnd; ++y, ++destY) {
        auto dstrow = rawGetRow(destY);
        auto savePx = saveBackground + y * width + X1;
        auto src = data + y * rowlen;
        for (int x = X1, adestX = destX; x < xEnd; ++x, ++adestX, ++savePx) {
          *savePx = rawGetPixelInRow(dstrow, adestX);
          if ((src[x >> 3] << (x & 7)) & 0x80)
            rawSetPixelInRow(dstrow, adestX);
        }
      }

    } else {

      // just draw the bitmap
      for (int y = Y1; y < yEnd; ++y, ++destY) {
        auto dstrow = rawGetRow(destY);
        auto src = data + y * rowlen;
        for (int x = X1, adestX = destX; x < xEnd; ++x, ++adestX) {
          if ((src[x >> 3] << (x & 7)) & 0x80)
            rawSetPixelInRow(dstrow, adestX);
        }
      }

    }
  }


  template <typename TRawGetRow, typename TRawGetPixelInRow, typename TRawSetPixelInRow, typename TBackground>
  void genericRawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, TBackground * saveBackground, int X1, int Y1, int XCount, int YCount,
                                     TRawGetRow rawGetRow, TRawGetPixelInRow rawGetPixelInRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    const int width  = bitmap->width;
    const int yEnd   = Y1 + YCount;
    const int xEnd   = X1 + XCount;
    auto data = bitmap->data;

    if (saveBackground) {

      // save background and draw the bitmap
      for (int y = Y1; y < yEnd; ++y, ++destY) {
        auto dstrow = rawGetRow(destY);
        auto savePx = saveBackground + y * width + X1;
        auto src = data + y * width + X1;
        for (int x = X1, adestX = destX; x < xEnd; ++x, ++adestX, ++savePx, ++src) {
          *savePx = rawGetPixelInRow(dstrow, adestX);
          if (*src & 0xc0)  // alpha > 0 ?
            rawSetPixelInRow(dstrow, adestX, *src);
        }
      }

    } else {

      // just draw the bitmap
      for (int y = Y1; y < yEnd; ++y, ++destY) {
        auto dstrow = rawGetRow(destY);
        auto src = data + y * width + X1;
        for (int x = X1, adestX = destX; x < xEnd; ++x, ++adestX, ++src) {
          if (*src & 0xc0)  // alpha > 0 ?
            rawSetPixelInRow(dstrow, adestX, *src);
        }
      }

    }
  }


  template <typename TRawGetRow, typename TRawGetPixelInRow, typename TRawSetPixelInRow, typename TBackground>
  void genericRawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, TBackground * saveBackground, int X1, int Y1, int XCount, int YCount,
                                     TRawGetRow rawGetRow, TRawGetPixelInRow rawGetPixelInRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    const int width = bitmap->width;
    const int yEnd  = Y1 + YCount;
    const int xEnd  = X1 + XCount;
    auto data = (RGBA8888 const *) bitmap->data;

    if (saveBackground) {

      // save background and draw the bitmap
      for (int y = Y1; y < yEnd; ++y, ++destY) {
        auto dstrow = rawGetRow(destY);
        auto savePx = saveBackground + y * width + X1;
        auto src = data + y * width + X1;
        for (int x = X1, adestX = destX; x < xEnd; ++x, ++adestX, ++savePx, ++src) {
          *savePx = rawGetPixelInRow(dstrow, adestX);
          if (src->A)
            rawSetPixelInRow(dstrow, adestX, *src);
        }
      }

    } else {

      // just draw the bitmap
      for (int y = Y1; y < yEnd; ++y, ++destY) {
        auto dstrow = rawGetRow(destY);
        auto src = data + y * width + X1;
        for (int x = X1, adestX = destX; x < xEnd; ++x, ++adestX, ++src) {
          if (src->A)
            rawSetPixelInRow(dstrow, adestX, *src);
        }
      }

    }
  }


  template <typename TRawGetRow, typename TRawGetPixelInRow, typename TBuffer>
  void genericRawCopyToBitmap(int srcX, int srcY, int width, TBuffer * saveBuffer, int X1, int Y1, int XCount, int YCount,
                            TRawGetRow rawGetRow, TRawGetPixelInRow rawGetPixelInRow)
  {
    const int yEnd  = Y1 + YCount;
    const int xEnd  = X1 + XCount;

    // save screen area to buffer
    for (int y = Y1; y < yEnd; ++y, ++srcY) {
      auto srcrow = rawGetRow(srcY);
      auto savePx = saveBuffer + y * width + X1;
      for (int x = X1, asrcX = srcX; x < xEnd; ++x, ++asrcX, ++savePx) {
        *savePx = rawGetPixelInRow(srcrow, asrcX);
      }
    }
  }


  template <typename TRawGetRow, /*typename TRawGetPixelInRow,*/ typename TRawSetPixelInRow /*, typename TBackground */>
  void genericRawDrawTransformedBitmap_Mask(int destX, int destY, Rect drawingRect, Bitmap const * bitmap, const float * invMatrix,
                                     TRawGetRow rawGetRow, /* TRawGetPixelInRow rawGetPixelInRow, */ TRawSetPixelInRow rawSetPixelInRow)
  {
    // transformed bitmap plot works as follows:
    // 1. for each pixel in the destination rectangle, calculate the corresponding pixel in the source bitmap
    // 2. if the source pixel is within source, and is not transparent, plot it to the destination
    //
    // drawingRect should be all on-screen, pre-clipped, but moved to -destX, -destY
    float pos[3] = {0.0f, 0.0f, 1.0f};
    float srcPos[3] = {0.0f, 0.0f, 1.0f};
    float maxX = drawingRect.X2;
    float maxY = drawingRect.Y2;
    auto data = bitmap->data;
    const int rowlen = (bitmap->width + 7) / 8;
    const float widthF = (float)bitmap->width;
    const float heightF = (float)bitmap->height;
 
    for (float y = drawingRect.Y1; y < maxY; y++) {
      for (float x = drawingRect.X1; x < maxX; x++) {
        // calculate the source pixel
        pos[0] = x;
        pos[1] = y;
        dspm_mult_3x3x1_f32(invMatrix, pos, srcPos);

        if (srcPos[0] < 0.0f || srcPos[0] >= widthF || srcPos[1] < 0 || srcPos[1] >= heightF)
          continue;

        auto srcRow = data + (int)srcPos[1] * rowlen;
        int srcXint = (int)srcPos[0];
        if ((srcRow[srcXint >> 3] << (srcXint & 7)) & 0x80)
          rawSetPixelInRow(rawGetRow((int)y + destY), (int)x + destX);
      }
    }
  }


  template <typename TRawGetRow, /*typename TRawGetPixelInRow,*/ typename TRawSetPixelInRow /*, typename TBackground */>
  void genericRawDrawTransformedBitmap_RGBA2222(int destX, int destY, Rect drawingRect, Bitmap const * bitmap, const float * invMatrix,
                                     TRawGetRow rawGetRow, /* TRawGetPixelInRow rawGetPixelInRow, */ TRawSetPixelInRow rawSetPixelInRow)
  {
    // transformed bitmap plot works as follows:
    // 1. for each pixel in the destination rectangle, calculate the corresponding pixel in the source bitmap
    // 2. if the source pixel is within source, and is not transparent, plot it to the destination
    //
    // drawingRect should be all on-screen, pre-clipped, but moved to -destX, -destY
    float pos[3] = {0.0f, 0.0f, 1.0f};
    float srcPos[3] = {0.0f, 0.0f, 1.0f};
    float maxX = drawingRect.X2;
    float maxY = drawingRect.Y2;
    auto data = bitmap->data;
    const int width = bitmap->width;
    const float widthF = (float)width;
    const float heightF = (float)bitmap->height;
 
    for (float y = drawingRect.Y1; y < maxY; y++) {
      for (float x = drawingRect.X1; x < maxX; x++) {
        // calculate the source pixel
        pos[0] = x;
        pos[1] = y;
        dspm_mult_3x3x1_f32(invMatrix, pos, srcPos);

        if (srcPos[0] < 0.0f || srcPos[0] >= widthF || srcPos[1] < 0 || srcPos[1] >= heightF)
          continue;

        auto src = data + (int)srcPos[1] * width + (int)srcPos[0];
        if (*src & 0xc0)  // alpha > 0 ?
          rawSetPixelInRow(rawGetRow((int)y + destY), (int)x + destX, *src);
      }
    }
  }


  template <typename TRawGetRow, /*typename TRawGetPixelInRow,*/ typename TRawSetPixelInRow /*, typename TBackground */>
  void genericRawDrawTransformedBitmap_RGBA8888(int destX, int destY, Rect drawingRect, Bitmap const * bitmap, const float * invMatrix,
                                     TRawGetRow rawGetRow, /* TRawGetPixelInRow rawGetPixelInRow, */ TRawSetPixelInRow rawSetPixelInRow)
  {
    // transformed bitmap plot works as follows:
    // 1. for each pixel in the destination rectangle, calculate the corresponding pixel in the source bitmap
    // 2. if the source pixel is within source, and is not transparent, plot it to the destination
    //
    // drawingRect should be all on-screen, pre-clipped, but moved to -destX, -destY
    float pos[3] = {0.0f, 0.0f, 1.0f};
    float srcPos[3] = {0.0f, 0.0f, 1.0f};
    float maxX = drawingRect.X2;
    float maxY = drawingRect.Y2;
    auto data = (RGBA8888 const *) bitmap->data;
    const int width = bitmap->width;
    const float widthF = (float)width;
    const float heightF = (float)bitmap->height;
 
    for (float y = drawingRect.Y1; y < maxY; y++) {
      for (float x = drawingRect.X1; x < maxX; x++) {
        // calculate the source pixel
        pos[0] = x;
        pos[1] = y;
        dspm_mult_3x3x1_f32(invMatrix, pos, srcPos);

        if (srcPos[0] < 0.0f || srcPos[0] >= widthF || srcPos[1] < 0 || srcPos[1] >= heightF)
          continue;

        auto src = data + (int)srcPos[1] * width + (int)srcPos[0];
        if (src->A)
          rawSetPixelInRow(rawGetRow((int)y + destY),  (int)x + destX, *src);
      }
    }
  }


  // Scroll is done copying and filling rows
  // scroll < 0 -> scroll UP
  // scroll > 0 -> scroll DOWN
  template <typename TRawCopyRow, typename TRawFillRow>
  void genericVScroll(int scroll, Rect & updateRect,
                      TRawCopyRow rawCopyRow, TRawFillRow rawFillRow)
  {
    RGB888 color = getActualBrushColor();
    int Y1 = paintState().scrollingRegion.Y1;
    int Y2 = paintState().scrollingRegion.Y2;
    int X1 = paintState().scrollingRegion.X1;
    int X2 = paintState().scrollingRegion.X2;
    int height = Y2 - Y1 + 1;

    if (scroll < 0) {

      // scroll UP

      for (int i = 0; i < height + scroll; ++i) {
        // copy X1..X2 of (Y1 + i - scroll) to (Y1 + i)
        rawCopyRow(X1, X2, (Y1 + i - scroll), (Y1 + i));
      }
      // fill lower area with brush color
      for (int i = height + scroll; i < height; ++i)
        rawFillRow(Y1 + i, X1, X2, color);

    } else if (scroll > 0) {

      // scroll DOWN
      for (int i = height - scroll - 1; i >= 0; --i) {
        // copy X1..X2 of (Y1 + i) to (Y1 + i + scroll)
        rawCopyRow(X1, X2, (Y1 + i), (Y1 + i + scroll));
      }

      // fill upper area with brush color
      for (int i = 0; i < scroll; ++i)
        rawFillRow(Y1 + i, X1, X2, color);

    }
  }


  // scroll is done swapping rows and rows pointers
  // scroll < 0 -> scroll UP
  // scroll > 0 -> scroll DOWN
  template <typename TSwapRowsCopying, typename TSwapRowsPointers, typename TRawFillRow>
  void genericVScroll(int scroll, Rect & updateRect,
                      TSwapRowsCopying swapRowsCopying, TSwapRowsPointers swapRowsPointers, TRawFillRow rawFillRow)
  {
    RGB888 color = getActualBrushColor();
    const int Y1 = paintState().scrollingRegion.Y1;
    const int Y2 = paintState().scrollingRegion.Y2;
    const int X1 = paintState().scrollingRegion.X1;
    const int X2 = paintState().scrollingRegion.X2;
    const int height = Y2 - Y1 + 1;

    const int viewPortWidth = m_viewPortWidth;

    if (scroll < 0) {

      // scroll UP

      for (int i = 0; i < height + scroll; ++i) {

        // these are necessary to maintain invariate out of scrolling regions
        if (X1 > 0)
          swapRowsCopying(Y1 + i, Y1 + i - scroll, 0, X1 - 1);
        if (X2 < viewPortWidth - 1)
          swapRowsCopying(Y1 + i, Y1 + i - scroll, X2 + 1, viewPortWidth - 1);

        // swap scan lines
        swapRowsPointers(Y1 + i, Y1 + i - scroll);
      }

      // fill lower area with brush color
      for (int i = height + scroll; i < height; ++i)
        rawFillRow(Y1 + i, X1, X2, color);

    } else if (scroll > 0) {

      // scroll DOWN
      for (int i = height - scroll - 1; i >= 0; --i) {

        // these are necessary to maintain invariate out of scrolling regions
        if (X1 > 0)
          swapRowsCopying(Y1 + i, Y1 + i + scroll, 0, X1 - 1);
        if (X2 < viewPortWidth - 1)
          swapRowsCopying(Y1 + i, Y1 + i + scroll, X2 + 1, viewPortWidth - 1);

        // swap scan lines
        swapRowsPointers(Y1 + i, Y1 + i + scroll);
      }

      // fill upper area with brush color
      for (int i = 0; i < scroll; ++i)
        rawFillRow(Y1 + i, X1, X2, color);

    }
  }



  // Scroll is done copying and filling columns
  // scroll < 0 -> scroll LEFT
  // scroll > 0 -> scroll RIGHT
  template <typename TPreparePixel, typename TRawGetRow, typename TRawGetPixelInRow, typename TRawSetPixelInRow>
  void genericHScroll(int scroll, Rect & updateRect,
                      TPreparePixel preparePixel, TRawGetRow rawGetRow, TRawGetPixelInRow rawGetPixelInRow, TRawSetPixelInRow rawSetPixelInRow)
  {
    auto pattern = preparePixel(getActualBrushColor());

    int Y1 = paintState().scrollingRegion.Y1;
    int Y2 = paintState().scrollingRegion.Y2;
    int X1 = paintState().scrollingRegion.X1;
    int X2 = paintState().scrollingRegion.X2;

    if (scroll < 0) {
      // scroll left
      for (int y = Y1; y <= Y2; ++y) {
        auto row = rawGetRow(y);
        for (int x = X1; x <= X2 + scroll; ++x) {
          auto c = rawGetPixelInRow(row, x - scroll);
          rawSetPixelInRow(row, x, c);
        }
        // fill right area with brush color
        for (int x = X2 + 1 + scroll; x <= X2; ++x)
          rawSetPixelInRow(row, x, pattern);
      }
    } else if (scroll > 0) {
      // scroll right
      for (int y = Y1; y <= Y2; ++y) {
        auto row = rawGetRow(y);
        for (int x = X2 - scroll; x >= X1; --x) {
          auto c = rawGetPixelInRow(row, x);
          rawSetPixelInRow(row, x + scroll, c);
        }
        // fill left area with brush color
        for (int x = X1; x < X1 + scroll; ++x)
          rawSetPixelInRow(row, x, pattern);
      }
    }
  }

  PaintState & paintState() { return m_paintState; }

  virtual void setupDefaultPalette() = 0;

  uint8_t RGB888toPaletteIndex(RGB888 const & rgb) {
    return m_packedRGB222_to_PaletteIndex[RGB888toPackedRGB222(rgb)];
  }

  uint8_t RGB2222toPaletteIndex(uint8_t value) {
    return m_packedRGB222_to_PaletteIndex[value & 0b00111111];
  }

  uint8_t RGB8888toPaletteIndex(RGBA8888 value) {
    return RGB888toPaletteIndex(RGB888(value.R, value.G, value.B));
  }

  virtual int getPaletteSize() = 0;

  void updateAbsoluteClippingRect();

  RGB888 getActualPenColor();

  RGB888 getActualBrushColor();

  void * getSignalsForScanline(int scanline);

  // returns "static" version of m_viewPort
  uint8_t * sgetScanline(int y)                  { return (uint8_t*) m_viewPort[y]; }

  // Should be called after the palette is updated.
  void updateRGB2PaletteLUT();

  /**
   * @brief Creates a new palette (signal block) with a 16-bit ID, copying the default palette
   * 
   * @param paletteId ID of the new palette
   */
  bool createPalette(uint16_t paletteId);


  /**
   * @brief Deletes a palette (signal block) with a 16-bit ID
   */
  void deletePalette(uint16_t paletteId);

  /**
   * @brief Sets color of specified palette item in default palette
   *
   * @param index Palette item (0..<palette size>)
   * @param color Color to assign to this item
   *
   * Example:
   *
   *     // Color item 0 is pure Red
   *     displayController.setPaletteItem(0, RGB888(255, 0, 0));
   */
  virtual void setPaletteItem(int index, RGB888 const & color);

  /**
   * @brief Sets color of specified palette item in a given palette
   * 
   * @param paletteId ID of the palette
   * @param index Index of the item in the palette
   * @param color Color to assign to this item
   */
  void setItemInPalette(uint16_t paletteId, int index, RGB888 const & color);


  /**
   * @brief Creates a new signal list based off simple pairs of row count and palette ID
   * 
   * @param rawList List of row count and palette ID pairs
   * @param entries Number of entries in the list
   */
  void updateSignalList(uint16_t * rawList, int entries);

  // Painter data members

  uint8_t **          m_viewPort;
  int32_t             m_viewPortWidth;
  int32_t             m_viewPortHeight;
  uint8_t             m_packedRGB222_to_PaletteIndex[64];
  PaintState          m_paintState;
  RGB222 *            m_palette;
  int                 m_signalTableSize;
  PaletteListItem *   m_signalList;
  PaletteListItem *   m_currentSignalItem;

  // signal maps for mapping framebuffer data into signals for a given palette ID
  std::unordered_map<uint16_t, void *>  m_signalMaps;

};


} // end of namespace
