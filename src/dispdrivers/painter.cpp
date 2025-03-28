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
  m_signalList = getPainter()->createSignalList(signalList, 1);
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


} // end of namespace
