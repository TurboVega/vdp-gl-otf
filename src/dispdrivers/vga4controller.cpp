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
#include "vga4controller.h"
#include "devdrivers/swgenerator.h"

#pragma GCC optimize ("O2")
namespace fabgl {

#define VGA4_COLUMNSQUANTUM 16

/*************************************************************************************/
/* VGA4Controller definitions */
VGA4Controller * VGA4Controller::s_instance = nullptr;

VGA4Controller::VGA4Controller()
  : VideoController(VGA4_LinesCount, VGA4_COLUMNSQUANTUM, NativePixelFormat::PALETTE4, 4, 1, ISRHandler)
{
  s_instance = this;
  m_painter = new Painter4();
  m_painter->postConstruct(256 * sizeof(uint32_t));
}

VGA4Controller::~VGA4Controller()
{
  s_instance = nullptr;
}

void VGA4Controller::packSignals(int index, uint8_t packed222, void * signals)
{
  auto _signals = (uint32_t *) signals;
  for (int i = 0; i < 256; ++i) {
    auto b = (uint8_t *) (_signals + i);
    for (int j = 0; j < 4; ++j) {
      auto aj = 6 - j * 2;
      if (((i >> aj) & 3) == index) {
        b[j ^ 2] = m_HVSync | packed222;
      }
    }
  }
}

void IRAM_ATTR VGA4Controller::ISRHandler(void * arg)
{
  #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  auto s1 = getCycleCount();
  #endif

  auto ctrl = (VGA4Controller *) arg;

  if (I2S1.int_st.out_eof) {

    auto const desc = (lldesc_t*) I2S1.out_eof_des_addr;

    if (desc == s_frameResetDesc) {
      s_scanLine = 0;
    }

    auto const width  = ctrl->getViewPortWidth();
    auto const height = ctrl->getViewPortHeight();
    int scanLine = (s_scanLine + VGA4_LinesCount / 2) % height;
    if (scanLine == 0) {
      ctrl->m_currentSignalItem = ctrl->m_signalList;
    }

    auto const lines  = ctrl->m_lines;
    auto lineIndex = scanLine & (VGA4_LinesCount - 1);

    for (int i = 0; i < VGA4_LinesCount / 2; ++i) {

      auto src  = (uint8_t const *) s_viewPortVisible[scanLine];
      auto dest = (uint32_t*) lines[lineIndex];
      uint8_t* decpix = (uint8_t*) dest;
      auto const packedPaletteIndexQuad_to_signals = (uint32_t *) ctrl->getSignalsForScanline(scanLine);

      // optimization warn: horizontal resolution must be a multiple of 16!
      for (int col = 0; col < width; col += 16) {

        auto src1 = *(src + 0);
        auto src2 = *(src + 1);
        auto src3 = *(src + 2);
        auto src4 = *(src + 3);

        PSRAM_HACK;

        auto v1 = packedPaletteIndexQuad_to_signals[src1];
        auto v2 = packedPaletteIndexQuad_to_signals[src2];
        auto v3 = packedPaletteIndexQuad_to_signals[src3];
        auto v4 = packedPaletteIndexQuad_to_signals[src4];

        *(dest + 0) = v1;
        *(dest + 1) = v2;
        *(dest + 2) = v3;
        *(dest + 3) = v4;

        dest += 4;
        src += 4;
      }

      ctrl->decorateScanLinePixels(decpix, scanLine);
      ++lineIndex;
      ++scanLine;
    }

    s_scanLine += VGA4_LinesCount / 2;

    if (scanLine >= height) {
      ctrl->m_frameCounter++;
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
