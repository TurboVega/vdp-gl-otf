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
 * @brief This file contains fabgl::VGA2Controller definition.
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
#include "dispdrivers/videocontroller.h"

// must be a power of 2
#define VGA2_LinesCount 4

namespace fabgl {

/**
* @brief Represents the VGA 2 colors bitmapped controller
*
* This VGA controller allows just 2 colors, but requires less (1/8) RAM than VGAController, at the same resolution. Each pixel is represented
* by 1 bit, which is an index to a palette of 2 colors. Each byte of the frame buffer contains eight pixels.
* For example, a frame buffer of 640x480 requires about 40K of RAM.
* This controller also allows higher resolutions, up to 1280x768.
*
* VGA2Controller output consumes up to 10% of one CPU core (measured at 640x480x60Hz).
*
*
* This example initializes VGA Controller with 64 colors (2 visible at the same time) at 640x480:
*
*     fabgl::VGA2Controller displayController;
*     // the default assigns GPIO22 and GPIO21 to Red, GPIO19 and GPIO18 to Green, GPIO5 and GPIO4 to Blue, GPIO23 to HSync and GPIO15 to VSync
*     displayController.begin();
*     displayController.setResolution(VGA_640x480_60Hz);
*/
class VGA2Controller : public VideoController {

public:

  VGA2Controller();
  ~VGA2Controller();

  // unwanted methods
  VGA2Controller(VGA2Controller const&) = delete;
  void operator=(VGA2Controller const&) = delete;


  /**
   * @brief Returns the singleton instance of VGA2Controller class
   *
   * @return A pointer to VGA2Controller singleton object
   */
  static VGA2Controller * instance() { return s_instance; }

  protected:

  void packSignals(int index, uint8_t packed222, void * signals);

private:

  static void ISRHandler(void * arg);

  static VGA2Controller *     s_instance;
};

} // end of namespace
