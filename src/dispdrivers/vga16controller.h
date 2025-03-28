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

/**
 * @file
 *
 * @brief This file contains fabgl::VGA16Controller definition.
 */
#include <stdint.h>
#include <stddef.h>
#include <atomic>
#include <functional>

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "fabglconf.h"
#include "fabutils.h"
#include "devdrivers/swgenerator.h"
#include "displaycontroller.h"
#include "dispdrivers/vgapalettedcontroller.h"

#define VGA16_LinesCount 4


namespace fabgl {

/**
* @brief Represents the VGA 16 colors bitmapped controller
*
* This VGA controller allows just 16 colors, but requires less (1/2) RAM than VGAController, at the same resolution. Each pixel is represented
* by 4 bits, which is an index to a palette of 16 colors. Each byte of the frame buffer contains two pixels.
* For example, a frame buffer of 640x480 requires about 153K of RAM.
*
* VGA16Controller output is a CPU intensive process and consumes up to 19% of one CPU core (measured at 640x480x60Hz). Anyway this allows to have
* more RAM free for your application.
*
*
* This example initializes VGA Controller with 64 colors (16 visible at the same time) at 640x480:
*
*     fabgl::VGA16Controller displayController;
*     // the default assigns GPIO22 and GPIO21 to Red, GPIO19 and GPIO18 to Green, GPIO5 and GPIO4 to Blue, GPIO23 to HSync and GPIO15 to VSync
*     displayController.begin();
*     displayController.setResolution(VGA_640x480_60Hz);
*/
class VGA16Controller : public VGAPalettedController {

public:

  VGA16Controller();
  ~VGA16Controller();

  // unwanted methods
  VGA16Controller(VGA16Controller const&) = delete;
  void operator=(VGA16Controller const&)  = delete;
  /**
   * @brief Returns the singleton instance of VGA16Controller class
   *
   * @return A pointer to VGA16Controller singleton object
   */
  static VGA16Controller * instance() { return s_instance; }

  protected:

  void packSignals(int index, uint8_t packed222, void * signals);

private:

  static void ISRHandler(void * arg);

  static VGA16Controller *    s_instance;

};

} // end of namespace
