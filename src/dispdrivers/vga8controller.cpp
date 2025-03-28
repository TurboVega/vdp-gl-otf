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

#include <alloca.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "driver/periph_ctrl.h"
#include "soc/rtc.h"
#include "esp_spi_flash.h"
#include "esp_heap_caps.h"

#include "fabutils.h"
#include "vga8controller.h"
#include "devdrivers/swgenerator.h"
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

#define VGA8_COLUMNSQUANTUM 16

/*************************************************************************************/
/* VGA8Controller definitions */
VGA8Controller * VGA8Controller::s_instance = nullptr;
VGA8Controller::VGA8Controller()
  : VGAPalettedController(VGA8_LinesCount, VGA8_COLUMNSQUANTUM, NativePixelFormat::PALETTE8, 8, 3, ISRHandler, 256 * sizeof(uint16_t))
{
  s_instance = this;
  m_painter = new Painter8();
}

VGA8Controller::~VGA8Controller()
{
  s_instance = nullptr;
}

void VGA8Controller::packSignals(int index, uint8_t packed222, void * signals)
{
  auto _signals = (uint16_t *) signals;
  for (int i = 0; i < 8; ++i) {
    _signals[(index << 3) | i] &= 0xFF00;
    _signals[(index << 3) | i] |= (m_HVSync | packed222);
    _signals[(i << 3) | index] &= 0x00FF;
    _signals[(i << 3) | index] |= (m_HVSync | packed222) << 8;
  }
}

void IRAM_ATTR VGA8Controller::ISRHandler(void * arg)
{
  #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  auto s1 = getCycleCount();
  #endif

  auto ctrl = (VGA8Controller *) arg;

  if (I2S1.int_st.out_eof) {

    auto const desc = (lldesc_t*) I2S1.out_eof_des_addr;

    if (desc == s_frameResetDesc) {
      s_scanLine = 0;
    }

    auto const width  = ctrl->m_viewPortWidth;
    auto const height = ctrl->m_viewPortHeight;
    int scanLine = (s_scanLine + VGA8_LinesCount / 2) % height;
    if (scanLine == 0) {
      ctrl->m_currentSignalItem = ctrl->m_signalList;
    }

    auto const lines  = ctrl->m_lines;
    auto lineIndex = scanLine & (VGA8_LinesCount - 1);

    for (int i = 0; i < VGA8_LinesCount / 2; ++i) {

      auto src  = (uint8_t const *) s_viewPortVisible[scanLine];
      auto dest = (uint16_t*) lines[lineIndex];
      uint8_t* decpix = (uint8_t*) dest;
      auto const packedPaletteIndexPair_to_signals = (uint16_t *) ctrl->getSignalsForScanline(scanLine);

      // optimization warn: horizontal resolution must be a multiple of 16!
      for (int col = 0; col < width; col += 16) {

        auto w1 = *((uint16_t*)(src    ));  // hi A:23334445, lo A:55666777
        auto w2 = *((uint16_t*)(src + 2));  // hi B:55666777, lo A:00011122
        auto w3 = *((uint16_t*)(src + 4));  // hi B:00011122, lo B:23334445

        PSRAM_HACK;

        auto src1 = w1 | (w2 << 16);
        auto src2 = (w2 >> 8) | (w3 << 8);

        auto v1 = packedPaletteIndexPair_to_signals[(src1      ) & 0x3f];  // pixels 0, 1
        auto v2 = packedPaletteIndexPair_to_signals[(src1 >>  6) & 0x3f];  // pixels 2, 3
        auto v3 = packedPaletteIndexPair_to_signals[(src1 >> 12) & 0x3f];  // pixels 4, 5
        auto v4 = packedPaletteIndexPair_to_signals[(src1 >> 18) & 0x3f];  // pixels 6, 7
        auto v5 = packedPaletteIndexPair_to_signals[(src2      ) & 0x3f];  // pixels 8, 9
        auto v6 = packedPaletteIndexPair_to_signals[(src2 >>  6) & 0x3f];  // pixels 10, 11
        auto v7 = packedPaletteIndexPair_to_signals[(src2 >> 12) & 0x3f];  // pixels 12, 13
        auto v8 = packedPaletteIndexPair_to_signals[(src2 >> 18) & 0x3f];  // pixels 14, 15

        *(dest + 2) = v1;
        *(dest + 3) = v2;
        *(dest + 0) = v3;
        *(dest + 1) = v4;
        *(dest + 6) = v5;
        *(dest + 7) = v6;
        *(dest + 4) = v7;
        *(dest + 5) = v8;

        dest += 8;
        src += 6;

      }

      ctrl->decorateScanLinePixels(decpix, scanLine);
      ++lineIndex;
      ++scanLine;
    }

    s_scanLine += VGA8_LinesCount / 2;

    if (scanLine >= height) {
      ctrl->frameCounter++;
      if (!ctrl->m_primitiveProcessingSuspended && spi_flash_cache_enabled() && ctrl->m_primitiveExecTask) {
        // vertical sync, unlock primitive execution task
        // warn: don't use vTaskSuspendAll() in primitive drawing, otherwise vTaskNotifyGiveFromISR may be blocked and screen will flick!
        vTaskNotifyGiveFromISR(ctrl->m_primitiveExecTask, NULL);
      }
    }

  }

  #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  s_vgapalctrlcycles += getCycleCount() - s1;
  #endif

  I2S1.int_clr.val = I2S1.int_st.val;
}

} // end of namespace
