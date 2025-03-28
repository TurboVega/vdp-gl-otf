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
 * @brief This file contains fabgl::VGA8Controller definition.
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

#define VGA8_LinesCount 4


namespace fabgl {

/**
* @brief Represents the VGA 8 colors bitmapped controller
*
* This VGA controller allows just 8 colors, but requires less (8/3) RAM than VGAController, at the same resolution. Each pixel is represented
* by 3 bits, which is an index to a palette of 8 colors. Every 3 bytes of the frame buffer there are 8 pixels.
* For example, a frame buffer of 640x480 requires about 113K of RAM.
*
* VGA8Controller output is very CPU intensive process and consumes up to 23% of one CPU core (measured at 640x480x60Hz). Anyway this allows to have
* more RAM free for your application.
*
*
* This example initializes VGA Controller with 64 colors (16 visible at the same time) at 640x480:
*
*     fabgl::VGA8Controller displayController;
*     // the default assigns GPIO22 and GPIO21 to Red, GPIO19 and GPIO18 to Green, GPIO5 and GPIO4 to Blue, GPIO23 to HSync and GPIO15 to VSync
*     displayController.begin();
*     displayController.setResolution(VGA_640x480_60Hz);
*/
class VGA8Controller : public VGAPalettedController {

public:

  VGA8Controller();
  ~VGA8Controller();

  // unwanted methods
  VGA8Controller(VGA8Controller const&) = delete;
  void operator=(VGA8Controller const&) = delete;
  /**
   * @brief Returns the singleton instance of VGA8Controller class
   *
   * @return A pointer to VGA8Controller singleton object
   */
  static VGA8Controller * instance() { return s_instance; }

  protected:

  void packSignals(int index, uint8_t packed222, void * signals);

  void directSetPixel(int x, int y, int value);

private:

  static void ISRHandler(void * arg);
  static VGA8Controller *     s_instance;

};

} // end of namespace
