// vgaotfcontroller.cpp - controls OTF display mode for VGA
//
// by Curtis Whitley
//

#include "vgaotfcontroller.h"

namespace fabgl {

VGAOtfController::VGAOtfController() {
    m_lines = (volatile uint8_t**) heap_caps_malloc(sizeof(uint8_t*) * OTF_NUM_PHYSICAL_SCAN_LINES, MALLOC_CAP_32BIT | MALLOC_CAP_INTERNAL);
    m_taskProcessingPrimitives = false;
    m_processPrimitivesOnBlank = false;
    m_primitiveExecTask = nullptr;
}

VGAOtfController::~VGAOtfController() {
    heap_caps_free(m_lines);
}

void VGAOtfController::end()
{
  if (m_primitiveExecTask) {
    vTaskDelete(m_primitiveExecTask);
    m_primitiveExecTask = nullptr;
    m_taskProcessingPrimitives = false;
  }
  VGABaseController::end();
}

void VGAOtfController::setNumScanLines() {
    m_physicalScanLines = OTF_NUM_PHYSICAL_SCAN_LINES;
}

void VGAOtfController::onSetupDMABuffer(lldesc_t volatile * buffer, bool isStartOfVertFrontPorch, int scan, bool isVisible, int visibleRow) {
  if (isVisible) {
    buffer->buf = (uint8_t *) m_lines[visibleRow % OTF_NUM_PHYSICAL_SCAN_LINES];

    // generate interrupt every couple of lines
    if ((scan == 0 && (visibleRow % OTF_LINES_COUNT) == 0)) {
      if (visibleRow == 0)
        s_frameResetDesc = buffer;
      buffer->eof = 1;
    }
  }
}

void VGAOtfController::allocateViewPort() {
    VGABaseController::allocateViewPort(MALLOC_CAP_DMA, m_viewPortWidth);

    for (int i = 0; i < OTF_LINES_COUNT; ++i)
      m_lines[i] = (uint8_t*) heap_caps_malloc(m_viewPortWidth, MALLOC_CAP_DMA);
}

void VGAOtfController::freeViewPort()
{
  VGABaseController::freeViewPort();

  for (int i = 0; i < m_linesCount; ++i) {
    heap_caps_free((void*)m_lines[i]);
    m_lines[i] = nullptr;
  }
}

void VGAOtfController::setResolution(VGATimings const& timings, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  VGABaseController::setResolution(timings, viewPortWidth, viewPortHeight, doubleBuffered);

  s_viewPort = m_viewPort;
  s_viewPortVisible = m_viewPortVisible;

  // fill view port
  //for (int i = 0; i < m_viewPortHeight; ++i)
  //  memset((void*)(m_viewPort[i]), 0, m_viewPortWidth / m_viewPortRatioDiv * m_viewPortRatioMul);

  calculateAvailableCyclesForDrawings();

  // must be started before interrupt alloc
  startGPIOStream();

  // ESP_INTR_FLAG_LEVEL1: should be less than PS2Controller interrupt level, necessary when running on the same core
  if (m_isr_handle == nullptr) {
    CoreUsage::setBusiestCore(FABGLIB_VIDEO_CPUINTENSIVE_TASKS_CORE);
    esp_intr_alloc_pinnedToCore(ETS_I2S1_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM, ISRHandler, this, &m_isr_handle, FABGLIB_VIDEO_CPUINTENSIVE_TASKS_CORE);
    I2S1.int_clr.val = 0xFFFFFFFF;
    I2S1.int_ena.out_eof = 1;
  }

  if (m_primitiveExecTask == nullptr) {
    xTaskCreatePinnedToCore(primitiveExecTask, "" , FABGLIB_VGAPALETTEDCONTROLLER_PRIMTASK_STACK_SIZE, this, FABGLIB_VGAPALETTEDCONTROLLER_PRIMTASK_PRIORITY, &m_primitiveExecTask, CoreUsage::quietCore());
  }

  resumeBackgroundPrimitiveExecution();
}

NativePixelFormat VGAOtfController::nativePixelFormat() {
    return NativePixelFormat::SBGR2222;
}

void VGAOtfController::readScreen(Rect const & rect, RGB888 * destBuf) {
}

void VGAOtfController::setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect) {
}

void VGAOtfController::absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color) {
}

void VGAOtfController::fillRow(int y, int x1, int x2, RGB888 color) {
}

void VGAOtfController::drawEllipse(Size const & size, Rect & updateRect) {
}

void VGAOtfController::drawArc(Rect const & rect, Rect & updateRect) {
}

void VGAOtfController::fillSegment(Rect const & rect, Rect & updateRect) {
}

void VGAOtfController::fillSector(Rect const & rect, Rect & updateRect) {
}

void VGAOtfController::clear(Rect & updateRect) {
}

void VGAOtfController::VScroll(int scroll, Rect & updateRect) {
}

void VGAOtfController::HScroll(int scroll, Rect & updateRect) {
}

void VGAOtfController::drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect) {
}

void VGAOtfController::invertRect(Rect const & rect, Rect & updateRect) {
}

void VGAOtfController::swapFGBG(Rect const & rect, Rect & updateRect) {
}

void VGAOtfController::copyRect(Rect const & source, Rect & updateRect) {
}

int VGAOtfController::getBitmapSavePixelSize() {
    return 0;
}

void VGAOtfController::rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount) {
}

void VGAOtfController::rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount) {
}

void VGAOtfController::rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount) {
}

void VGAOtfController::rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount) {
}

void VGAOtfController::rawCopyToBitmap(int srcX, int srcY, int width, void * saveBuffer, int X1, int Y1, int XCount, int YCount) {
}

void VGAOtfController::rawDrawBitmapWithMatrix_Mask(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix) {
}

void VGAOtfController::rawDrawBitmapWithMatrix_RGBA2222(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix) {
}

void VGAOtfController::rawDrawBitmapWithMatrix_RGBA8888(int destX, int destY, Rect & drawingRect, Bitmap const * bitmap, const float * invMatrix) {
}

void IRAM_ATTR VGAOtfController::ISRHandler(void * arg)
{
  auto ctrl = (VGAOtfController *) arg;

  if (I2S1.int_st.out_eof) {

    auto const desc = (lldesc_t*) I2S1.out_eof_des_addr;

    if (desc == s_frameResetDesc)
      s_scanLine = 0;

    auto const width  = ctrl->m_viewPortWidth;
    auto const height = ctrl->m_viewPortHeight;
    auto const lines  = ctrl->m_lines;

    int scanLine = (s_scanLine + OTF_LINES_COUNT) % height;

    auto lineIndex = scanLine & (OTF_LINES_COUNT - 1);

    for (int i = 0; i < OTF_LINES_COUNT; ++i) {
      auto src  = (uint8_t const *) s_viewPortVisible[scanLine];
      auto dest = (uint16_t*) lines[lineIndex];
    }

    s_scanLine += OTF_LINES_COUNT;

    if (scanLine >= height && !ctrl->m_primitiveProcessingSuspended && spi_flash_cache_enabled() && ctrl->m_primitiveExecTask) {
      // vertical sync, unlock primitive execution task
      // warn: don't use vTaskSuspendAll() in primitive drawing, otherwise vTaskNotifyGiveFromISR may be blocked and screen will flick!
      vTaskNotifyGiveFromISR(ctrl->m_primitiveExecTask, NULL);
    }

  }

  #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  s_vgapalctrlcycles += getCycleCount() - s1;
  #endif

  I2S1.int_clr.val = I2S1.int_st.val;
}

} // namespace fabgl
