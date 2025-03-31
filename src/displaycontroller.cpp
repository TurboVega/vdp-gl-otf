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


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// BitmappedDisplayController implementation


int BitmappedDisplayController::queueSize = FABGLIB_DEFAULT_DISPLAYCONTROLLER_QUEUE_SIZE;


BitmappedDisplayController::BitmappedDisplayController()
  : m_primDynMemPool(FABGLIB_PRIMITIVES_DYNBUFFERS_SIZE)
{
  m_execQueue                           = nullptr;
  m_backgroundPrimitiveExecutionEnabled = true;
  m_sprites                             = nullptr;
  m_spritesCount                        = 0;
  m_doubleBuffered                      = false;
  m_mouseCursor.visible                 = false;
  m_backgroundPrimitiveTimeoutEnabled   = true;
  m_spritesHidden                       = true;
}


BitmappedDisplayController::~BitmappedDisplayController()
{
  vQueueDelete(m_execQueue);
  delete m_painter;
}


void BitmappedDisplayController::setDoubleBuffered(bool value)
{
  m_doubleBuffered = value;
  if (m_execQueue)
    vQueueDelete(m_execQueue);
  // on double buffering a queue of single element is enough and necessary (see addPrimitive() for details)
  m_execQueue = xQueueCreate(value ? 1 : BitmappedDisplayController::queueSize, sizeof(Primitive));
}


void BitmappedDisplayController::addPrimitive(Primitive & primitive)
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
void BitmappedDisplayController::primitiveReplaceDynamicBuffers(Primitive & primitive)
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
bool IRAM_ATTR BitmappedDisplayController::getPrimitiveISR(Primitive * primitive)
{
  return xQueueReceiveFromISR(m_execQueue, primitive, nullptr);
}


bool BitmappedDisplayController::getPrimitive(Primitive * primitive, int timeOutMS)
{
  return xQueueReceive(m_execQueue, primitive, msToTicks(timeOutMS));
}


// cannot be called inside an ISR
void BitmappedDisplayController::waitForPrimitives()
{
  Primitive p;
  xQueuePeek(m_execQueue, &p, portMAX_DELAY);
}


void BitmappedDisplayController::primitivesExecutionWait()
{
  if (m_backgroundPrimitiveExecutionEnabled) {
    while (uxQueueMessagesWaiting(m_execQueue) > 0)
      ;
  }
}


// When false primitives are executed immediately, otherwise they are added to the primitive queue
// When set to false the queue is emptied executing all pending primitives
// Cannot be nested
void BitmappedDisplayController::enableBackgroundPrimitiveExecution(bool value)
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
void IRAM_ATTR BitmappedDisplayController::processPrimitives()
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


void BitmappedDisplayController::setSprites(Sprite * sprites, int count, int spriteSize)
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


Sprite * IRAM_ATTR BitmappedDisplayController::getSprite(int index)
{
  return (Sprite*) ((uint8_t*)m_sprites + index * m_spriteSize);
}


void BitmappedDisplayController::refreshSprites()
{
  Primitive p(PrimitiveCmd::RefreshSprites);
  addPrimitive(p);
}


void IRAM_ATTR BitmappedDisplayController::hideSprites(Rect & updateRect)
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


void IRAM_ATTR BitmappedDisplayController::showSprites(Rect & updateRect)
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
void BitmappedDisplayController::setMouseCursor(Cursor * cursor)
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


void BitmappedDisplayController::setMouseCursor(CursorName cursorName)
{
  setMouseCursor(&CURSORS[(int)cursorName]);
}


void BitmappedDisplayController::setMouseCursorPos(int X, int Y)
{
  m_mouseCursor.moveTo(X - m_mouseHotspotX, Y - m_mouseHotspotY);
  refreshSprites();
}


void BitmappedDisplayController::drawSpriteScanLine(uint8_t * pixelData, int scanRow, int scanWidth, int viewportHeight) {
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


void IRAM_ATTR BitmappedDisplayController::execPrimitive(Primitive const & prim, Rect & updateRect, bool insideISR)
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

} // end of namespace
