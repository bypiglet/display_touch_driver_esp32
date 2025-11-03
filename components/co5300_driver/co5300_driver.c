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


bool co5300_spi_init(void)
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

    spi_bus_config_t bus_cfg = {
        .mosi_io_num    = LCD_SDIO0,
        .miso_io_num    = LCD_SDIO1, 
        .sclk_io_num    = LCD_SCLK,
        .quadwp_io_num  = LCD_SDIO2,
        .quadhd_io_num  = LCD_SDIO3,
        .max_transfer_sz = (SPI_MAX_PIXELS_AT_ONCE * 16) + 8,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS,
    };

    esp_err_t ret = spi_bus_initialize(QSPI_SPI_HOST, &bus_cfg, QSPI_DMA_CHANNEL);
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


/**********旋转调试**********/

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


bool co5300_begin()
{
    if (!co5300_spi_init()) {
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

static inline void beginWrite()
{
    cs_low(&g);
}

static inline void endWrite()
{
    cs_high(&g);
}

 static void POLL_START()
{
  //esp_err_t ret = spi_device_polling_start(g.spi, g.tran_base, portMAX_DELAY);
  esp_err_t ret = spi_device_polling_transmit(g.spi, g.tran_base);
  if (ret != ESP_OK)
  {
    printf("spi_device_polling_start error: %d\n", ret);
    // Consider adding error handling here
  }
}

 static void POLL_END()
{
  //esp_err_t ret = spi_device_polling_end(g.spi, portMAX_DELAY);
  // if (ret != ESP_OK)
  // {
  //   printf("spi_device_polling_end error: %d\n", ret);
  //   // Consider adding error handling here
  // }

}


//4线写数据8位
 static void write8(uint8_t d)
{
  cs_low(&g);
  g.tran_ext.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_MODE_QIO;
  g.tran_ext.base.cmd = SPI_CMD_QUAD_WRITE_PIXEL;
  g.tran_ext.base.addr = 0x003C00;
  g.tran_ext.base.tx_data[0] = d;
  g.tran_ext.base.length = 8;
  POLL_START();
  POLL_END();
  cs_high(&g);  
}

//单线写数据8位
static void writeCommand(uint8_t c)
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

//地址可控的写命令+8位数据
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

//地址可控的写命令+16位数据+16位数据
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

//批量操作
void batchOperation(const uint8_t *ops, size_t len)
{
    size_t i = 0;
    while(i < len)
    {
        uint8_t op = ops[i++];
        uint8_t data_len = 0;
        
        switch(op)
        {
        case BEGIN_WRITE:
            beginWrite();
            continue;

        case END_WRITE:
            endWrite();
            continue;

        case DELAY:
            if (i < len) vTaskDelay(ops[i++]);
            continue;

        case WRITE_C8_D8:
            if (i + 2 <= len) writeC8D8(ops[i], ops[i+1]);
            i += 2;
            continue;

        case WRITE_COMMAND_8:
            if (i < len) writeCommand(ops[i++]);
            continue;

        case WRITE_BYTES: 
            if (i < len) data_len = ops[i++];
            break;

        case WRITE_DATA_8:
            data_len = 1;
            break;

        case WRITE_DATA_16:
            data_len = 2;
            break;

        default:
            printf("Unknown op %d at %zu\n", op, i-1);
            continue;
        }

        // 通用数据拷贝
        while (data_len-- && i < len)
        {
            write8(ops[i++]);
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
    writeAddrWindow(x, y, 2, 2);
    writeRepeat(color, 4);
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

// 填充矩形区域
// x, y: 左上角坐标
// w, h: 宽和高
// color: 填充颜色
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    // CO5300最小开窗为 2x2 对齐，若需要可在此对 w,h 对齐处理（可选）
    int32_t totalPixels = w * h;

    // 设置写入区域
    writeAddrWindow(x, y, w, h);

    // 批量写入
    writeRepeat(color, totalPixels);
}

void  draw16bitBeRGBBitmap(int16_t x, int16_t y,uint16_t *bitmap,int16_t w,int16_t h)
{   
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



void tra_test()
{   
    if (!co5300_begin()) {
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
