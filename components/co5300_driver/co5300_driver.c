#include "co5300_driver.h"

// —— 头部：保留/简化上下文 —— 
typedef volatile uint32_t* PORTreg_t;

typedef union {
    uint8_t  _buffer[SPI_MAX_PIXELS_AT_ONCE * 2];   // 8-bit 视图
    uint16_t _buffer16[SPI_MAX_PIXELS_AT_ONCE];     // 16-bit 像素视图（RGB565）
    uint32_t _buffer32[SPI_MAX_PIXELS_AT_ONCE / 2]; // 2 像素拼成 32bit（便于 DMA）
} PixelTransferBuffer;

typedef struct {
    // CS 端口寄存器与掩码
    PORTreg_t cs_port_set;
    PORTreg_t cs_port_clr;
    uint32_t  cs_pin_mask;

    // SPI
    spi_device_handle_t   spi;
    spi_transaction_ext_t tran_ext;   // 复用一个扩展事务
    spi_transaction_t*    tran_base;  // &tran_ext.base

    // 几何/窗口状态
    uint8_t  rotation;
    uint8_t  x_start, y_start;
    int16_t  cur_x, cur_y;
    uint16_t cur_w, cur_h;
    int16_t  max_x, max_y;
    int16_t  width, height;

    // 传输缓冲
    PixelTransferBuffer txbuf;
} co5300_t;

static co5300_t g; // 单实例（后续可做多实例）

//g.max_x = LCD_WIDTH - 1, g.max_y = LCD_HEIGHT - 1;

uint8_t COL_OFFSET1 = 16, ROW_OFFSET1 =0;
uint8_t COL_OFFSET2 = 1, ROW_OFFSET2 = 1;


static inline void cs_low(co5300_t* ctx){ *ctx->cs_port_clr = ctx->cs_pin_mask; }
static inline void cs_high(co5300_t* ctx){ *ctx->cs_port_set = ctx->cs_pin_mask; }


esp_err_t  lcd_gpio_init(void)
{

    const uint64_t pin_mask = (1ULL << LCD_CS) | (1ULL << LCD_RST);
    // 配置 CS 和 RESET 引脚为输出模式
    gpio_config_t io_conf = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = pin_mask,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        return err;
    }    

    gpio_set_level(LCD_CS, 1);     // 默认不选中
    gpio_set_level(LCD_RST, 1);    // 默认不复位

    return ESP_OK;
}


bool begin()
{
    lcd_gpio_init();
    gpio_set_level(LCD_CS, 1);    // 默认不复位

#if (LCD_CS < 32)
    g.cs_pin_mask = (1u << LCD_CS); // GPIO0-31 use OUT registers
    g.cs_port_set = (PORTreg_t)&GPIO.out_w1ts;
    g.cs_port_clr = (PORTreg_t)&GPIO.out_w1tc;
#else
    g.cs_pin_mask = (1u << (LCD_CS - 32)); // GPIO32+ use OUT1 registers
    g.cs_port_set = (PORTreg_t)&GPIO.out1_w1ts.val;
    g.cs_port_clr = (PORTreg_t)&GPIO.out1_w1tc.val;
#endif

  spi_bus_config_t buscfg = {
      .mosi_io_num = LCD_SDIO0,
      .miso_io_num = LCD_SDIO1,
      .sclk_io_num = LCD_SCLK,
      .quadwp_io_num = LCD_SDIO2,
      .quadhd_io_num = LCD_SDIO3,
      .max_transfer_sz = (SPI_MAX_PIXELS_AT_ONCE * 16) + 8,
      .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS,
  };
  esp_err_t ret = spi_bus_initialize(QSPI_SPI_HOST, &buscfg, QSPI_DMA_CHANNEL);
  if (ret != ESP_OK)
  {
    ESP_ERROR_CHECK(ret);
    return false;
  }

  spi_device_interface_config_t devcfg = {
      .command_bits = 8,
      .address_bits = 24,
      .mode = 0,
      .clock_speed_hz = QSPI_FREQUENCY,
      .spics_io_num = -1, // avoid use system CS control
      .flags = SPI_DEVICE_HALFDUPLEX,
      .queue_size = 1,
  };
  ret = spi_bus_add_device(QSPI_SPI_HOST, &devcfg, &g.spi);
  if (ret != ESP_OK)
  {
    ESP_ERROR_CHECK(ret);
    return false;
  }

  spi_device_acquire_bus(g.spi, portMAX_DELAY);
  memset(&g.tran_ext, 0, sizeof(g.tran_ext));
  g.tran_base = (spi_transaction_t *)&g.tran_ext;

  return true;
}


// 写 MADCTL
void setMADCTL(uint8_t rotation) {
    uint8_t madctl = 0;

    switch(g.rotation) {
        case 0: madctl = 0x00; break; // 竖屏
        case 1: madctl = 0x60; break; // 横屏
        case 2: madctl = 0xC0; break; // 180°竖屏
        case 3: madctl = 0xA0; break; // 横屏180°翻转
    }
    writeC8D8(CO5300_W_MADCTL,madctl);
}

void setRotation(uint8_t r)
{
    g.rotation = (r & 7);


      switch (g.rotation)
    {
    case 7:
    case 5:
    case 3:
    case 1:
        g.width = LCD_HEIGHT;
        g.height = LCD_WIDTH;
        g.max_x = g.width - 1;  ///< x zero base bound
        g.max_y = g.height - 1; ///< y zero base bound
        break;
    case 6:
    case 4:
    case 2:
    default: // case 0:
        g.width = LCD_WIDTH;
        g.height = LCD_HEIGHT;
        g.max_x = g.width - 1;  ///< x zero base bound
        g.max_y = g.height - 1; ///< y zero base bound
        break;
    }

    switch (g.rotation)
    {
    case 5:
    case 3:
        g.x_start = ROW_OFFSET2;
        g.y_start = COL_OFFSET1;
        break;
    case 6:
    case 2:
        g.x_start = COL_OFFSET2;
        g.y_start = ROW_OFFSET2;
        break;
    case 7:
    case 1:
        g.x_start = ROW_OFFSET1;
        g.y_start = COL_OFFSET2;
        break;
    case 4:
    default: // case 0:
        g.x_start = COL_OFFSET1;
        g.y_start = ROW_OFFSET1;
        break;
    }
    g.cur_x = 0xFFFF;
    g.cur_y = 0xFFFF;
    g.cur_w = 0xFFFF;
    g.cur_h = 0xFFFF;
    setMADCTL(g.rotation);
}

bool TFT_begin()
{
    if (!begin()) {
        return false;
    }
    
    // Reset display
    gpio_set_level(LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(CO5300_RST_DELAY));
    gpio_set_level(LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(CO5300_RST_DELAY));
    
    // Initialize display with command sequence
    batchOperation(co5300_init_operations, sizeof(co5300_init_operations));

    setRotation(0);
    // Set initial window
    writeAddrWindow(0, 0, LCD_WIDTH, LCD_HEIGHT);
    
    return true;
}

void beginWrite()
{
    cs_low(&g);
}

void endWrite()
{
    cs_high(&g);
}

 void POLL_START()
{
  //esp_err_t ret = spi_device_polling_start(g.spi, g.tran_base, portMAX_DELAY);
  esp_err_t ret = spi_device_polling_transmit(g.spi, g.tran_base);
  if (ret != ESP_OK)
  {
    printf("spi_device_polling_start error: %d\n", ret);
    // Consider adding error handling here
  }
}

 void POLL_END()
{
  //esp_err_t ret = spi_device_polling_end(g.spi, portMAX_DELAY);
  // if (ret != ESP_OK)
  // {
  //   printf("spi_device_polling_end error: %d\n", ret);
  //   // Consider adding error handling here
  // }

}

void writeCommand16(uint16_t c)
{
  cs_low(&g);
  g.tran_ext.base.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
  g.tran_ext.base.cmd = SPI_CMD_WRITE;
  g.tran_ext.base.addr = c;
  g.tran_ext.base.tx_buffer = NULL;
  g.tran_ext.base.length = 0;
  POLL_START();
  POLL_END();
  cs_high(&g);
}



void write16(uint16_t d)
{
  cs_low(&g);
  g.tran_ext.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_MODE_QIO;
  g.tran_ext.base.cmd = SPI_CMD_READ;
  g.tran_ext.base.addr = 0x003C00;
  g.tran_ext.base.tx_data[0] = d >> 8;
  g.tran_ext.base.tx_data[1] = d;
  g.tran_ext.base.length = 16;
  POLL_START();
  POLL_END();
  cs_high(&g);
}

void my_write(uint8_t d)
{
  cs_low(&g);
  g.tran_ext.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_MODE_QIO;
  g.tran_ext.base.cmd = SPI_CMD_READ;
  g.tran_ext.base.addr = 0x003C00;
  g.tran_ext.base.tx_data[0] = d;
  g.tran_ext.base.length = 8;
  POLL_START();
  POLL_END();
  cs_high(&g);
}

void writeCommand(uint8_t c)
{
  cs_low(&g);
  g.tran_ext.base.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
  g.tran_ext.base.cmd = SPI_CMD_WRITE;
  g.tran_ext.base.addr = ((uint32_t)c) << 8;
  g.tran_ext.base.tx_buffer = NULL;
  g.tran_ext.base.length = 0;
  POLL_START();
  POLL_END();
  cs_high(&g);
}

void writeC8D8(uint8_t c, uint8_t d)
{
  cs_low(&g);
  g.tran_ext.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
  g.tran_ext.base.cmd = SPI_CMD_WRITE;
  g.tran_ext.base.addr = ((uint32_t)c) << 8;
  g.tran_ext.base.tx_data[0] = d;
  g.tran_ext.base.length = 8;
  POLL_START();
  POLL_END();
  cs_high(&g);
}

void writeC8D16(uint8_t c, uint16_t d)
{
  cs_low(&g);
  g.tran_ext.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
  g.tran_ext.base.cmd = SPI_CMD_WRITE;
  g.tran_ext.base.addr = ((uint32_t)c) << 8;
  g.tran_ext.base.tx_data[0] = d >> 8;
  g.tran_ext.base.tx_data[1] = d;
  g.tran_ext.base.length = 16;
  POLL_START();
  POLL_END();
  cs_high(&g);
}

void writeC8D16D16(uint8_t c, uint16_t d1, uint16_t d2)
{
  cs_low(&g);
  g.tran_ext.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
  g.tran_ext.base.cmd = SPI_CMD_WRITE;
  g.tran_ext.base.addr = ((uint32_t)c) << 8;
  g.tran_ext.base.tx_data[0] = d1 >> 8;
  g.tran_ext.base.tx_data[1] = d1;
  g.tran_ext.base.tx_data[2] = d2 >> 8;
  g.tran_ext.base.tx_data[3] = d2;
  g.tran_ext.base.length = 32;
  POLL_START();
  POLL_END();
  cs_high(&g);
}



/* 初始批量操作 */
void batchOperation(const uint8_t *operations, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        uint8_t l = 0;
        switch (operations[i])
        {
        case BEGIN_WRITE:
            beginWrite();
            break;
        case WRITE_C8_D16:
            break;
        case WRITE_C8_D8:
            writeC8D8(operations[i+1], operations[i+2]);
            i += 2;
            break;
        case WRITE_COMMAND_8:
            writeCommand(operations[++i]);
            break;
        case WRITE_C16_D16:

            break;
        case WRITE_COMMAND_16:

            break;
        case WRITE_DATA_8:
            l = 1;
            break;
        case WRITE_DATA_16:
            l = 2;
            break;
        case WRITE_BYTES:
            l = operations[++i];
            break;
        case END_WRITE:
            endWrite();
            break;
        case DELAY:
            vTaskDelay(operations[++i]);
            break;
        default:
            printf("Unknown operation id at %d: %d", i, operations[i]);
            break;
        }
        while (l--)
        {
            my_write(operations[++i]);
        }
    }
}

/* 设置显示区域 */
void writeAddrWindow(int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    if ((x != g.cur_x) || (w != g.cur_w) || (y != g.cur_y) || (h != g.cur_h))
    {
        writeC8D16D16(CO5300_W_CASET, x + g.x_start, x + w - 1 + g.x_start);
        writeC8D16D16(CO5300_W_PASET, y + g.y_start, y + h - 1 + g.y_start);

        g.cur_x = x;
        g.cur_y = y;
        g.cur_w = w;
        g.cur_h = h;
    }

    writeCommand(CO5300_W_RAMWR); // write to RAM
}


/* QSPI底层接口 */
void writeRepeat(uint16_t p, uint32_t len)
{
    bool first_send = true;

  uint16_t bufLen = (len >= SPI_MAX_PIXELS_AT_ONCE) ? SPI_MAX_PIXELS_AT_ONCE : len;
  int16_t xferLen, l;
  uint32_t c32;
  MSB_32_16_16_SET(c32, p, p);

  l = (bufLen + 1) / 2;
  for (uint32_t i = 0; i < l; i++)
  {
    g.txbuf._buffer32[i] = c32;
  }

  cs_low(&g);
  // Issue pixels in blocks from temp buffer  
  while (len) // While pixels remains
  {
    xferLen = (bufLen <= len) ? bufLen : len; // How many this pass?

    if (first_send)
    {
      g.tran_ext.base.flags = SPI_TRANS_MODE_QIO;
      g.tran_ext.base.cmd = 0x32;
      g.tran_ext.base.addr = 0x003C00;
      first_send = false;
    }
    else
    {
      g.tran_ext.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                                 SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
    }
    g.tran_ext.base.tx_buffer = g.txbuf._buffer16;
    g.tran_ext.base.length = xferLen << 4;

    POLL_START();
    POLL_END();

    len -= xferLen;
  }
  cs_high(&g);
}

void writePixels(uint16_t *data, uint32_t len)
{

  cs_low(&g);
  uint32_t l, l2;
  uint16_t p1, p2;
  bool first_send = true;
  while (len)
  {
    l = (len > SPI_MAX_PIXELS_AT_ONCE) ? SPI_MAX_PIXELS_AT_ONCE : len;

    if (first_send)
    {
      g.tran_ext.base.flags = SPI_TRANS_MODE_QIO;
      g.tran_ext.base.cmd = 0x32;
      g.tran_ext.base.addr = 0x003C00;
      first_send = false;
    }
    else
    {
      g.tran_ext.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                                 SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
    }
    l2 = l >> 1;
    for (uint32_t i = 0; i < l2; ++i)
    {
      p1 = *data++;
      p2 = *data++;
      MSB_32_16_16_SET(g.txbuf._buffer32[i], p1, p2);
    }
    if (l & 1)
    {
      p1 = *data++;
      MSB_16_SET(g.txbuf._buffer16[l - 1], p1);
    }

    g.tran_ext.base.tx_buffer = g.txbuf._buffer32;
    g.tran_ext.base.length = l << 4;

    POLL_START();
    POLL_END();

    len -= l;
  }
  cs_high(&g);
}

/*像素点填充*/
void writePixelPreclipped(int16_t x, int16_t y, uint16_t color)
{
    // CO5300最小开窗为2x2
    // writeAddrWindow(x, y, 1 + 42, 1 + 42);
    // _bus->writeRepeat(color, 45);
    writeAddrWindow(x, y, 2, 2);
    writeRepeat(color, 4);
}

void writeRepeatBuffer(uint16_t *buf, uint32_t len)
{
    uint32_t maxBlock = SPI_MAX_PIXELS_AT_ONCE;
    uint32_t offset = 0;
    cs_low(&g);

    while (len)
    {
        uint32_t block = (len > maxBlock) ? maxBlock : len;

        // 填充 SPI DMA 缓冲区
        for (uint32_t i = 0; i < block; i += 2)
        {
            uint32_t c32;
            MSB_32_16_16_SET(c32, buf[i], buf[i + 1]);
            g.txbuf._buffer32[i / 2] = c32;
        }

        g.tran_ext.base.tx_buffer = g.txbuf._buffer16;
        g.tran_ext.base.length = block << 4;
        POLL_START();
        POLL_END();

        len -= block;
        offset += block;
    }

    cs_high(&g);
}
// 批量刷新指定矩形区域
void flushArea(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t  *color_p)
{
    int16_t w = x2 - x1 + 1;
    int16_t h = y2 - y1 + 1;

    // 设置屏幕窗口
    writeAddrWindow(x1, y1, w, h);

    uint32_t totalPixels = w * h;
    uint32_t maxBlock = SPI_MAX_PIXELS_AT_ONCE;
    uint32_t offset = 0;

    while (totalPixels)
    {
        uint32_t block = (totalPixels > maxBlock) ? maxBlock : totalPixels;

        // LVGL 内存中连续的像素块批量写入
        writeRepeatBuffer((uint16_t *)(color_p + offset), block);

        totalPixels -= block;
        offset += block;
    }
}

/* 水平线绘制 */
void writeFastHLine(int16_t x, int16_t y,
    int16_t w, uint16_t color)
{
for (int16_t i = x; i < x + w; i++)
{
    if (_ordered_in_range(x, 0, g.max_x) && _ordered_in_range(y, 0, g.max_y))
    {
    writePixelPreclipped(i, y, color);
     }
}
}

/* 像素描点 */
void writePixel(int16_t x, int16_t y,uint16_t color)
{

    if (_ordered_in_range(x, 0, g.max_x) && _ordered_in_range(y, 0, g.max_y))
    {
        writePixelPreclipped(x, y, color);
     }
}

void writeBytes(uint8_t *data, uint32_t len)
{
  cs_low(&g);
  uint32_t l;
  bool first_send = true;
  while (len)
  {
    l = (len >= (SPI_MAX_PIXELS_AT_ONCE << 1)) ? (SPI_MAX_PIXELS_AT_ONCE << 1) : len;

    if (first_send)
    {
      g.tran_ext.base.flags = SPI_TRANS_MODE_QIO;
      g.tran_ext.base.cmd = 0x32;
      g.tran_ext.base.addr = 0x003C00;
      first_send = false;
    }
    else
    {
      g.tran_ext.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                                 SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
    }

    g.tran_ext.base.tx_buffer = data;
    g.tran_ext.base.length = l << 3;

    POLL_START();
    POLL_END();

    len -= l;
    data += l;
  }
  cs_high(&g);
}

void  draw16bitBeRGBBitmap(int16_t x, int16_t y,uint16_t *bitmap,int16_t w,int16_t h)
{   
    
    //int32_t offset = 0;
    
    // uint16_t p;
    // for (int16_t j = 0; j < h; j++,y++)
    // {
    //      for (int16_t i = 0; i < w; i++)
    //      {   
    //         p = bitmap[offset++];
    //         MSB_16_SET(p,p);
    //         writePixel(x + i, y, p);
    //      }
    //  }
    cs_low(&g);
    writeAddrWindow(x, y, w, h);
    writePixels((uint16_t *)bitmap, (uint32_t)w * h);
    cs_high(&g);
}

/* 设置显示亮度 */
void Display_Brightness(uint8_t brightness)
{
    cs_low(&g);
    writeC8D8(CO5300_W_WDBRIGHTNESSVALNOR, brightness);
    cs_high(&g);
}

/* 填充矩形裁剪 */
void writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h,
                                          uint16_t color)
{
    writeAddrWindow(x, y, w, h);  // 一次性设置整个区域窗口
    writeRepeat(color, w * h * 4); // 一次写入所有像素颜色（4 = 单位像素的字节或像素数，需根据实际调整）
}

/* 填充矩形 */
void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (w < 0) { x += w + 1; w = -w; }       // 负宽度处理，调整x并取正宽度
    if (h < 0) { y += h + 1; h = -h; }       // 负高度处理，调整y并取正高度
    if (w == 0 || h == 0) return;             // 宽高为0，无需绘制，直接返回

    int16_t x2 = x + w - 1, y2 = y + h - 1;  // 计算矩形右下角坐标
    if (x > g.max_x || y > g.max_y               // 矩形完全在屏幕外，直接返回
        || x2 < 0 || y2 < 0) return;

    if (x < 0) { w += x; x = 0; }             // 裁剪左边界，调整x和宽度
    if (y < 0) { h += y; y = 0; }             // 裁剪上边界，调整y和高度
    if (x2 > g.max_x) w = g.max_x - x + 1;      // 裁剪右边界，调整宽度
    if (y2 > g.max_y) h = g.max_y - y + 1;      // 裁剪下边界，调整高度
    if (w <= 0 || h <= 0) return;              // 裁剪后宽高无效，直接返回

    writeFillRectPreclipped(x, y, w, h, color); // 调用裁剪后绘制函数
}

void fillScreen(uint16_t color)
{
    fillRect(0, 0, g.width, g.height, color);
}

//矩形填充
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint16_t color)
{
    cs_low(&g);
    writeFillRect(x, y, w, h, color);
    cs_high(&g);
}



void drawBitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t data)
{
    cs_low(&g);
    writeAddrWindow(x, y, w, h);       // 设置开窗
    writeRepeat(data, w * h * 2); // 一次写入所有像素颜色（2 = 单位像素的字节或像素数，需根据实际调整）
    cs_high(&g);
}

void tra_test()
{   
    if (!TFT_begin()) {
        printf("TFT initialization failed\n");
        return;
    }
    
    // Brightness test
    for (int i = 0; i <= 255; i++)
    {
        Display_Brightness(i);
        vTaskDelay(pdMS_TO_TICKS(3));
    }
    
}
