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
#include "vga16controller.h"
#include "devdrivers/swgenerator.h"

#pragma GCC optimize ("O2")
namespace fabgl {

// high nibble is pixel 0, low nibble is pixel 1

#define VGA16_COLUMNSQUANTUM 16

/*************************************************************************************/
/* VGA16Controller definitions */
VGA16Controller * VGA16Controller::s_instance = nullptr;

VGA16Controller::VGA16Controller()
  : VGAPalettedController(VGA16_LinesCount, VGA16_COLUMNSQUANTUM, NativePixelFormat::PALETTE16, 2, 1, ISRHandler, 256 * sizeof(uint16_t))
{
  s_instance = this;
  m_painter = new Painter16();
  postConstruct();
}

VGA16Controller::~VGA16Controller()
{
  s_instance = nullptr;
}

void VGA16Controller::packSignals(int index, uint8_t packed222, void * signals)
{
  auto _signals = (uint16_t *) signals;
  for (int i = 0; i < 16; ++i) {
    _signals[(index << 4) | i] &= 0xFF00;
    _signals[(index << 4) | i] |= (m_HVSync | packed222);
    _signals[(i << 4) | index] &= 0x00FF;
    _signals[(i << 4) | index] |= (m_HVSync | packed222) << 8;
  }
}

void IRAM_ATTR VGA16Controller::ISRHandler(void * arg)
{
  #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  auto s1 = getCycleCount();
  #endif

  auto ctrl = (VGA16Controller *) arg;

  if (I2S1.int_st.out_eof) {

    auto const desc = (lldesc_t*) I2S1.out_eof_des_addr;

    if (desc == s_frameResetDesc) {
      s_scanLine = 0;
    }

    auto const width  = ctrl->m_viewPortWidth;
    auto const height = ctrl->m_viewPortHeight;
    int scanLine = (s_scanLine + VGA16_LinesCount / 2) % height;
    if (scanLine == 0) {
      ctrl->m_currentSignalItem = ctrl->m_signalList;
    }

    auto const lines  = ctrl->m_lines;
    auto lineIndex = scanLine & (VGA16_LinesCount - 1);

    for (int i = 0; i < VGA16_LinesCount / 2; ++i) {

      auto src  = (uint8_t const *) s_viewPortVisible[scanLine];
      auto dest = (uint16_t*) lines[lineIndex];
      uint8_t* decpix = (uint8_t*) dest;
      auto const packedPaletteIndexPair_to_signals = (uint16_t *) ctrl->getSignalsForScanline(scanLine);

      // optimization warn: horizontal resolution must be a multiple of 16!
      for (int col = 0; col < width; col += 16) {

        auto src1 = *(src + 0);
        auto src2 = *(src + 1);
        auto src3 = *(src + 2);
        auto src4 = *(src + 3);
        auto src5 = *(src + 4);
        auto src6 = *(src + 5);
        auto src7 = *(src + 6);
        auto src8 = *(src + 7);

        PSRAM_HACK;

        auto v1 = packedPaletteIndexPair_to_signals[src1];
        auto v2 = packedPaletteIndexPair_to_signals[src2];
        auto v3 = packedPaletteIndexPair_to_signals[src3];
        auto v4 = packedPaletteIndexPair_to_signals[src4];
        auto v5 = packedPaletteIndexPair_to_signals[src5];
        auto v6 = packedPaletteIndexPair_to_signals[src6];
        auto v7 = packedPaletteIndexPair_to_signals[src7];
        auto v8 = packedPaletteIndexPair_to_signals[src8];

        *(dest + 1) = v1;
        *(dest    ) = v2;
        *(dest + 3) = v3;
        *(dest + 2) = v4;
        *(dest + 5) = v5;
        *(dest + 4) = v6;
        *(dest + 7) = v7;
        *(dest + 6) = v8;

        dest += 8;
        src += 8;

      }

      ctrl->decorateScanLinePixels(decpix, scanLine);
      ++lineIndex;
      ++scanLine;
    }

    s_scanLine += VGA16_LinesCount / 2;

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
