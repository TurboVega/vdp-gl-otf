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

#include "fabutils.h"
#include "vga64controller.h"
#include "devdrivers/swgenerator.h"
#pragma GCC optimize ("O2")
namespace fabgl {

/*************************************************************************************/
/* VGA64Controller definitions */
VGA64Controller * VGA64Controller::s_instance = nullptr;
VGA64Controller::VGA64Controller()
{
  s_instance = this;
  m_painter = new Painter64();
  m_painter->postConstruct();
  m_nativePixelFormat = NativePixelFormat::SBGR2222;
}

void VGA64Controller::init()
{
  VideoController::init();

  m_doubleBufferOverDMA = true;
}

void VGA64Controller::suspendBackgroundPrimitiveExecution()
{
  VideoController::suspendBackgroundPrimitiveExecution();
  if (m_primitiveProcessingSuspended == 1) {
    I2S1.int_clr.val     = 0xFFFFFFFF;
    I2S1.int_ena.out_eof = 0;
  }
}

void VGA64Controller::resumeBackgroundPrimitiveExecution()
{
  VideoController::resumeBackgroundPrimitiveExecution();
  if (m_primitiveProcessingSuspended == 0) {
    if (m_isr_handle == nullptr)
      esp_intr_alloc(ETS_I2S1_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1, VSyncInterrupt, this, &m_isr_handle);
    I2S1.int_clr.val     = 0xFFFFFFFF;
    I2S1.int_ena.out_eof = 1;
  }
}

void VGA64Controller::allocateViewPort()
{
  VideoController::allocateViewPort(MALLOC_CAP_DMA, getPainter()->getViewPortWidth);
}

void VGA64Controller::setResolution(VGATimings const& timings, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  VideoController::setResolution(timings, viewPortWidth, viewPortHeight, doubleBuffered);

  // fill view port
  for (int i = 0; i < m_viewPortHeight; ++i)
    fill(m_viewPort[i], 0, m_viewPortWidth, 0, 0, 0, false, false);

  // number of microseconds usable in VSynch ISR
  m_maxVSyncISRTime = ceil(1000000.0 / m_timings.frequency * m_timings.scanCount * m_HLineSize * (m_timings.VSyncPulse + m_timings.VBackPorch + m_timings.VFrontPorch + m_viewPortRow));

  startGPIOStream();
  resumeBackgroundPrimitiveExecution();
}

void VGA64Controller::onSetupDMABuffer(lldesc_t volatile * buffer, bool isStartOfVertFrontPorch, int scan, bool isVisible, int visibleRow)
{
  // generate interrupt at the beginning of vertical front porch
  if (isStartOfVertFrontPorch)
    buffer->eof = 1;
}

void IRAM_ATTR VGA64Controller::VSyncInterrupt(void * arg)
{
  if (I2S1.int_st.out_eof) {
    auto VGACtrl = (VGA64Controller*)arg;
    int64_t startTime = VGACtrl->backgroundPrimitiveTimeoutEnabled() ? esp_timer_get_time() : 0;
    Rect updateRect = Rect(SHRT_MAX, SHRT_MAX, SHRT_MIN, SHRT_MIN);
    VGACtrl->m_frameCounter++;
    do {
      Primitive prim;
      if (VGACtrl->getPrimitiveISR(&prim) == false)
        break;

      VGACtrl->execPrimitive(prim, updateRect, true);

      if (VGACtrl->m_primitiveProcessingSuspended)
        break;

    } while (!VGACtrl->backgroundPrimitiveTimeoutEnabled() || (startTime + VGACtrl->m_maxVSyncISRTime > esp_timer_get_time()));
    VGACtrl->showSprites(updateRect);
  }
  I2S1.int_clr.val = I2S1.int_st.val;
}

} // end of namespace
