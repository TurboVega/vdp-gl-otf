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
#include "vga2controller.h"
#include "devdrivers/swgenerator.h"

#pragma GCC optimize ("O2")
namespace fabgl {

static inline __attribute__((always_inline)) void VGA2_SETPIXELINROW(uint8_t * row, int x, int value) {
  int brow = x >> 3;
  row[brow] ^= (-value ^ row[brow]) & (0x80 >> (x & 7));
}

static inline __attribute__((always_inline)) int VGA2_GETPIXELINROW(uint8_t * row, int x) {
  int brow = x >> 3;
  return (row[brow] & (0x80 >> (x & 7))) != 0;
}

#define VGA2_INVERTPIXELINROW(row, x)       (row)[(x) >> 3] ^= (0x80 >> ((x) & 7))

static inline __attribute__((always_inline)) void VGA2_SETPIXEL(int x, int y, int value) {
  auto row = (uint8_t*) sgetScanline(y);
  int brow = x >> 3;
  row[brow] ^= (-value ^ row[brow]) & (0x80 >> (x & 7));
}

#define VGA2_GETPIXEL(x, y)                 VGA2_GETPIXELINROW((uint8_t*)m_viewPort[(y)], (x))

#define VGA2_INVERT_PIXEL(x, y)             VGA2_INVERTPIXELINROW((uint8_t*)m_viewPort[(y)], (x))
#define VGA2_COLUMNSQUANTUM 16

/*************************************************************************************/
/* VGA2Controller definitions */
VGA2Controller * VGA2Controller::s_instance = nullptr;

VGA2Controller::VGA2Controller()
  : VideoController(VGA2_LinesCount, VGA2_COLUMNSQUANTUM, NativePixelFormat::PALETTE2, 8, 1, ISRHandler)
{
  s_instance = this;
  m_painter = new Painter2();
  m_painter->postConstruct(256 * sizeof(uint64_t));
}

VGA2Controller::~VGA2Controller()
{
  s_instance = nullptr;
}

void VGA2Controller::packSignals(int index, uint8_t packed222, void * signals)
{
  auto _signals = (uint64_t *) signals;
  for (int i = 0; i < 256; ++i) {
    auto b = (uint8_t *) (_signals + i);
    for (int j = 0; j < 8; ++j) {
      auto aj = 7 - j;
      if ((index == 0 && ((1 << aj) & i) == 0) || (index == 1 && ((1 << aj) & i) != 0)) {
        b[j ^ 2] = m_HVSync | packed222;
      }
    }
  }
}

void IRAM_ATTR VGA2Controller::ISRHandler(void * arg)
{
  #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  auto s1 = getCycleCount();
  #endif

  auto ctrl = (VGA2Controller *) arg;

  if (I2S1.int_st.out_eof) {

    auto const desc = (lldesc_t*) I2S1.out_eof_des_addr;

    if (desc == s_frameResetDesc) {
      s_scanLine = 0;
    }

    auto const width  = ctrl->getViewPortWidth();
    auto const height = ctrl->getViewPortHeight();
    int scanLine = (s_scanLine + VGA2_LinesCount / 2) % height;
    if (scanLine == 0) {
      ctrl->m_currentSignalItem = ctrl->m_signalList;
    }

    auto const lines  = ctrl->m_lines;
    auto lineIndex = scanLine & (VGA2_LinesCount - 1);

    for (int i = 0; i < VGA2_LinesCount / 2; ++i) {

      auto src  = (uint8_t const *) s_viewPortVisible[scanLine];
      auto dest = (uint64_t*) lines[lineIndex];
      uint8_t* decpix = (uint8_t*) dest;
      auto const packedPaletteIndexOctet_to_signals = (uint64_t *) ctrl->getSignalsForScanline(scanLine);

      // optimization warn: horizontal resolution must be a multiple of 16!
      for (int col = 0; col < width; col += 16) {

        auto src1 = *(src + 0);
        auto src2 = *(src + 1);

        PSRAM_HACK;

        auto v1 = packedPaletteIndexOctet_to_signals[src1];
        auto v2 = packedPaletteIndexOctet_to_signals[src2];

        *(dest + 0) = v1;
        *(dest + 1) = v2;

        dest += 2;
        src += 2;
        
      }

      ctrl->decorateScanLinePixels(decpix, scanLine);

      ++lineIndex;
      ++scanLine;
    }

    s_scanLine += VGA2_LinesCount / 2;

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
