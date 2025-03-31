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

#pragma GCC optimize ("O2")

#include "painter.h"
#include <string.h>
#include "esp_heap_caps.h"

namespace fabgl {

/*************************************************************************************/
/* Painter definitions */

Painter::Painter() {
}

Painter::~Painter() {
}

void Painter::setViewPort(uint8_t** rows, uint32_t width, uint32_t height) {
  m_viewPort = rows;
  m_viewPortWidth = width;
  m_viewPortHeight = height;
}

Painter::postConstruct() {
  createPalette(0);
  uint16_t signalList[2] = { 0, 0 };
  m_signalList = createSignalList(signalList, 1);
  m_currentSignalItem = m_signalList;
}

// parameters not checked
void Painter::rawORRow(int y, int x1, int x2, uint8_t colorIndex)
{
  // for a mono display, if our colorIndex is zero we do nothing
  // if it's one it's a set
  if (colorIndex == 0)
    return;
  rawFillRow(y, x1, x2, colorIndex);
}

// parameters not checked
void Painter::rawANDRow(int y, int x1, int x2, uint8_t colorIndex)
{
  // for a mono display, if our colorIndex is one we do nothing
  // if it's zero it's a clear
  if (colorIndex == 1)
    return;
  rawFillRow(y, x1, x2, 0);
}

// parameters not checked
void Painter::rawXORRow(int y, int x1, int x2, uint8_t colorIndex)
{
  // In a mono display, XOR is the same as invert when colorIndex is 1
  if (colorIndex == 0)
    return;
  rawInvertRow(y, x1, x2);
}

void Painter::clear(Rect & updateRect)
{
  uint8_t paletteIndex = RGB888toPaletteIndex(getActualBrushColor());
  uint8_t pattern8 = paletteIndex ? 0xFF : 0x00;
  for (int y = 0; y < m_viewPortHeight; ++y)
    memset((uint8_t*) m_viewPort[y], pattern8, m_viewPortWidth / 8);
}

// scroll < 0 -> scroll UP
// scroll > 0 -> scroll DOWN
void Painter::VScroll(int scroll, Rect & updateRect)
{
  genericVScroll(scroll, updateRect,
                 [&] (int yA, int yB, int x1, int x2)        { swapRows(yA, yB, x1, x2); },              // swapRowsCopying
                 [&] (int yA, int yB)                        { tswap(m_viewPort[yA], m_viewPort[yB]); }, // swapRowsPointers
                 [&] (int y, int x1, int x2, RGB888 color)   { rawFillRow(y, x1, x2, RGB888toPaletteIndex(color)); }   // rawFillRow
                );
}

void Painter::invertRect(Rect const & rect, Rect & updateRect)
{
  genericInvertRect(rect, updateRect,
                    [&] (int Y, int X1, int X2) { rawInvertRow(Y, X1, X2); }
                   );
}

void Painter::setPaletteItem(int index, RGB888 const & color)
{
  setItemInPalette(0, index, color);
}


void Painter::setItemInPalette(uint16_t paletteId, int index, RGB888 const & color)
{
  if (m_signalMaps.find(paletteId) == m_signalMaps.end()) {
    if (!createPalette(paletteId)) {
      return;
    }
  }
  index %= getPaletteSize();
  if (paletteId == 0) {
    m_palette[index] = color;
  }
  auto packed222 = RGB888toPackedRGB222(color);
  packSignals(index, packed222, m_signalMaps[paletteId]);
}

// rebuild m_packedRGB222_to_PaletteIndex
void Painter::updateRGB2PaletteLUT()
{
  auto paletteSize = getPaletteSize();
  for (int r = 0; r < 4; ++r)
    for (int g = 0; g < 4; ++g)
      for (int b = 0; b < 4; ++b) {
        double H1, S1, V1;
        rgb222_to_hsv(r, g, b, &H1, &S1, &V1);
        int bestIdx = 0;
        int bestDst = 1000000000;
        for (int i = 0; i < paletteSize; ++i) {
          double H2, S2, V2;
          rgb222_to_hsv(m_palette[i].R, m_palette[i].G, m_palette[i].B, &H2, &S2, &V2);
          double AH = H1 - H2;
          double AS = S1 - S2;
          double AV = V1 - V2;
          int dst = AH * AH + AS * AS + AV * AV;
          if (dst <= bestDst) {  // "<=" to prioritize higher indexes
            bestIdx = i;
            bestDst = dst;
            if (bestDst == 0)
              break;
          }
        }
        m_packedRGB222_to_PaletteIndex[r | (g << 2) | (b << 4)] = bestIdx;
      }
}


bool Painter::createPalette(uint16_t paletteId)
{
  if (m_signalTableSize == 0) {
    return false;
  }
  if (m_signalMaps.find(paletteId) == m_signalMaps.end()) {
    m_signalMaps[paletteId] = heap_caps_malloc(m_signalTableSize, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!m_signalMaps[paletteId]) {
      m_signalMaps.erase(paletteId);
      // create failed
      return false;
    }
    if (paletteId == 0) {
      return true;
    }
  }
  // Duplicate palette 0 into new palette
  if (paletteId != 0) {
    memcpy(m_signalMaps[paletteId], m_signalMaps[0], m_signalTableSize);
  }
  return true;
}


void Painter::deletePalette(uint16_t paletteId)
{
  if (paletteId == 0) {
    return;
  }
  if (paletteId == 65535) {
    // iterate over all palettes and delete them using deletePalette
    for (auto it = m_signalMaps.begin(); it != m_signalMaps.end(); ++it) {
      deletePalette(it->first);
    }
    return;
  }
  if (m_signalMaps.find(paletteId) != m_signalMaps.end()) {
    auto signals = m_signalMaps[paletteId];
    auto signalItem = m_signalList;
    while (signalItem) {
      if (signalItem->signals == signals) {
        signalItem->signals = m_signalMaps[0];
      }
      signalItem = signalItem->next;
    }
    heap_caps_free(m_signalMaps[paletteId]);
    m_signalMaps.erase(paletteId);
  }
}

void IRAM_ATTR Painter::resetPaintState()
{
  m_paintState.penColor              = RGB888(255, 255, 255);
  m_paintState.brushColor            = RGB888(0, 0, 0);
  m_paintState.position              = Point(0, 0);
  m_paintState.glyphOptions.value    = 0;  // all options: 0
  m_paintState.paintOptions          = PaintOptions();
  m_paintState.scrollingRegion       = Rect(0, 0, getViewPortWidth() - 1, getViewPortHeight() - 1);
  m_paintState.origin                = Point(0, 0);
  m_paintState.clippingRect          = Rect(0, 0, getViewPortWidth() - 1, getViewPortHeight() - 1);
  m_paintState.absClippingRect       = m_paintState.clippingRect;
  m_paintState.penWidth              = 1;
  m_paintState.lineEnds              = LineEnds::None;
  m_paintState.linePattern           = LinePattern();
  m_paintState.lineOptions           = LineOptions();
  m_paintState.linePatternLength     = 8;
}


RGB888 IRAM_ATTR Painter::getActualBrushColor()
{
  return paintState().paintOptions.swapFGBG ? paintState().penColor : paintState().brushColor;
}


RGB888 IRAM_ATTR Painter::getActualPenColor()
{
  return paintState().paintOptions.swapFGBG ? paintState().brushColor : paintState().penColor;
}

void Painter::deleteSignalList(PaletteListItem * item)
{
  if (item) {
    deleteSignalList(item->next);
    heap_caps_free(item);
  }
}


void Painter::updateSignalList(uint16_t * rawList, int entries)
{
  // Walk list, updating existing signal list
  // creating new list if we exceed the current list,
  // deleting any remaining items if we have fewer entries
  PaletteListItem * item = m_signalList;
  int row = 0;

  while (entries) {
    auto rows = rawList[0];
    auto paletteID = rawList[1];
    rawList += 2;

    row += rows;
    item->endRow = row;
    if (m_signalMaps.find(paletteID) != m_signalMaps.end()) {
      item->signals = m_signalMaps[paletteID];
    } else {
      item->signals = m_signalMaps[0];
    }

    entries--;

    if (entries) {
      if (item->next) {
        item = item->next;
      } else {
        item->next = createSignalList(rawList, entries, row);
        return;
      }
    }
  }

  if (item->next) {
    deleteSignalList(item->next);
    item->next = NULL;
  }
}


PaletteListItem * Painter::createSignalList(uint16_t * rawList, int entries, int row)
{
  PaletteListItem * item = (PaletteListItem *) heap_caps_malloc(sizeof(PaletteListItem), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
  auto rows = rawList[0];
  auto paletteID = rawList[1];

  item->endRow = row + rows;

  if (m_signalMaps.find(paletteID) != m_signalMaps.end()) {
    item->signals = m_signalMaps[paletteID];
  } else {
    item->signals = m_signalMaps[0];
  }

  if (entries > 1) {
    item->next = createSignalList(rawList + 2, entries - 1, row + rows);
  } else {
    item->next = NULL;
  }

  return item;
}


void * IRAM_ATTR Painter::getSignalsForScanline(int scanLine) {
  if (scanLine < m_currentSignalItem->endRow) {
    return m_currentSignalItem->signals;
  }
  while (m_currentSignalItem->next && (scanLine >= m_currentSignalItem->endRow)) {
    m_currentSignalItem = m_currentSignalItem->next;
  }
  return m_currentSignalItem->signals;
}


void IRAM_ATTR Painter::lineTo(Point const & position, Rect & updateRect)
{
  RGB888 color = getActualPenColor();

  int origX = paintState().origin.X;
  int origY = paintState().origin.Y;
  int x1 = paintState().position.X;
  int y1 = paintState().position.Y;
  int x2 = position.X + origX;
  int y2 = position.Y + origY;

  int hw = paintState().penWidth / 2;
  updateRect = updateRect.merge(Rect(imin(x1, x2) - hw, imin(y1, y2) - hw, imax(x1, x2) + hw, imax(y1, y2) + hw));
  absDrawLine(x1, y1, x2, y2, color);

  paintState().position = Point(x2, y2);
}


void IRAM_ATTR Painter::updateAbsoluteClippingRect()
{
  int X1 = iclamp(paintState().origin.X + paintState().clippingRect.X1, 0, getViewPortWidth() - 1);
  int Y1 = iclamp(paintState().origin.Y + paintState().clippingRect.Y1, 0, getViewPortHeight() - 1);
  int X2 = iclamp(paintState().origin.X + paintState().clippingRect.X2, 0, getViewPortWidth() - 1);
  int Y2 = iclamp(paintState().origin.Y + paintState().clippingRect.Y2, 0, getViewPortHeight() - 1);
  paintState().absClippingRect = Rect(X1, Y1, X2, Y2);
}


void IRAM_ATTR Painter::drawRect(Rect const & rect, Rect & updateRect)
{
  int x1 = (rect.X1 < rect.X2 ? rect.X1 : rect.X2) + paintState().origin.X;
  int y1 = (rect.Y1 < rect.Y2 ? rect.Y1 : rect.Y2) + paintState().origin.Y;
  int x2 = (rect.X1 < rect.X2 ? rect.X2 : rect.X1) + paintState().origin.X;
  int y2 = (rect.Y1 < rect.Y2 ? rect.Y2 : rect.Y1) + paintState().origin.Y;

  int hw = paintState().penWidth / 2;
  updateRect = updateRect.merge(Rect(x1 - hw, y1 - hw, x2 + hw, y2 + hw));
  RGB888 color = getActualPenColor();

  absDrawLine(x1 + 1, y1,     x2, y1, color);
  absDrawLine(x2,     y1 + 1, x2, y2, color);
  absDrawLine(x2 - 1, y2,     x1, y2, color);
  absDrawLine(x1,     y2 - 1, x1, y1, color);
}


void IRAM_ATTR Painter::fillRect(Rect const & rect, RGB888 const & color, Rect & updateRect)
{
  int x1 = (rect.X1 < rect.X2 ? rect.X1 : rect.X2) + paintState().origin.X;
  int y1 = (rect.Y1 < rect.Y2 ? rect.Y1 : rect.Y2) + paintState().origin.Y;
  int x2 = (rect.X1 < rect.X2 ? rect.X2 : rect.X1) + paintState().origin.X;
  int y2 = (rect.Y1 < rect.Y2 ? rect.Y2 : rect.Y1) + paintState().origin.Y;

  const int clipX1 = paintState().absClippingRect.X1;
  const int clipY1 = paintState().absClippingRect.Y1;
  const int clipX2 = paintState().absClippingRect.X2;
  const int clipY2 = paintState().absClippingRect.Y2;

  if (x1 > clipX2 || x2 < clipX1 || y1 > clipY2 || y2 < clipY1)
    return;

  x1 = iclamp(x1, clipX1, clipX2);
  y1 = iclamp(y1, clipY1, clipY2);
  x2 = iclamp(x2, clipX1, clipX2);
  y2 = iclamp(y2, clipY1, clipY2);

  updateRect = updateRect.merge(Rect(x1, y1, x2, y2));

  for (int y = y1; y <= y2; ++y)
    fillRow(y, x1, x2, color);
}


// McIlroy's algorithm
void IRAM_ATTR Painter::fillEllipse(int centerX, int centerY, Size const & size, RGB888 const & color, Rect & updateRect)
{
  const int clipX1 = paintState().absClippingRect.X1;
  const int clipY1 = paintState().absClippingRect.Y1;
  const int clipX2 = paintState().absClippingRect.X2;
  const int clipY2 = paintState().absClippingRect.Y2;

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
  int width = 1;
  int t = -a2 * y;
  int dxt = 2 * b2 * x;
  int dyt = -2 * a2 * y;

  while (y >= 0 && x <= halfWidth) {
    if (t + b2 * x <= crit1 || t + a2 * y <= crit3) {
      x++;
      dxt += d2xt;
      t += dxt;
      width += 2;
    } else {
      int col1 = centerX - x;
      int col2 = centerX - x + width - 1;
      if (col1 <= clipX2 && col2 >= clipX1) {
        col1 = iclamp(col1, clipX1, clipX2);
        col2 = iclamp(col2, clipX1, clipX2);
        int row1 = centerY - y;
        int row2 = centerY + y;
        if (row1 >= clipY1 && row1 <= clipY2)
          fillRow(row1, col1, col2, color);
        if (y != 0 && row2 >= clipY1 && row2 <= clipY2)
          fillRow(row2, col1, col2, color);
      }
      if (t - a2 * y <= crit2) {
        x++;
        dxt += d2xt;
        t += dxt;
        width += 2;
      }
      y--;
      dyt += d2yt;
      t += dyt;
    }
  }
  // one line horizontal ellipse case
  if (halfHeight == 0 && centerY >= clipY1 && centerY <= clipY2)
    fillRow(centerY, iclamp(centerX - halfWidth, clipX1, clipX2), iclamp(centerX - halfWidth + 2 * halfWidth + 1, clipX1, clipX2), color);
}


void IRAM_ATTR Painter::renderGlyphsBuffer(GlyphsBufferRenderInfo const & glyphsBufferRenderInfo, Rect & updateRect)
{
  int itemX = glyphsBufferRenderInfo.itemX;
  int itemY = glyphsBufferRenderInfo.itemY;

  int glyphsWidth  = glyphsBufferRenderInfo.glyphsBuffer->glyphsWidth;
  int glyphsHeight = glyphsBufferRenderInfo.glyphsBuffer->glyphsHeight;

  uint32_t const * mapItem = glyphsBufferRenderInfo.glyphsBuffer->map + itemX + itemY * glyphsBufferRenderInfo.glyphsBuffer->columns;

  GlyphOptions glyphOptions = glyphMapItem_getOptions(mapItem);
  auto fgColor = glyphMapItem_getFGColor(mapItem);
  auto bgColor = glyphMapItem_getBGColor(mapItem);

  Glyph glyph;
  glyph.X      = (int16_t) (itemX * glyphsWidth * (glyphOptions.doubleWidth ? 2 : 1));
  glyph.Y      = (int16_t) (itemY * glyphsHeight);
  glyph.width  = glyphsWidth;
  glyph.height = glyphsHeight;
  glyph.data   = glyphsBufferRenderInfo.glyphsBuffer->glyphsData + glyphMapItem_getIndex(mapItem) * glyphsHeight * ((glyphsWidth + 7) / 8);;

  drawGlyph(glyph, glyphOptions, fgColor, bgColor, updateRect);
}


void IRAM_ATTR Painter::drawPath(Path const & path, Rect & updateRect)
{
  RGB888 color = getActualPenColor();

  const int clipX1 = paintState().absClippingRect.X1;
  const int clipY1 = paintState().absClippingRect.Y1;
  const int clipX2 = paintState().absClippingRect.X2;
  const int clipY2 = paintState().absClippingRect.Y2;

  int origX = paintState().origin.X;
  int origY = paintState().origin.Y;

  int minX = clipX1;
  int maxX = clipX2 + 1;
  int minY = INT_MAX;
  int maxY = 0;
  for (int i = 0; i < path.pointsCount; ++i) {
    int py = path.points[i].Y + origY;
    if (py < minY)
      minY = py;
    if (py > maxY)
      maxY = py;
  }
  minY = tmax(clipY1, minY);
  maxY = tmin(clipY2, maxY);

  int hw = paintState().penWidth / 2;
  updateRect = updateRect.merge(Rect(minX - hw, minY - hw, maxX + hw, maxY + hw));

  int i = 0;
  for (; i < path.pointsCount - 1; ++i) {
    const int x1 = path.points[i].X + origX;
    const int y1 = path.points[i].Y + origY;
    const int x2 = path.points[i + 1].X + origX;
    const int y2 = path.points[i + 1].Y + origY;
    absDrawLine(x1, y1, x2, y2, color);
  }
  const int x1 = path.points[i].X + origX;
  const int y1 = path.points[i].Y + origY;
  const int x2 = path.points[0].X + origX;
  const int y2 = path.points[0].Y + origY;
  absDrawLine(x1, y1, x2, y2, color);

  if (path.freePoints)
    m_primDynMemPool.free((void*)path.points);
}


void IRAM_ATTR Painter::fillPath(Path const & path, RGB888 const & color, Rect & updateRect)
{
  const int clipX1 = paintState().absClippingRect.X1;
  const int clipY1 = paintState().absClippingRect.Y1;
  const int clipX2 = paintState().absClippingRect.X2;
  const int clipY2 = paintState().absClippingRect.Y2;

  const int origX = paintState().origin.X;
  const int origY = paintState().origin.Y;

  int minX = clipX1;
  int maxX = clipX2 + 1;
  int minY = INT_MAX;
  int maxY = 0;
  for (int i = 0; i < path.pointsCount; ++i) {
    int py = path.points[i].Y + origY;
    if (py < minY)
      minY = py;
    if (py > maxY)
      maxY = py;
  }
  minY = tmax(clipY1, minY);
  maxY = tmin(clipY2, maxY);

  updateRect = updateRect.merge(Rect(minX, minY, maxX, maxY));

  int16_t nodeX[path.pointsCount];

  for (int pixelY = minY; pixelY <= maxY; ++pixelY) {

    int nodes = 0;
    int j = path.pointsCount - 1;
    for (int i = 0; i < path.pointsCount; ++i) {
      int piy = path.points[i].Y + origY;
      int pjy = path.points[j].Y + origY;
      if ((piy < pixelY && pjy >= pixelY) || (pjy < pixelY && piy >= pixelY)) {
        int pjx = path.points[j].X + origX;
        int pix = path.points[i].X + origX;
        int a = (pixelY - piy) * (pjx - pix);
        int b = (pjy - piy);
        nodeX[nodes++] = pix + a / b + (((a < 0) ^ (b > 0)) && (a % b));
      }
      j = i;
    }

    int i = 0;
    while (i < nodes - 1) {
      if (nodeX[i] > nodeX[i + 1]) {
        tswap(nodeX[i], nodeX[i + 1]);
        if (i)
          --i;
      } else
        ++i;
    }

    for (int i = 0; i < nodes; i += 2) {
      if (nodeX[i] >= maxX)
        break;
      if (nodeX[i + 1] > minX) {
        if (nodeX[i] < minX)
          nodeX[i] = minX;
        if (nodeX[i + 1] > maxX)
          nodeX[i + 1] = maxX;
        fillRow(pixelY, nodeX[i], nodeX[i + 1] - 1, color);
      }
    }
  }

  if (path.freePoints)
    m_primDynMemPool.free((void*)path.points);
}


void IRAM_ATTR Painter::absDrawThickLine(int X1, int Y1, int X2, int Y2, int penWidth, RGB888 const & color)
{
  // just to "de-absolutize"
  const int origX = paintState().origin.X;
  const int origY = paintState().origin.Y;
  X1 -= origX;
  Y1 -= origY;
  X2 -= origX;
  Y2 -= origY;

  Point pts[4];

  const double angle = atan2(Y2 - Y1, X2 - X1);
  const double pw = (double)penWidth / 2.0;
  const int ofs1 = lround(pw * cos(angle + M_PI_2));
  const int ofs2 = lround(pw * sin(angle + M_PI_2));
  const int ofs3 = lround(pw * cos(angle - M_PI_2));
  const int ofs4 = lround(pw * sin(angle - M_PI_2));
  pts[0].X = X1 + ofs1;
  pts[0].Y = Y1 + ofs2;
  pts[1].X = X1 + ofs3;
  pts[1].Y = Y1 + ofs4;
  pts[2].X = X2 + ofs3;
  pts[2].Y = Y2 + ofs4;
  pts[3].X = X2 + ofs1;
  pts[3].Y = Y2 + ofs2;

  Rect updateRect;
  Path path = { pts, 4, false };
  fillPath(path, color, updateRect);

  switch (paintState().lineEnds) {
    case LineEnds::Circle:
      if ((penWidth & 1) == 0)
        --penWidth;
      fillEllipse(X1, Y1, Size(penWidth, penWidth), color, updateRect);
      fillEllipse(X2, Y2, Size(penWidth, penWidth), color, updateRect);
      break;
    default:
      break;
  }
}


void IRAM_ATTR Painter::drawBitmap(BitmapDrawingInfo const & bitmapDrawingInfo, Rect & updateRect)
{
  int x = bitmapDrawingInfo.X + paintState().origin.X;
  int y = bitmapDrawingInfo.Y + paintState().origin.Y;
  updateRect = updateRect.merge(Rect(x, y, x + bitmapDrawingInfo.bitmap->width - 1, y + bitmapDrawingInfo.bitmap->height - 1));
  absDrawBitmap(x, y, bitmapDrawingInfo.bitmap, nullptr, false);
}


void IRAM_ATTR Painter::absDrawBitmap(int destX, int destY, Bitmap const * bitmap, void * saveBackground, bool ignoreClippingRect)
{
  const int clipX1 = ignoreClippingRect ? 0 : paintState().absClippingRect.X1;
  const int clipY1 = ignoreClippingRect ? 0 : paintState().absClippingRect.Y1;
  const int clipX2 = ignoreClippingRect ? getViewPortWidth() - 1 : paintState().absClippingRect.X2;
  const int clipY2 = ignoreClippingRect ? getViewPortHeight() - 1 : paintState().absClippingRect.Y2;

  if (destX > clipX2 || destY > clipY2)
    return;

  int width  = bitmap->width;
  int height = bitmap->height;

  int X1 = 0;
  int XCount = width;

  if (destX < clipX1) {
    X1 = clipX1 - destX;
    destX = clipX1;
  }
  if (X1 >= width)
    return;

  if (destX + XCount > clipX2 + 1)
    XCount = clipX2 + 1 - destX;
  if (X1 + XCount > width)
    XCount = width - X1;

  int Y1 = 0;
  int YCount = height;

  if (destY < clipY1) {
    Y1 = clipY1 - destY;
    destY = clipY1;
  }
  if (Y1 >= height)
    return;

  if (destY + YCount > clipY2 + 1)
    YCount = clipY2 + 1 - destY;
  if (Y1 + YCount > height)
    YCount = height - Y1;

  switch (bitmap->format) {

    case PixelFormat::Undefined:
      break;

    case PixelFormat::Native:
      rawDrawBitmap_Native(destX, destY, bitmap, X1, Y1, XCount, YCount);
      break;

    case PixelFormat::Mask:
      rawDrawBitmap_Mask(destX, destY, bitmap, saveBackground, X1, Y1, XCount, YCount);
      break;

    case PixelFormat::RGBA2222:
      rawDrawBitmap_RGBA2222(destX, destY, bitmap, saveBackground, X1, Y1, XCount, YCount);
      break;

    case PixelFormat::RGBA8888:
      rawDrawBitmap_RGBA8888(destX, destY, bitmap, saveBackground, X1, Y1, XCount, YCount);
      break;

  }

}


void IRAM_ATTR Painter::copyToBitmap(BitmapDrawingInfo const & bitmapCopyingInfo)
{
  int x = bitmapCopyingInfo.X + paintState().origin.X;
  int y = bitmapCopyingInfo.Y + paintState().origin.Y;
  absCopyToBitmap(x, y, bitmapCopyingInfo.bitmap);
}


void IRAM_ATTR Painter::absCopyToBitmap(int srcX, int srcY, Bitmap const * bitmap)
{
  const int width  = bitmap->width;
  const int height = bitmap->height;
  const int clipX1 = 0;
  const int clipY1 = 0;
  const int clipX2 = paintState().absClippingRect.X2;
  const int clipY2 = paintState().absClippingRect.Y2;

  if (srcX > clipX2 || srcY > clipY2)
    return;

  int X1 = 0;
  int XCount = width;

  if (srcX < clipX1) {
    X1 = clipX1 - srcX;
    srcX = clipX1;
  }
  if (X1 >= width)
    return;

  if (srcX + XCount > clipX2 + 1)
    XCount = clipX2 + 1 - srcX;
  if (X1 + XCount > width)
    XCount = width - X1;

  int Y1 = 0;
  int YCount = height;

  if (srcY < clipY1) {
    Y1 = clipY1 - srcY;
    srcY = clipY1;
  }
  if (Y1 >= height)
    return;

  if (srcY + YCount > clipY2 + 1)
    YCount = clipY2 + 1 - srcY;
  if (Y1 + YCount > height)
    YCount = height - Y1;

  rawCopyToBitmap(srcX, srcY, width, bitmap->data, X1, Y1, XCount, YCount);
}


void IRAM_ATTR Painter::drawBitmapWithTransform(BitmapTransformedDrawingInfo const & drawingInfo, Rect & updateRect)
{
  // work out corners of the bitmap by taking the corners of the bitmap and transforming them by the matrix
  int x = paintState().origin.X + drawingInfo.X;
  int y = paintState().origin.Y + drawingInfo.Y;
  int width = drawingInfo.bitmap->width;
  int height = drawingInfo.bitmap->height;

  Rect originalBox = Rect(0, 0, width, height);
  auto transformMatrix = drawingInfo.transformMatrix;

  // work out each corner's new position
  int minX = INT_MAX;
  int minY = INT_MAX;
  int maxX = INT_MIN;
  int maxY = INT_MIN;
  float pos[3] = { (float)originalBox.X1, (float)originalBox.Y1, 1.0f };
  float transformed[3];
  dspm_mult_3x3x1_f32(transformMatrix, pos, transformed);
  minX = imin(minX, (int)transformed[0]);
  minY = imin(minY, (int)transformed[1]);
  maxX = imax(maxX, (int)transformed[0]);
  maxY = imax(maxY, (int)transformed[1]);

  pos[0] = (float)originalBox.X2;
  dspm_mult_3x3x1_f32(transformMatrix, pos, transformed);
  minX = imin(minX, (int)transformed[0]);
  minY = imin(minY, (int)transformed[1]);
  maxX = imax(maxX, (int)transformed[0]);
  maxY = imax(maxY, (int)transformed[1]);

  pos[0] = (float)originalBox.X1;
  pos[1] = (float)originalBox.Y2;
  dspm_mult_3x3x1_f32(transformMatrix, pos, transformed);
  minX = imin(minX, (int)transformed[0]);
  minY = imin(minY, (int)transformed[1]);
  maxX = imax(maxX, (int)transformed[0]);
  maxY = imax(maxY, (int)transformed[1]);

  pos[0] = (float)originalBox.X2;
  dspm_mult_3x3x1_f32(transformMatrix, pos, transformed);
  minX = imin(minX, (int)transformed[0]);
  minY = imin(minY, (int)transformed[1]);
  maxX = imax(maxX, (int)transformed[0]);
  maxY = imax(maxY, (int)transformed[1]);

  Rect transformedBox = Rect(minX, minY, maxX, maxY);

  // transformed box at this point is the bounding box _without_ taking into account origin
  // drawingRect gets translated, and then clipped to the current clipping rect
  Rect drawingRect = transformedBox.translate(x, y).intersection(paintState().absClippingRect);

  if (drawingRect.width() == 0 || drawingRect.height() == 0) {
    // no area within our current clipping rect to draw
    if (drawingInfo.freeMatrix) {
      m_primDynMemPool.free((void*)drawingInfo.transformMatrix);
      m_primDynMemPool.free((void*)drawingInfo.transformInverse);
    }
    return;
  }

  updateRect = updateRect.merge(drawingRect);

  // translate our drawing rect _back_ to the origin
  // NB agon-vdp never adjusts the vdp-gl canvas origin, so this is essentially unnecessary, and untested
  drawingRect = drawingRect.translate(-x, -y);

  // drawingRect is now the translated area of the bitmap that we want to draw
  // using an inverted matrix, we can work out the original pixels in the bitmap to draw on-screen

  // Draw the bitmap
  switch (drawingInfo.bitmap->format) {

    case PixelFormat::Undefined:
      break;

    // case PixelFormat::Native:
    //   rawDrawBitmapWithMatrix_Native(x, y, drawingRect, drawingInfo.bitmap, drawingInfo.transformInverse);
    //   break;

    case PixelFormat::Mask:
      rawDrawBitmapWithMatrix_Mask(x, y, drawingRect, drawingInfo.bitmap, drawingInfo.transformInverse);
      break;

    case PixelFormat::RGBA2222:
      rawDrawBitmapWithMatrix_RGBA2222(x, y, drawingRect, drawingInfo.bitmap, drawingInfo.transformInverse);
      break;

    case PixelFormat::RGBA8888:
      rawDrawBitmapWithMatrix_RGBA8888(x, y, drawingRect, drawingInfo.bitmap, drawingInfo.transformInverse);
      break;

  }

  if (drawingInfo.freeMatrix) {
    m_primDynMemPool.free((void*)drawingInfo.transformMatrix);
    m_primDynMemPool.free((void*)drawingInfo.transformInverse);
  }
}

} // end of namespace
