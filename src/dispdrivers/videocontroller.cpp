/*
  videocontroller - Controls video output

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

#include "displaycontroller.h"

#include <string.h>
#include <limits.h>
#include <math.h>

#include "freertos/task.h"

#include "fabutils.h"
#include "images/cursors.h"


#pragma GCC optimize ("O2")


namespace fabgl {

#if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  volatile uint64_t s_vgapalctrlcycles = 0;
#endif

volatile uint8_t * * VideoController::s_viewPort;
volatile uint8_t * * VideoController::s_viewPortVisible;
lldesc_t volatile *  VideoController::s_frameResetDesc;
volatile int         VideoController::s_scanLine;
volatile int         VideoController::s_scanWidth;
volatile int         VideoController::s_viewPortHeight;

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// Sprite implementation


Sprite::Sprite()
{
  x                       = 0;
  y                       = 0;
  currentFrame            = 0;
  frames                  = nullptr;
  framesCount             = 0;
  savedBackgroundWidth    = 0;
  savedBackgroundHeight   = 0;
  savedBackground         = nullptr; // allocated or reallocated when bitmaps are added
  savedX                  = 0;
  savedY                  = 0;
  collisionDetectorObject = nullptr;
  visible                 = true;
  isStatic                = false;
  allowDraw               = true;
  hardware                = false;
  paintOptions            = PaintOptions();
}


Sprite::~Sprite()
{
  framesCount = 0;
  free(frames);
  free(savedBackground);
}


void Sprite::clearBitmaps()
{
  framesCount = 0;
  auto framesPtr = frames;
  frames = nullptr;
  free(framesPtr);
}


Sprite * Sprite::addBitmap(Bitmap * bitmap)
{
  auto newFrames = (Bitmap**) realloc(frames, sizeof(Bitmap*) * (framesCount + 1));
  newFrames[framesCount] = bitmap;
  frames = newFrames;
  ++framesCount;
  return this;
}


Sprite * Sprite::addBitmap(Bitmap * bitmap[], int count)
{
  auto newFrames = (Bitmap**) realloc(frames, sizeof(Bitmap*) * (framesCount + count));
  for (int i = 0; i < count; ++i)
    newFrames[framesCount + i] = bitmap[i];
  frames = newFrames;
  framesCount += count;
  return this;
}


Sprite * Sprite::moveBy(int offsetX, int offsetY)
{
  x += offsetX;
  y += offsetY;
  return this;
}


Sprite * Sprite::moveBy(int offsetX, int offsetY, int wrapAroundWidth, int wrapAroundHeight)
{
  x += offsetX;
  y += offsetY;
  if (x > wrapAroundWidth)
    x = - (int) getWidth();
  if (x < - (int) getWidth())
    x = wrapAroundWidth;
  if (y > wrapAroundHeight)
    y = - (int) getHeight();
  if (y < - (int) getHeight())
    y = wrapAroundHeight;
  return this;
}


Sprite * Sprite::moveTo(int x, int y)
{
  this->x = x;
  this->y = y;
  return this;
}


VideoController::VideoController(int linesCount, int columnsQuantum, NativePixelFormat nativePixelFormat, int viewPortRatioDiv, int viewPortRatioMul, intr_handler_t isrHandler, int signalTableSize)
  : m_columnsQuantum(columnsQuantum),
    m_nativePixelFormat(nativePixelFormat),
    m_viewPortRatioDiv(viewPortRatioDiv),
    m_viewPortRatioMul(viewPortRatioMul),
    m_isrHandler(isrHandler),
    m_signalTableSize(signalTableSize),
    m_primDynMemPool(FABGLIB_PRIMITIVES_DYNBUFFERS_SIZE)
{
  m_linesCount = linesCount;
  m_lines   = (volatile uint8_t**) heap_caps_malloc(sizeof(uint8_t*) * m_linesCount, MALLOC_CAP_32BIT | MALLOC_CAP_INTERNAL);
  m_palette = (RGB222*) heap_caps_malloc(sizeof(RGB222) * getPaletteSize(), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
}

VideoController::~VideoController()
{
  m_execQueue                           = nullptr;
  m_backgroundPrimitiveExecutionEnabled = true;
  m_sprites                             = nullptr;
  m_spritesCount                        = 0;
  m_doubleBuffered                      = false;
  m_mouseCursor.visible                 = false;
  m_backgroundPrimitiveTimeoutEnabled   = true;
  m_spritesHidden                       = true;

  vQueueDelete(m_execQueue);
  delete m_painter;
  heap_caps_free(m_palette);
  heap_caps_free(m_lines);
  for (auto it = m_signalMaps.begin(); it != m_signalMaps.end();) {
    if (it->second) {
      heap_caps_free((void *)it->second);
    }
    it = m_signalMaps.erase(it);
  }
  deleteSignalList(m_signalList);
}


int VideoController::queueSize = FABGLIB_DEFAULT_DISPLAYCONTROLLER_QUEUE_SIZE;


void VideoController::setDoubleBuffered(bool value)
{
  m_doubleBuffered = value;
  if (m_execQueue)
    vQueueDelete(m_execQueue);
  // on double buffering a queue of single element is enough and necessary (see addPrimitive() for details)
  m_execQueue = xQueueCreate(value ? 1 : VideoController::queueSize, sizeof(Primitive));
}


void VideoController::addPrimitive(Primitive & primitive)
{
  if ((m_backgroundPrimitiveExecutionEnabled && m_doubleBuffered == false) || primitive.cmd == PrimitiveCmd::SwapBuffers) {
    primitiveReplaceDynamicBuffers(primitive);
    xQueueSendToBack(m_execQueue, &primitive, portMAX_DELAY);

    if (m_doubleBuffered) {
      // wait notufy from PrimitiveCmd::SwapBuffers executor
      ulTaskNotifyTake(true, portMAX_DELAY);
    }

  } else {
    Rect updateRect = Rect(SHRT_MAX, SHRT_MAX, SHRT_MIN, SHRT_MIN);
    execPrimitive(primitive, updateRect, false);
    showSprites(updateRect);
  }
}


// some primitives require additional buffers (like drawPath and fillPath).
// this function copies primitive data into an allocated buffer (using LightMemoryPool allocator) that
// will be released inside primitive drawing code.
void VideoController::primitiveReplaceDynamicBuffers(Primitive & primitive)
{
  switch (primitive.cmd) {
    case PrimitiveCmd::DrawPath:
    case PrimitiveCmd::FillPath:
    {
      int sz = primitive.path.pointsCount * sizeof(Point);
      if (sz < FABGLIB_PRIMITIVES_DYNBUFFERS_SIZE) {
        void * newbuf = nullptr;
        // wait until we have enough free space
        while ((newbuf = m_primDynMemPool.alloc(sz)) == nullptr)
          taskYIELD();
        memcpy(newbuf, primitive.path.points, sz);
        primitive.path.points = (Point*)newbuf;
        primitive.path.freePoints = true;
      }
      break;
    }
    case PrimitiveCmd::DrawTransformedBitmap:
    {
      int sz = sizeof(float) * 9;
      void * newbuf = nullptr;
      // wait until we have enough free space
      while ((newbuf = m_primDynMemPool.alloc(sz)) == nullptr)
        taskYIELD();
      memcpy(newbuf, primitive.bitmapTransformedDrawingInfo.transformMatrix, sz);
      primitive.bitmapTransformedDrawingInfo.transformMatrix = (float*)newbuf;
      // copy the inv matrix too
      while ((newbuf = m_primDynMemPool.alloc(sz)) == nullptr)
        taskYIELD();
      memcpy(newbuf, primitive.bitmapTransformedDrawingInfo.transformInverse, sz);
      primitive.bitmapTransformedDrawingInfo.transformInverse = (float*)newbuf;
      primitive.bitmapTransformedDrawingInfo.freeMatrix = true;
      break;
    }

    default:
      break;
  }
}


// call this only inside an ISR
bool IRAM_ATTR VideoController::getPrimitiveISR(Primitive * primitive)
{
  return xQueueReceiveFromISR(m_execQueue, primitive, nullptr);
}


bool VideoController::getPrimitive(Primitive * primitive, int timeOutMS)
{
  return xQueueReceive(m_execQueue, primitive, msToTicks(timeOutMS));
}


// cannot be called inside an ISR
void VideoController::waitForPrimitives()
{
  Primitive p;
  xQueuePeek(m_execQueue, &p, portMAX_DELAY);
}


void VideoController::primitivesExecutionWait()
{
  if (m_backgroundPrimitiveExecutionEnabled) {
    while (uxQueueMessagesWaiting(m_execQueue) > 0)
      ;
  }
}


// When false primitives are executed immediately, otherwise they are added to the primitive queue
// When set to false the queue is emptied executing all pending primitives
// Cannot be nested
void VideoController::enableBackgroundPrimitiveExecution(bool value)
{
  if (value != m_backgroundPrimitiveExecutionEnabled) {
    if (value) {
      resumeBackgroundPrimitiveExecution();
    } else {
      suspendBackgroundPrimitiveExecution();
      processPrimitives();
    }
    m_backgroundPrimitiveExecutionEnabled = value;
  }
}


// Use for fast queue processing. Warning, may generate flickering because don't care of vertical sync
// Do not call inside ISR
void IRAM_ATTR VideoController::processPrimitives()
{
  suspendBackgroundPrimitiveExecution();
  Rect updateRect = Rect(SHRT_MAX, SHRT_MAX, SHRT_MIN, SHRT_MIN);
  Primitive prim;
  while (xQueueReceive(m_execQueue, &prim, 0) == pdTRUE)
    execPrimitive(prim, updateRect, false);
  showSprites(updateRect);
  resumeBackgroundPrimitiveExecution();
  Primitive p(PrimitiveCmd::Refresh, updateRect);
  addPrimitive(p);
}


void VideoController::setSprites(Sprite * sprites, int count, int spriteSize)
{
  suspendBackgroundPrimitiveExecution();
  auto updateRect = Rect(0, 0, getViewPortWidth() - 1, getViewPortHeight() - 1);
  hideSprites(updateRect);
  m_spritesCount = 0;
  m_sprites      = sprites;
  m_spriteSize   = spriteSize;
  m_spritesCount = count;

  // allocates background buffer
  if (!isDoubleBuffered()) {
    uint8_t * spritePtr = (uint8_t*)m_sprites;
    for (int i = 0; i < m_spritesCount; ++i, spritePtr += m_spriteSize) {
      Sprite * sprite = (Sprite*) spritePtr;
      if (sprite->hardware) {
        if (sprite->savedBackground) {
          free(sprite->savedBackground);
          sprite->savedBackground = nullptr;
        }
        continue;
      }
      int reqBackBufferSize = 0;
      for (int i = 0; i < sprite->framesCount; ++i)
        reqBackBufferSize = tmax(reqBackBufferSize, sprite->frames[i]->width * sprite->frames[i]->height);
      if (reqBackBufferSize > 0)
        sprite->savedBackground = (uint8_t*) realloc(sprite->savedBackground, reqBackBufferSize);
    }
  }
  resumeBackgroundPrimitiveExecution();
  Primitive p(PrimitiveCmd::RefreshSprites);
  addPrimitive(p);
}


Sprite * IRAM_ATTR VideoController::getSprite(int index)
{
  return (Sprite*) ((uint8_t*)m_sprites + index * m_spriteSize);
}


void VideoController::refreshSprites()
{
  Primitive p(PrimitiveCmd::RefreshSprites);
  addPrimitive(p);
}


void IRAM_ATTR VideoController::hideSprites(Rect & updateRect)
{
  if (!m_spritesHidden) {
    m_spritesHidden = true;

    // normal sprites
    if (spritesCount() > 0 && !isDoubleBuffered()) {
      // restore saved backgrounds
      for (int i = spritesCount() - 1; i >= 0; --i) {
        Sprite * sprite = getSprite(i);
        if (sprite->allowDraw && sprite->savedBackgroundWidth > 0) {
          int savedX = sprite->savedX;
          int savedY = sprite->savedY;
          int savedWidth  = sprite->savedBackgroundWidth;
          int savedHeight = sprite->savedBackgroundHeight;
          Bitmap bitmap(savedWidth, savedHeight, sprite->savedBackground, PixelFormat::Native);
          absDrawBitmap(savedX, savedY, &bitmap, nullptr, true);
          updateRect = updateRect.merge(Rect(savedX, savedY, savedX + savedWidth - 1, savedY + savedHeight - 1));
          sprite->savedBackgroundWidth = sprite->savedBackgroundHeight = 0;
        }
      }
    }

    // mouse cursor sprite
    Sprite * mouseSprite = mouseCursor();
    if (mouseSprite->savedBackgroundWidth > 0) {
      int savedX = mouseSprite->savedX;
      int savedY = mouseSprite->savedY;
      int savedWidth  = mouseSprite->savedBackgroundWidth;
      int savedHeight = mouseSprite->savedBackgroundHeight;
      Bitmap bitmap(savedWidth, savedHeight, mouseSprite->savedBackground, PixelFormat::Native);
      absDrawBitmap(savedX, savedY, &bitmap, nullptr, true);
      updateRect = updateRect.merge(Rect(savedX, savedY, savedX + savedWidth - 1, savedY + savedHeight - 1));
      mouseSprite->savedBackgroundWidth = mouseSprite->savedBackgroundHeight = 0;
    }
  }
}


void IRAM_ATTR VideoController::showSprites(Rect & updateRect)
{
  if (m_spritesHidden) {
    m_spritesHidden = false;
    auto options = paintState().paintOptions;

    // normal sprites
    // save backgrounds and draw sprites
    for (int i = 0; i < spritesCount(); ++i) {
      Sprite * sprite = getSprite(i);
      if (!sprite->hardware &&
          sprite->visible && sprite->allowDraw && sprite->getFrame()) {
        // save sprite X and Y so other threads can change them without interferring
        int spriteX = sprite->x;
        int spriteY = sprite->y;
        Bitmap const * bitmap = sprite->getFrame();
        int bitmapWidth  = bitmap->width;
        int bitmapHeight = bitmap->height;
        paintState().paintOptions = sprite->paintOptions;
        absDrawBitmap(spriteX, spriteY, bitmap, sprite->savedBackground, true);
        sprite->savedX = spriteX;
        sprite->savedY = spriteY;
        sprite->savedBackgroundWidth  = bitmapWidth;
        sprite->savedBackgroundHeight = bitmapHeight;
        if (sprite->isStatic)
          sprite->allowDraw = false;
        updateRect = updateRect.merge(Rect(spriteX, spriteY, spriteX + bitmapWidth - 1, spriteY + bitmapHeight - 1));
      }
    }

    // mouse cursor sprite
    // save backgrounds and draw mouse cursor
    Sprite * mouseSprite = mouseCursor();
    if (mouseSprite->visible && mouseSprite->getFrame()) {
      // save sprite X and Y so other threads can change them without interferring
      int spriteX = mouseSprite->x;
      int spriteY = mouseSprite->y;
      Bitmap const * bitmap = mouseSprite->getFrame();
      int bitmapWidth  = bitmap->width;
      int bitmapHeight = bitmap->height;
      paintState().paintOptions = PaintOptions();
      absDrawBitmap(spriteX, spriteY, bitmap, mouseSprite->savedBackground, true);
      mouseSprite->savedX = spriteX;
      mouseSprite->savedY = spriteY;
      mouseSprite->savedBackgroundWidth  = bitmapWidth;
      mouseSprite->savedBackgroundHeight = bitmapHeight;
      updateRect = updateRect.merge(Rect(spriteX, spriteY, spriteX + bitmapWidth - 1, spriteY + bitmapHeight - 1));
    }

    paintState().paintOptions = options;
  }
}


// cursor = nullptr -> disable mouse
void VideoController::setMouseCursor(Cursor * cursor)
{
  if (cursor == nullptr || &cursor->bitmap != m_mouseCursor.getFrame()) {
    m_mouseCursor.visible = false;
    m_mouseCursor.clearBitmaps();

    refreshSprites();
    processPrimitives();
    primitivesExecutionWait();

    if (cursor) {
      m_mouseCursor.moveBy(+m_mouseHotspotX, +m_mouseHotspotY);
      m_mouseHotspotX = cursor->hotspotX;
      m_mouseHotspotY = cursor->hotspotY;
      m_mouseCursor.addBitmap(&cursor->bitmap);
      m_mouseCursor.visible = true;
      m_mouseCursor.moveBy(-m_mouseHotspotX, -m_mouseHotspotY);
      if (!isDoubleBuffered())
        m_mouseCursor.savedBackground = (uint8_t*) realloc(m_mouseCursor.savedBackground, cursor->bitmap.width * cursor->bitmap.height);
    }
    refreshSprites();
  }
}


void VideoController::setMouseCursor(CursorName cursorName)
{
  setMouseCursor(&CURSORS[(int)cursorName]);
}


void VideoController::setMouseCursorPos(int X, int Y)
{
  m_mouseCursor.moveTo(X - m_mouseHotspotX, Y - m_mouseHotspotY);
  refreshSprites();
}


void VideoController::drawSpriteScanLine(uint8_t * pixelData, int scanRow, int scanWidth, int viewportHeight) {
    // normal sprites
    for (int i = 0; i < spritesCount(); ++i) {
      Sprite * sprite = getSprite(i);
      if (sprite->hardware &&
          sprite->visible && sprite->allowDraw && sprite->getFrame()) {
        auto spriteFrame = sprite->getFrame();
        int spriteWidth = spriteFrame->width;
        int spriteHeight = spriteFrame->height;

        int spriteY = sprite->y;
        int spriteYend = spriteY + spriteHeight;
        if (scanRow < spriteY) continue;
        if (scanRow >= spriteYend) continue;
        int offsetY = scanRow - spriteY;

        int spriteX = sprite->x;
        if (spriteX >= scanWidth) continue;
        int spriteXend = spriteX + spriteWidth;
        if (spriteXend <= 0) continue;

        int offsetX = (spriteX < 0 ? -spriteX : 0);
        int drawWidth =
          (spriteXend > scanWidth ?
            scanWidth - spriteX :
            spriteWidth - offsetX);

        switch (spriteFrame->format) {
          case PixelFormat::RGBA8888: {
              auto src = (const uint32_t*)(spriteFrame->data) + (offsetY * spriteWidth) + offsetX;
              auto pos = spriteX + offsetX;
              while (drawWidth--) {
                auto src_pix = *src++;
                if (src_pix & 0xFF000000) {
                  auto r = (src_pix & 0x000000C0) >> (8-2);
                  auto g = (src_pix & 0x0000C000) >> (16-4);
                  auto b = (src_pix & 0x00C00000) >> (24-6);
                  pixelData[pos^2] = r | g | b | m_HVSync;
                }
                pos++;
              }
            }
            break;

          case PixelFormat::RGBA2222: {
              auto src = spriteFrame->data + (offsetY * spriteWidth) + offsetX;
              pixelData += spriteX + offsetX;
              auto hv4 = (((uint32_t) m_HVSync) << 24) |
                        (((uint32_t) m_HVSync) << 16) |
                        (((uint32_t) m_HVSync) << 8) |
                        ((uint32_t) m_HVSync);

              while (drawWidth) {
                if (drawWidth >= 4 && !((uint32_t)(void*)pixelData & 3))
                {
                  // Do a full word (4 pixels)
                  auto src_pix = *((uint32_t*)src);
                  auto alphas = src_pix & 0xC0C0C0C0; 
                  if (alphas == 0xC0C0C0C0) {
                    src_pix = (src_pix & 0x3F3F3F3F) | hv4; 
                    *((uint32_t*)pixelData) = (src_pix << 16) | (src_pix >> 16);
                    src += 4;
                    pixelData += 4;
                    drawWidth -= 4;
                    continue;
                  } else if (alphas == 0x00000000) {
                    src += 4;
                    pixelData += 4;
                    drawWidth -= 4;
                    continue;
                  }
                }

                // Just do a single byte (1 pixel)
                if (*src & 0xC0) {
                  auto rgb = *src & 0x3F;
                  *((uint8_t*)(((uint32_t)(void*)pixelData)^2)) = rgb | m_HVSync;
                }
                src++;
                pixelData++;
                drawWidth--;
              }
            }
            break;
        }
      }
    }
}


void IRAM_ATTR VideoController::execPrimitive(Primitive const & prim, Rect & updateRect, bool insideISR)
{
  switch (prim.cmd) {
    case PrimitiveCmd::Flush:
      break;
    case PrimitiveCmd::Refresh:
      updateRect = updateRect.merge(prim.rect);
      break;
    case PrimitiveCmd::Reset:
      resetPaintState();
      break;
    case PrimitiveCmd::SetPenColor:
      paintState().penColor = prim.color;
      break;
    case PrimitiveCmd::SetBrushColor:
      paintState().brushColor = prim.color;
      break;
    case PrimitiveCmd::SetPixel:
      setPixelAt( (PixelDesc) { prim.position, getActualPenColor() }, updateRect );
      break;
    case PrimitiveCmd::SetPixelAt:
      setPixelAt(prim.pixelDesc, updateRect);
      break;
    case PrimitiveCmd::MoveTo:
      paintState().position = Point(prim.position.X + paintState().origin.X, prim.position.Y + paintState().origin.Y);
      break;
    case PrimitiveCmd::LineTo:
      lineTo(prim.position, updateRect);
      break;
    case PrimitiveCmd::FillRect:
      fillRect(prim.rect, getActualBrushColor(), updateRect);
      break;
    case PrimitiveCmd::DrawRect:
      drawRect(prim.rect, updateRect);
      break;
    case PrimitiveCmd::FillEllipse:
      fillEllipse(paintState().position.X, paintState().position.Y, prim.size, getActualBrushColor(), updateRect);
      break;
    case PrimitiveCmd::DrawEllipse:
      drawEllipse(prim.size, updateRect);
      break;
    case PrimitiveCmd::DrawArc:
      drawArc(prim.rect, updateRect);
      break;
    case PrimitiveCmd::FillSegment:
      fillSegment(prim.rect, updateRect);
      break;
    case PrimitiveCmd::FillSector:
      fillSector(prim.rect, updateRect);
      break;
    case PrimitiveCmd::Clear:
      updateRect = updateRect.merge(Rect(0, 0, getViewPortWidth() - 1, getViewPortHeight() - 1));
      clear(updateRect);
      break;
    case PrimitiveCmd::VScroll:
      updateRect = updateRect.merge(Rect(paintState().scrollingRegion.X1, paintState().scrollingRegion.Y1, paintState().scrollingRegion.X2, paintState().scrollingRegion.Y2));
      VScroll(prim.ivalue, updateRect);
      break;
    case PrimitiveCmd::HScroll:
      updateRect = updateRect.merge(Rect(paintState().scrollingRegion.X1, paintState().scrollingRegion.Y1, paintState().scrollingRegion.X2, paintState().scrollingRegion.Y2));
      HScroll(prim.ivalue, updateRect);
      break;
    case PrimitiveCmd::DrawGlyph:
      drawGlyph(prim.glyph, paintState().glyphOptions, paintState().penColor, paintState().brushColor, updateRect);
      break;
    case PrimitiveCmd::SetGlyphOptions:
      paintState().glyphOptions = prim.glyphOptions;
      break;
    case PrimitiveCmd::SetPaintOptions:
      paintState().paintOptions = prim.paintOptions;
      break;
    case PrimitiveCmd::InvertRect:
      invertRect(prim.rect, updateRect);
      break;
    case PrimitiveCmd::CopyRect:
      copyRect(prim.rect, updateRect);
      break;
    case PrimitiveCmd::SetScrollingRegion:
      paintState().scrollingRegion = prim.rect;
      break;
    case PrimitiveCmd::SwapFGBG:
      swapFGBG(prim.rect, updateRect);
      break;
    case PrimitiveCmd::RenderGlyphsBuffer:
      renderGlyphsBuffer(prim.glyphsBufferRenderInfo, updateRect);
      break;
    case PrimitiveCmd::DrawBitmap:
      drawBitmap(prim.bitmapDrawingInfo, updateRect);
      break;
    case PrimitiveCmd::CopyToBitmap:
      copyToBitmap(prim.bitmapDrawingInfo);
      break;
    case PrimitiveCmd::DrawTransformedBitmap: {
      if (insideISR) {
        uint32_t cp0_regs[18];
        // get FPU state
        uint32_t cp_state = xthal_get_cpenable();
        
        if (cp_state) {
          // Save FPU registers
          xthal_save_cp0(cp0_regs);
        } else {
          // enable FPU
          xthal_set_cpenable(1);
        }
        
        drawBitmapWithTransform(prim.bitmapTransformedDrawingInfo, updateRect);

        if (cp_state) {
          // Restore FPU registers
          xthal_restore_cp0(cp0_regs);
        } else {
          // turn it back off
          xthal_set_cpenable(0);
        }
      } else {
        drawBitmapWithTransform(prim.bitmapTransformedDrawingInfo, updateRect);
      }
    }  break;
    case PrimitiveCmd::RefreshSprites:
      hideSprites(updateRect);
      showSprites(updateRect);
      break;
    case PrimitiveCmd::SwapBuffers:
      swapBuffers();
      updateRect = updateRect.merge(Rect(0, 0, getViewPortWidth() - 1, getViewPortHeight() - 1));
      if (insideISR) {
        vTaskNotifyGiveFromISR(prim.notifyTask, nullptr);
      } else {
        xTaskNotifyGive(prim.notifyTask);
      }
      break;
    case PrimitiveCmd::DrawPath:
      drawPath(prim.path, updateRect);
      break;
    case PrimitiveCmd::FillPath:
      fillPath(prim.path, getActualBrushColor(), updateRect);
      break;
    case PrimitiveCmd::SetOrigin:
      paintState().origin = prim.position;
      updateAbsoluteClippingRect();
      break;
    case PrimitiveCmd::SetClippingRect:
      paintState().clippingRect = prim.rect;
      updateAbsoluteClippingRect();
      break;
    case PrimitiveCmd::SetPenWidth:
      paintState().penWidth = imax(1, prim.ivalue);
      break;
    case PrimitiveCmd::SetLineEnds:
      paintState().lineEnds = prim.lineEnds;
      break;
    case PrimitiveCmd::SetLinePattern:
      paintState().linePattern = prim.linePattern;
      break;
    case PrimitiveCmd::SetLinePatternLength:
      paintState().linePatternLength = imin(64, imax(1, prim.ivalue));
      break;
    case PrimitiveCmd::SetLinePatternOffset:
      paintState().linePattern.offset = prim.ivalue % paintState().linePatternLength;
      break;
    case PrimitiveCmd::SetLineOptions:
      paintState().lineOptions = prim.lineOptions;
      break;
  }
}

void VideoController::init()
{
  CurrentVideoMode::set(VideoMode::VGA);

  m_DMABuffers                   = nullptr;
  m_DMABuffersCount              = 0;
  m_DMABuffersHead               = nullptr;
  m_DMABuffersVisible            = nullptr;
  m_primitiveProcessingSuspended = 1; // >0 suspended
  m_isr_handle                   = nullptr;
  m_doubleBufferOverDMA          = false;
  m_viewPort                     = nullptr;
  m_viewPortMemoryPool           = nullptr;
  m_taskProcessingPrimitives     = false;
  m_primitiveExecTask            = nullptr;
  m_processPrimitivesOnBlank     = false;

  m_GPIOStream.begin();
}

// initializer for 64 colors configuration
void VideoController::begin(gpio_num_t red1GPIO, gpio_num_t red0GPIO, gpio_num_t green1GPIO, gpio_num_t green0GPIO, gpio_num_t blue1GPIO, gpio_num_t blue0GPIO, gpio_num_t HSyncGPIO, gpio_num_t VSyncGPIO)
{
  init();

  // GPIO configuration for bit 0
  setupGPIO(red0GPIO,   VGA_RED_BIT,   GPIO_MODE_OUTPUT);
  setupGPIO(green0GPIO, VGA_GREEN_BIT, GPIO_MODE_OUTPUT);
  setupGPIO(blue0GPIO,  VGA_BLUE_BIT,  GPIO_MODE_OUTPUT);

  // GPIO configuration for bit 1
  setupGPIO(red1GPIO,   VGA_RED_BIT + 1,   GPIO_MODE_OUTPUT);
  setupGPIO(green1GPIO, VGA_GREEN_BIT + 1, GPIO_MODE_OUTPUT);
  setupGPIO(blue1GPIO,  VGA_BLUE_BIT + 1,  GPIO_MODE_OUTPUT);

  // GPIO configuration for VSync and HSync
  setupGPIO(HSyncGPIO, VGA_HSYNC_BIT, GPIO_MODE_OUTPUT);
  setupGPIO(VSyncGPIO, VGA_VSYNC_BIT, GPIO_MODE_OUTPUT);


  RGB222::lowBitOnly = false;
  m_bitsPerChannel = 2;
}


// initializer for default configuration
void VideoController::begin()
{
  begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_23, GPIO_NUM_15);
}


void VideoController::end()
{
  if (m_DMABuffers) {
    suspendBackgroundPrimitiveExecution();
    vTaskDelay(50 / portTICK_PERIOD_MS);
    m_GPIOStream.stop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
    if (m_isr_handle) {
      esp_intr_free(m_isr_handle);
      m_isr_handle = nullptr;
    }
    freeBuffers();
  }
  if (m_primitiveExecTask) {
    vTaskDelete(m_primitiveExecTask);
    m_primitiveExecTask = nullptr;
    m_taskProcessingPrimitives = false;
  }
}


void VideoController::setupGPIO(gpio_num_t gpio, int bit, gpio_mode_t mode)
{
  configureGPIO(gpio, mode);
  gpio_matrix_out(gpio, I2S1O_DATA_OUT0_IDX + bit, false, false);
}


void VideoController::freeBuffers()
{
  if (m_DMABuffersCount > 0) {
    heap_caps_free((void*)m_HBlankLine_withVSync);
    heap_caps_free((void*)m_HBlankLine);

    freeViewPort();

    setDMABuffersCount(0);
  }
}


void VideoController::freeViewPort()
{
  if (m_viewPortMemoryPool) {
    for (auto poolPtr = m_viewPortMemoryPool; *poolPtr; ++poolPtr)
      heap_caps_free((void*) *poolPtr);
    heap_caps_free(m_viewPortMemoryPool);
    m_viewPortMemoryPool = nullptr;
  }
  if (m_viewPort) {
    heap_caps_free(m_viewPort);
    m_viewPort = nullptr;
  }
  if (isDoubleBuffered())
    heap_caps_free(m_viewPortVisible);
  m_viewPortVisible = nullptr;

  for (int i = 0; i < m_linesCount; ++i) {
    heap_caps_free((void*)m_lines[i]);
    m_lines[i] = nullptr;
  }
}


// Can be used to change buffers count, maintainig already set pointers.
// If m_doubleBufferOverDMA = true, uses m_DMABuffersHead and m_DMABuffersVisible to implement
// double buffer on DMA level.
bool VideoController::setDMABuffersCount(int buffersCount)
{
  if (buffersCount == 0) {
    if (m_DMABuffersVisible && m_DMABuffersVisible != m_DMABuffers)
      heap_caps_free( (void*) m_DMABuffersVisible );
    heap_caps_free( (void*) m_DMABuffers );
    m_DMABuffers = nullptr;
    m_DMABuffersVisible = nullptr;
    m_DMABuffersCount = 0;
    return true;
  }

  if (buffersCount != m_DMABuffersCount) {

    // buffers head
    if (m_doubleBufferOverDMA && m_DMABuffersHead == nullptr) {
      m_DMABuffersHead = (lldesc_t*) heap_caps_malloc(sizeof(lldesc_t), MALLOC_CAP_DMA);
      m_DMABuffersHead->eof    = m_DMABuffersHead->sosf = m_DMABuffersHead->offset = 0;
      m_DMABuffersHead->owner  = 1;
      m_DMABuffersHead->size   = 0;
      m_DMABuffersHead->length = 0;
      m_DMABuffersHead->buf    = m_HBlankLine;   // dummy valid address. Setting nullptr crashes DMA!
      m_DMABuffersHead->qe.stqe_next = nullptr;  // this will be set before the first frame
    }

    // (re)allocate and initialize DMA descs
    m_DMABuffers = (lldesc_t*) heap_caps_realloc((void*)m_DMABuffers, buffersCount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    if (m_doubleBufferOverDMA && isDoubleBuffered())
      m_DMABuffersVisible = (lldesc_t*) heap_caps_realloc((void*)m_DMABuffersVisible, buffersCount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    else
      m_DMABuffersVisible = m_DMABuffers;
    if (!m_DMABuffers || !m_DMABuffersVisible)
      return false;

    auto buffersHead = m_DMABuffersHead ? m_DMABuffersHead : &m_DMABuffers[0];

    for (int i = 0; i < buffersCount; ++i) {
      m_DMABuffers[i].eof          = 0;
      m_DMABuffers[i].sosf         = 0;
      m_DMABuffers[i].offset       = 0;
      m_DMABuffers[i].owner        = 1;
      m_DMABuffers[i].qe.stqe_next = (lldesc_t*) (i == buffersCount - 1 ? buffersHead : &m_DMABuffers[i + 1]);
      if (m_doubleBufferOverDMA && isDoubleBuffered()) {
        m_DMABuffersVisible[i].eof          = 0;
        m_DMABuffersVisible[i].sosf         = 0;
        m_DMABuffersVisible[i].offset       = 0;
        m_DMABuffersVisible[i].owner        = 1;
        m_DMABuffersVisible[i].qe.stqe_next = (lldesc_t*) (i == buffersCount - 1 ? buffersHead : &m_DMABuffersVisible[i + 1]);
      }
    }

    m_DMABuffersCount = buffersCount;
  }

  return true;
}


// modeline syntax:
//   "label" clock_mhz hdisp hsyncstart hsyncend htotal vdisp vsyncstart vsyncend vtotal [(+HSync | -HSync) (+VSync | -VSync)] [DoubleScan | QuadScan] [FrontPorchBegins | SyncBegins | BackPorchBegins | VisibleBegins] [MultiScanBlank]
bool VideoController::convertModelineToTimings(char const * modeline, VGATimings * timings)
{
  float freq;
  int hdisp, hsyncstart, hsyncend, htotal, vdisp, vsyncstart, vsyncend, vtotal;
  char HSyncPol = 0, VSyncPol = 0;
  int pos = 0;

  int count = sscanf(modeline, "\"%[^\"]\" %g %d %d %d %d %d %d %d %d %n", timings->label, &freq, &hdisp, &hsyncstart, &hsyncend, &htotal, &vdisp, &vsyncstart, &vsyncend, &vtotal, &pos);

  if (count == 10 && pos > 0) {

    timings->frequency      = freq * 1000000;
    timings->HVisibleArea   = hdisp;
    timings->HFrontPorch    = hsyncstart - hdisp;
    timings->HSyncPulse     = hsyncend - hsyncstart;
    timings->HBackPorch     = htotal - hsyncend;
    timings->VVisibleArea   = vdisp;
    timings->VFrontPorch    = vsyncstart - vdisp;
    timings->VSyncPulse     = vsyncend - vsyncstart;
    timings->VBackPorch     = vtotal - vsyncend;
    timings->HSyncLogic     = '-';
    timings->VSyncLogic     = '-';
    timings->scanCount      = 1;
    timings->multiScanBlack = 0;
    timings->HStartingBlock = VGAScanStart::VisibleArea;

    // actually this checks just the first character
    auto pc  = modeline + pos;
    auto end = pc + strlen(modeline);
    while (*pc && pc < end) {
      switch (*pc) {
        // parse [(+HSync | -HSync) (+VSync | -VSync)]
        case '+':
        case '-':
          if (!HSyncPol)
            timings->HSyncLogic = HSyncPol = *pc;
          else if (!VSyncPol)
            timings->VSyncLogic = VSyncPol = *pc;
          pc += 6;
          break;
        // parse [DoubleScan | QuadScan]
        // DoubleScan
        case 'D':
        case 'd':
          timings->scanCount = 2;
          pc += 10;
          break;
        // QuadScan
        case 'Q':
        case 'q':
          timings->scanCount = 4;
          pc += 8;
          break;
        // parse [FrontPorchBegins | SyncBegins | BackPorchBegins | VisibleBegins] [MultiScanBlank]
        // FrontPorchBegins
        case 'F':
        case 'f':
          timings->HStartingBlock = VGAScanStart::FrontPorch;
          pc += 16;
          break;
        // SyncBegins
        case 'S':
        case 's':
          timings->HStartingBlock = VGAScanStart::Sync;
          pc += 10;
          break;
        // BackPorchBegins
        case 'B':
        case 'b':
          timings->HStartingBlock = VGAScanStart::BackPorch;
          pc += 15;
          break;
        // VisibleBegins
        case 'V':
        case 'v':
          timings->HStartingBlock = VGAScanStart::VisibleArea;
          pc += 13;
          break;
        // MultiScanBlank
        case 'M':
        case 'm':
          timings->multiScanBlack = 1;
          pc += 14;
          break;
        case ' ':
          ++pc;
          break;
        default:
          return false;
      }
    }

    return true;

  }
  return false;
}


// Suspend vertical sync interrupt
// Warning: After call to suspendBackgroundPrimitiveExecution() adding primitives may cause a deadlock.
// To avoid this a call to "processPrimitives()" should be performed very often.
// Can be nested
void VideoController::suspendBackgroundPrimitiveExecution()
{
  ++m_primitiveProcessingSuspended;
  while (m_taskProcessingPrimitives)
    ;
}


// Resume vertical sync interrupt after suspendBackgroundPrimitiveExecution()
// Can be nested
void VideoController::resumeBackgroundPrimitiveExecution()
{
  m_primitiveProcessingSuspended = tmax(0, m_primitiveProcessingSuspended - 1);
}


void VideoController::startGPIOStream()
{
  m_GPIOStream.play(m_timings.frequency, m_DMABuffers);
}


void VideoController::setResolution(char const * modeline, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  VGATimings timings;
  if (convertModelineToTimings(modeline, &timings))
    setResolution(timings, viewPortWidth, viewPortHeight, doubleBuffered);
}


void VideoController::setResolution(VGATimings const& timings, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  // just in case setResolution() was called before
  end();

  m_timings = timings;

  // inform base class about screen size
  setScreenSize(m_timings.HVisibleArea, m_timings.VVisibleArea);

  setDoubleBuffered(doubleBuffered);

  m_HVSync = packHVSync(false, false);

  m_HLineSize = m_timings.HFrontPorch + m_timings.HSyncPulse + m_timings.HBackPorch + m_timings.HVisibleArea;

  m_HBlankLine_withVSync = (uint8_t*) heap_caps_malloc(m_HLineSize, MALLOC_CAP_DMA);
  m_HBlankLine           = (uint8_t*) heap_caps_malloc(m_HLineSize, MALLOC_CAP_DMA);

  m_viewPortWidth  = ~3 & (viewPortWidth <= 0 || viewPortWidth >= m_timings.HVisibleArea ? m_timings.HVisibleArea : viewPortWidth); // view port width must be 32 bit aligned
  m_viewPortHeight = viewPortHeight <= 0 || viewPortHeight >= m_timings.VVisibleArea ? m_timings.VVisibleArea : viewPortHeight;

  // adjust view port size if necessary
  checkViewPortSize();

  // need to center viewport?
  m_viewPortCol = (m_timings.HVisibleArea - m_viewPortWidth) / 2;
  m_viewPortRow = (m_timings.VVisibleArea - m_viewPortHeight) / 2;

  // view port col and row must be 32 bit aligned
  m_viewPortCol = m_viewPortCol & ~3;
  m_viewPortRow = m_viewPortRow & ~3;

  m_rawFrameHeight = m_timings.VVisibleArea + m_timings.VFrontPorch + m_timings.VSyncPulse + m_timings.VBackPorch;
  s_scanWidth = m_viewPortWidth;
  s_viewPortHeight = m_viewPortHeight;

  // allocate DMA descriptors
  setDMABuffersCount(calcRequiredDMABuffersCount(m_viewPortHeight));

  // allocate the viewport
  allocateViewPort();

  // adjust again view port size if necessary
  checkViewPortSize();

  // this may free space if m_viewPortHeight has been reduced
  setDMABuffersCount(calcRequiredDMABuffersCount(m_viewPortHeight));

  // fill buffers
  fillVertBuffers(0);
  fillHorizBuffers(0);

  resetPaintState();

  if (m_doubleBufferOverDMA)
    m_DMABuffersHead->qe.stqe_next = (lldesc_t*) &m_DMABuffersVisible[0];

  if (m_primitiveExecTask == nullptr) {
    xTaskCreatePinnedToCore(primitiveExecTask, "" , FABGLIB_VGAPALETTEDCONTROLLER_PRIMTASK_STACK_SIZE, this, FABGLIB_VGAPALETTEDCONTROLLER_PRIMTASK_PRIORITY, &m_primitiveExecTask, CoreUsage::quietCore());
  }
}


// this method may adjust m_viewPortHeight to the actual number of allocated rows.
// to reduce memory allocation overhead try to allocate the minimum number of blocks.
void VideoController::allocateViewPort(uint32_t allocCaps, int rowlen)
{
  int linesCount[FABGLIB_VIEWPORT_MEMORY_POOL_COUNT]; // where store number of lines for each pool
  int poolsCount = 0; // number of allocated pools
  int remainingLines = m_viewPortHeight;
  m_viewPortHeight = 0; // m_viewPortHeight needs to be recalculated

  if (isDoubleBuffered())
    remainingLines *= 2;

  // allocate pools
  m_viewPortMemoryPool = (uint8_t * *) heap_caps_malloc(sizeof(uint8_t*) * (FABGLIB_VIEWPORT_MEMORY_POOL_COUNT + 1), MALLOC_CAP_32BIT);
  while (remainingLines > 0 && poolsCount < FABGLIB_VIEWPORT_MEMORY_POOL_COUNT) {
    int largestBlock = heap_caps_get_largest_free_block(allocCaps);
    if (largestBlock < FABGLIB_MINFREELARGESTBLOCK)
      break;
    linesCount[poolsCount] = tmax(1, tmin(remainingLines, (largestBlock - FABGLIB_MINFREELARGESTBLOCK) / rowlen));
    m_viewPortMemoryPool[poolsCount] = (uint8_t*) heap_caps_malloc(linesCount[poolsCount] * rowlen, allocCaps);
    if (m_viewPortMemoryPool[poolsCount] == nullptr)
      break;
    remainingLines -= linesCount[poolsCount];
    m_viewPortHeight += linesCount[poolsCount];
    ++poolsCount;
  }
  m_viewPortMemoryPool[poolsCount] = nullptr;

  // fill m_viewPort[] with line pointers
  if (isDoubleBuffered()) {
    m_viewPortHeight /= 2;
    m_viewPortVisible = (volatile uint8_t * *) heap_caps_malloc(sizeof(uint8_t*) * m_viewPortHeight, MALLOC_CAP_32BIT | MALLOC_CAP_INTERNAL);
  }
  m_viewPort = (volatile uint8_t * *) heap_caps_malloc(sizeof(uint8_t*) * m_viewPortHeight, MALLOC_CAP_32BIT | MALLOC_CAP_INTERNAL);
  if (!isDoubleBuffered())
    m_viewPortVisible = m_viewPort;
  for (int p = 0, l = 0; p < poolsCount; ++p) {
    uint8_t * pool = m_viewPortMemoryPool[p];
    for (int i = 0; i < linesCount[p]; ++i) {
      if (l + i < m_viewPortHeight)
        m_viewPort[l + i] = pool;
      else
        m_viewPortVisible[l + i - m_viewPortHeight] = pool; // set only when double buffered is enabled
      pool += rowlen;
    }
    l += linesCount[p];
  }

  for (int i = 0; i < m_linesCount; ++i)
    m_lines[i] = (uint8_t*) heap_caps_malloc(m_viewPortWidth, MALLOC_CAP_DMA);
}


uint8_t IRAM_ATTR VideoController::packHVSync(bool HSync, bool VSync)
{
  uint8_t hsync_value = (m_timings.HSyncLogic == '+' ? (HSync ? 1 : 0) : (HSync ? 0 : 1));
  uint8_t vsync_value = (m_timings.VSyncLogic == '+' ? (VSync ? 1 : 0) : (VSync ? 0 : 1));
  return (vsync_value << VGA_VSYNC_BIT) | (hsync_value << VGA_HSYNC_BIT);
}


uint8_t IRAM_ATTR VideoController::preparePixelWithSync(RGB222 rgb, bool HSync, bool VSync)
{
  return packHVSync(HSync, VSync) | (rgb.B << VGA_BLUE_BIT) | (rgb.G << VGA_GREEN_BIT) | (rgb.R << VGA_RED_BIT);
}


int VideoController::calcRequiredDMABuffersCount(int viewPortHeight)
{
  int rightPadSize = m_timings.HVisibleArea - m_viewPortWidth - m_viewPortCol;
  int buffersCount = m_timings.scanCount * (m_rawFrameHeight + viewPortHeight);

  switch (m_timings.HStartingBlock) {
    case VGAScanStart::FrontPorch:
      // FRONTPORCH -> SYNC -> BACKPORCH -> VISIBLEAREA
      buffersCount += m_timings.scanCount * (rightPadSize > 0 ? viewPortHeight : 0);
      break;
    case VGAScanStart::Sync:
      // SYNC -> BACKPORCH -> VISIBLEAREA -> FRONTPORCH
      buffersCount += m_timings.scanCount * viewPortHeight;
      break;
    case VGAScanStart::BackPorch:
      // BACKPORCH -> VISIBLEAREA -> FRONTPORCH -> SYNC
      buffersCount += m_timings.scanCount * viewPortHeight;
      break;
    case VGAScanStart::VisibleArea:
      // VISIBLEAREA -> FRONTPORCH -> SYNC -> BACKPORCH
      buffersCount += m_timings.scanCount * (m_viewPortCol > 0 ? viewPortHeight : 0);
      break;
  }

  return buffersCount;
}


// refill buffers changing Front Porch and Back Porch
// offsetX : (< 0 : left  > 0 right)
void VideoController::fillHorizBuffers(int offsetX)
{
  // fill all with no hsync

  fill(m_HBlankLine,           0, m_HLineSize, 0, 0, 0, false, false);
  fill(m_HBlankLine_withVSync, 0, m_HLineSize, 0, 0, 0, false,  true);

  // calculate hsync pos and fill it

  int16_t porchSum = m_timings.HFrontPorch + m_timings.HBackPorch;
  m_timings.HFrontPorch = tmax(8, (int16_t)m_timings.HFrontPorch - offsetX);
  m_timings.HBackPorch  = tmax(8, porchSum - m_timings.HFrontPorch);
  m_timings.HFrontPorch = porchSum - m_timings.HBackPorch;

  int syncPos = 0;

  switch (m_timings.HStartingBlock) {
    case VGAScanStart::FrontPorch:
      // FRONTPORCH -> SYNC -> BACKPORCH -> VISIBLEAREA
      syncPos = m_timings.HFrontPorch;
      break;
    case VGAScanStart::Sync:
      // SYNC -> BACKPORCH -> VISIBLEAREA -> FRONTPORCH
      syncPos = 0;
      break;
    case VGAScanStart::BackPorch:
      // BACKPORCH -> VISIBLEAREA -> FRONTPORCH -> SYNC
      syncPos = m_timings.HBackPorch + m_timings.HVisibleArea + m_timings.HFrontPorch;
      break;
    case VGAScanStart::VisibleArea:
      // VISIBLEAREA -> FRONTPORCH -> SYNC -> BACKPORCH
      syncPos = m_timings.HVisibleArea + m_timings.HFrontPorch;
      break;
  }

  fill(m_HBlankLine,           syncPos, m_timings.HSyncPulse, 0, 0, 0, true,  false);
  fill(m_HBlankLine_withVSync, syncPos, m_timings.HSyncPulse, 0, 0, 0, true,   true);
}


void VideoController::fillVertBuffers(int offsetY)
{
  int16_t porchSum = m_timings.VFrontPorch + m_timings.VBackPorch;
  m_timings.VFrontPorch = tmax(1, (int16_t)m_timings.VFrontPorch - offsetY);
  m_timings.VBackPorch  = tmax(1, porchSum - m_timings.VFrontPorch);
  m_timings.VFrontPorch = porchSum - m_timings.VBackPorch;

  // associate buffers pointer to DMA info buffers
  //
  //  Vertical order:
  //    VisibleArea
  //    Front Porch
  //    Sync
  //    Back Porch

  int VVisibleArea_pos = 0;
  int VFrontPorch_pos  = VVisibleArea_pos + m_timings.VVisibleArea;
  int VSync_pos        = VFrontPorch_pos  + m_timings.VFrontPorch;
  int VBackPorch_pos   = VSync_pos        + m_timings.VSyncPulse;

  int rightPadSize = m_timings.HVisibleArea - m_viewPortWidth - m_viewPortCol;

  for (int line = 0, DMABufIdx = 0; line < m_rawFrameHeight; ++line) {

    bool isVVisibleArea = (line < VFrontPorch_pos);
    bool isVFrontPorch  = (line >= VFrontPorch_pos && line < VSync_pos);
    bool isVSync        = (line >= VSync_pos && line < VBackPorch_pos);
    bool isVBackPorch   = (line >= VBackPorch_pos);

    for (int scan = 0; scan < m_timings.scanCount; ++scan) {

      bool isStartOfVertFrontPorch = (line == VFrontPorch_pos && scan == 0);

      if (isVSync) {

        setDMABufferBlank(DMABufIdx++, m_HBlankLine_withVSync, m_HLineSize, scan, isStartOfVertFrontPorch);

      } else if (isVFrontPorch || isVBackPorch) {

        setDMABufferBlank(DMABufIdx++, m_HBlankLine, m_HLineSize, scan, isStartOfVertFrontPorch);

      } else if (isVVisibleArea) {

        int visibleAreaLine = line - VVisibleArea_pos;
        bool isViewport = visibleAreaLine >= m_viewPortRow && visibleAreaLine < m_viewPortRow + m_viewPortHeight;
        int HInvisibleAreaSize = m_HLineSize - m_timings.HVisibleArea;

        if (isViewport) {
          // visible, this is the viewport
          switch (m_timings.HStartingBlock) {
            case VGAScanStart::FrontPorch:
              // FRONTPORCH -> SYNC -> BACKPORCH -> VISIBLEAREA
              setDMABufferBlank(DMABufIdx++, m_HBlankLine, HInvisibleAreaSize + m_viewPortCol, scan, isStartOfVertFrontPorch);
              setDMABufferView(DMABufIdx++, visibleAreaLine - m_viewPortRow, scan, isStartOfVertFrontPorch);
              if (rightPadSize > 0)
                setDMABufferBlank(DMABufIdx++, m_HBlankLine + HInvisibleAreaSize, rightPadSize, scan, isStartOfVertFrontPorch);
              break;
            case VGAScanStart::Sync:
              // SYNC -> BACKPORCH -> VISIBLEAREA -> FRONTPORCH
              setDMABufferBlank(DMABufIdx++, m_HBlankLine, m_timings.HSyncPulse + m_timings.HBackPorch + m_viewPortCol, scan, isStartOfVertFrontPorch);
              setDMABufferView(DMABufIdx++, visibleAreaLine - m_viewPortRow, scan, isStartOfVertFrontPorch);
              setDMABufferBlank(DMABufIdx++, m_HBlankLine + m_HLineSize - m_timings.HFrontPorch - rightPadSize, m_timings.HFrontPorch + rightPadSize, scan, isStartOfVertFrontPorch);
              break;
            case VGAScanStart::BackPorch:
              // BACKPORCH -> VISIBLEAREA -> FRONTPORCH -> SYNC
              setDMABufferBlank(DMABufIdx++, m_HBlankLine, m_timings.HBackPorch + m_viewPortCol, scan, isStartOfVertFrontPorch);
              setDMABufferView(DMABufIdx++, visibleAreaLine - m_viewPortRow, scan, isStartOfVertFrontPorch);
              setDMABufferBlank(DMABufIdx++, m_HBlankLine + m_HLineSize - m_timings.HFrontPorch - m_timings.HSyncPulse - rightPadSize, m_timings.HFrontPorch + m_timings.HSyncPulse + rightPadSize, scan, isStartOfVertFrontPorch);
              break;
            case VGAScanStart::VisibleArea:
              // VISIBLEAREA -> FRONTPORCH -> SYNC -> BACKPORCH
              if (m_viewPortCol > 0)
                setDMABufferBlank(DMABufIdx++, m_HBlankLine, m_viewPortCol, scan, isStartOfVertFrontPorch);
              setDMABufferView(DMABufIdx++, visibleAreaLine - m_viewPortRow, scan, isStartOfVertFrontPorch);
              setDMABufferBlank(DMABufIdx++, m_HBlankLine + m_timings.HVisibleArea - rightPadSize, HInvisibleAreaSize + rightPadSize, scan, isStartOfVertFrontPorch);
              break;
          }

        } else {
          // not visible
          setDMABufferBlank(DMABufIdx++, m_HBlankLine, m_HLineSize, scan, isStartOfVertFrontPorch);
        }

      }

    }

  }
}


// address must be allocated with MALLOC_CAP_DMA or even an address of another already allocated buffer
// allocated buffer length (in bytes) must be 32 bit aligned
// Max length is 4092 bytes
void VideoController::setDMABufferBlank(int index, void volatile * address, int length, int scan, bool isStartOfVertFrontPorch)
{
  int size = (length + 3) & (~3);
  m_DMABuffers[index].eof    = 0;
  m_DMABuffers[index].size   = size;
  m_DMABuffers[index].length = length;
  m_DMABuffers[index].buf    = (uint8_t*) address;
  onSetupDMABuffer(&m_DMABuffers[index], isStartOfVertFrontPorch, scan, false, 0);
  if (m_doubleBufferOverDMA && isDoubleBuffered()) {
    m_DMABuffersVisible[index].eof    = 0;
    m_DMABuffersVisible[index].size   = size;
    m_DMABuffersVisible[index].length = length;
    m_DMABuffersVisible[index].buf    = (uint8_t*) address;
    onSetupDMABuffer(&m_DMABuffersVisible[index], isStartOfVertFrontPorch, scan, false, 0);
  }
}


bool VideoController::isMultiScanBlackLine(int scan)
{
  return (scan > 0 && m_timings.multiScanBlack == 1 && m_timings.HStartingBlock == FrontPorch);
}


// address must be allocated with MALLOC_CAP_DMA or even an address of another already allocated buffer
// allocated buffer length (in bytes) must be 32 bit aligned
// Max length is 4092 bytes
void VideoController::setDMABufferView(int index, int row, int scan, volatile uint8_t * * viewPort, bool onVisibleDMA)
{
  uint8_t * bufferPtr = nullptr;
  if (isMultiScanBlackLine(scan))
    bufferPtr = (uint8_t *) (m_HBlankLine + m_HLineSize - m_timings.HVisibleArea);  // this works only when HSYNC, FrontPorch and BackPorch are at the beginning of m_HBlankLine
  else if (viewPort)
    bufferPtr = (uint8_t *) viewPort[row];
  lldesc_t volatile * DMABuffers = onVisibleDMA ? m_DMABuffersVisible : m_DMABuffers;
  DMABuffers[index].size   = (m_viewPortWidth + 3) & (~3);
  DMABuffers[index].length = m_viewPortWidth;
  DMABuffers[index].buf    = bufferPtr;
}


// address must be allocated with MALLOC_CAP_DMA or even an address of another already allocated buffer
// allocated buffer length (in bytes) must be 32 bit aligned
// Max length is 4092 bytes
void VideoController::setDMABufferView(int index, int row, int scan, bool isStartOfVertFrontPorch)
{
  setDMABufferView(index, row, scan, m_viewPort, false);
  if (!isMultiScanBlackLine(scan))
    onSetupDMABuffer(&m_DMABuffers[index], isStartOfVertFrontPorch, scan, true, row);
  if (isDoubleBuffered()) {
    setDMABufferView(index, row, scan, m_viewPortVisible, true);
    if (!isMultiScanBlackLine(scan))
      onSetupDMABuffer(&m_DMABuffersVisible[index], isStartOfVertFrontPorch, scan, true, row);
  }
}


void volatile * VideoController::getDMABuffer(int index, int * length)
{
  *length = m_DMABuffers[index].length;
  return m_DMABuffers[index].buf;
}


// buffer: buffer to fill (buffer size must be 32 bit aligned)
// startPos: initial position (in pixels)
// length: number of pixels to fill
//
// Returns next pos to fill (startPos + length)
int VideoController::fill(uint8_t volatile * buffer, int startPos, int length, uint8_t red, uint8_t green, uint8_t blue, bool HSync, bool VSync)
{
  uint8_t pattern = preparePixelWithSync((RGB222){red, green, blue}, HSync, VSync);
  for (int i = 0; i < length; ++i, ++startPos)
    VGA_PIXELINROW(buffer, startPos) = pattern;
  return startPos;
}


void VideoController::moveScreen(int offsetX, int offsetY)
{
  suspendBackgroundPrimitiveExecution();
  fillVertBuffers(offsetY);
  fillHorizBuffers(offsetX);
  resumeBackgroundPrimitiveExecution();
}


void VideoController::shrinkScreen(int shrinkX, int shrinkY)
{
  VGATimings * currTimings = getResolutionTimings();

  currTimings->HBackPorch  = tmax(currTimings->HBackPorch + 4 * shrinkX, 4);
  currTimings->HFrontPorch = tmax(currTimings->HFrontPorch + 4 * shrinkX, 4);

  currTimings->VBackPorch  = tmax(currTimings->VBackPorch + shrinkY, 1);
  currTimings->VFrontPorch = tmax(currTimings->VFrontPorch + shrinkY, 1);

  setResolution(*currTimings, m_viewPortWidth, m_viewPortHeight, isDoubleBuffered());
}


void IRAM_ATTR VideoController::swapBuffers()
{
  tswap(m_viewPort, m_viewPortVisible);
  if (m_doubleBufferOverDMA) {
    tswap(m_DMABuffers, m_DMABuffersVisible);
    m_DMABuffersHead->qe.stqe_next = (lldesc_t*) &m_DMABuffersVisible[0];
  }
}


// Chance to overwrite a scan line in the output DMA buffer.
void IRAM_ATTR VideoController::decorateScanLinePixels(uint8_t * pixels, uint16_t scanRow) {
  drawSpriteScanLine(pixels, scanRow, s_scanWidth, s_viewPortHeight);
}

// we can use getCycleCount here because primitiveExecTask is pinned to a specific core (so cycle counter is the same)
// getCycleCount() requires 0.07us, while esp_timer_get_time() requires 0.78us
void VideoController::primitiveExecTask(void * arg)
{
  auto ctrl = (VideoController *) arg;

  while (true) {
    if (!ctrl->m_primitiveProcessingSuspended) {
      auto startCycle = ctrl->backgroundPrimitiveTimeoutEnabled() ? getCycleCount() : 0;
      Rect updateRect = Rect(SHRT_MAX, SHRT_MAX, SHRT_MIN, SHRT_MIN);
      ctrl->m_taskProcessingPrimitives = true;
      do {
        Primitive prim;
        if (ctrl->getPrimitive(&prim, 0) == false)
          break;
        ctrl->execPrimitive(prim, updateRect, false);
        if (ctrl->m_primitiveProcessingSuspended)
          break;
      } while (!ctrl->backgroundPrimitiveTimeoutEnabled() || (startCycle + ctrl->m_primitiveExecTimeoutCycles > getCycleCount()));
      ctrl->showSprites(updateRect);
      ctrl->m_taskProcessingPrimitives = false;
    }

    // wait for vertical sync
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }

}


// calculates number of CPU cycles usable to draw primitives
void VideoController::calculateAvailableCyclesForDrawings()
{
  int availtime_us;

  if (m_processPrimitivesOnBlank) {
    // allowed time to process primitives is limited to the vertical blank. Slow, but avoid flickering
    availtime_us = ceil(1000000.0 / m_timings.frequency * m_timings.scanCount * m_HLineSize * (m_linesCount / 2 + m_timings.VFrontPorch + m_timings.VSyncPulse + m_timings.VBackPorch + m_viewPortRow));
  } else {
    // allowed time is the half of an entire frame. Fast, but may flick
    availtime_us = ceil(1000000.0 / m_timings.frequency * m_timings.scanCount * m_HLineSize * (m_timings.VVisibleArea + m_timings.VFrontPorch + m_timings.VSyncPulse + m_timings.VBackPorch));
    availtime_us /= 2;
  }

  m_primitiveExecTimeoutCycles = getCPUFrequencyMHz() * availtime_us;  // at 240Mhz, there are 240 cycles every microsecond
}

void VideoController::init()
{
  m_doubleBufferOverDMA      = false;
}


void VideoController::end()
{
}


void VideoController::suspendBackgroundPrimitiveExecution()
{
}

// make sure view port height is divisible by m_linesCount, view port width is divisible by m_columnsQuantum
void VideoController::checkViewPortSize()
{
  m_viewPortHeight &= ~(m_linesCount - 1);
  m_viewPortWidth  &= ~(m_columnsQuantum - 1);
}


void VideoController::setResolution(VGATimings const& timings, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  VGABaseController::setResolution(timings, viewPortWidth, viewPortHeight, doubleBuffered);

  s_viewPort        = m_viewPort;
  s_viewPortVisible = m_viewPortVisible;

  // fill view port
  for (int i = 0; i < m_viewPortHeight; ++i)
    memset((void*)(m_viewPort[i]), nativePixelFormat() == NativePixelFormat::SBGR2222 ? m_HVSync : 0, m_viewPortWidth / m_viewPortRatioDiv * m_viewPortRatioMul);

  deletePalette(65535);
  setupDefaultPalette();
  updateRGB2PaletteLUT();

  uint16_t signalList[2] = { 0, 0 };
  updateSignalList(signalList, 1);

  calculateAvailableCyclesForDrawings();

  // must be started before interrupt alloc
  startGPIOStream();

  // ESP_INTR_FLAG_LEVEL1: should be less than PS2Controller interrupt level, necessary when running on the same core
  if (m_isr_handle == nullptr) {
    CoreUsage::setBusiestCore(FABGLIB_VIDEO_CPUINTENSIVE_TASKS_CORE);
    esp_intr_alloc_pinnedToCore(ETS_I2S1_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM, m_isrHandler, this, &m_isr_handle, FABGLIB_VIDEO_CPUINTENSIVE_TASKS_CORE);
    I2S1.int_clr.val     = 0xFFFFFFFF;
    I2S1.int_ena.out_eof = 1;
  }

  resumeBackgroundPrimitiveExecution();
}


void VideoController::onSetupDMABuffer(lldesc_t volatile * buffer, bool isStartOfVertFrontPorch, int scan, bool isVisible, int visibleRow)
{
  if (isVisible) {
    buffer->buf = (uint8_t *) m_lines[visibleRow % m_linesCount];

    // generate interrupt every half m_linesCount
    if ((scan == 0 && (visibleRow % (m_linesCount / 2)) == 0)) {
      if (visibleRow == 0)
        s_frameResetDesc = buffer;
      buffer->eof = 1;
    }
  }
}


void VideoController::swapBuffers()
{
  VGABaseController::swapBuffers();
  s_viewPort        = m_viewPort;
  s_viewPortVisible = m_viewPortVisible;
}

} // end of namespace
