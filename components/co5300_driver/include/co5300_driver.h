#ifndef CO5300_DRIVER_H
#define CO5300_DRIVER_H

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/spi_common.h"
#include "soc/gpio_struct.h"
#include "string.h"

#define LCD_SDIO0 41   //11
#define LCD_SDIO1 1   //13
#define LCD_SDIO2 40    //14  
#define LCD_SDIO3 39        //15   
#define LCD_SCLK 45     //12
#define LCD_CS 21       //10
#define LCD_RST 6       //17  45
#define LCD_WIDTH  368
#define LCD_HEIGHT 448

#define SPI_MAX_PIXELS_AT_ONCE 1024
#define QSPI_FREQUENCY 40000000
#define QSPI_SPI_MODE 0
#define QSPI_SPI_HOST SPI2_HOST
#define QSPI_DMA_CHANNEL SPI_DMA_CH_AUTO

#define RGB565(r, g, b) ((((r)&0xF8) << 8) | (((g)&0xFC) << 3) | ((b) >> 3))

#define RGB565_YELLOW RGB565(255, 255, 0)
#define YELLOW RGB565_YELLOW

#define MSB_16(val) (((val)&0xFF00) >> 8) | (((val)&0xFF) << 8)

#define MSB_16_SET(var, val) \
  {                          \
    (var) = MSB_16(val);     \
  }
  
#define MSB_32_16_16_SET(var, v1, v2)                                                                                   \
  {                                                                                                                     \
    (var) = (((uint32_t)v2 & 0xff00) << 8) | (((uint32_t)v2 & 0xff) << 24) | ((v1 & 0xff00) >> 8) | ((v1 & 0xff) << 8); \
  }


// SPI command constants
#define SPI_CMD_WRITE 0x02
#define SPI_CMD_READ  0x32

// Range checking macro
#define _ordered_in_range(val, min, max) ((val) >= (min) && (val) <= (max))


#define CO5300_MAXWIDTH 480  ///< CO5300 max TFT width
#define CO5300_MAXHEIGHT 480 ///< CO5300 max TFT width

#define CO5300_RST_DELAY 200    ///< delay ms wait for reset finish
#define CO5300_SLPIN_DELAY 120  ///< delay ms wait for sleep in finish
#define CO5300_SLPOUT_DELAY 120 ///< delay ms wait for sleep out finish

// User Command
#define CO5300_C_NOP 0x00          // nop
#define CO5300_C_SWRESET 0x01      // Software Reset
#define CO5300_R_RDID 0x04         // Read Display Identification Information ID/1/2/3
#define CO5300_R_RDNERRORSDSI 0x05 // Read Number of Errors on DSI
#define CO5300_R_RDPOWERMODE 0x0A  // Read Display Power Mode
#define CO5300_R_RDMADCTL 0x0B     // Read Display MADCTL
#define CO5300_R_RDPIXFMT 0x0C     // Read Display Pixel Format
#define CO5300_R_RDIMGFMT 0x0D     // Read Display Image Mode
#define CO5300_R_RDSIGMODE 0x0E    // Read Display Signal Mode
#define CO5300_R_RDSELFDIAG 0x0F   // Read Display Self-Diagnostic Result

#define CO5300_C_SLPIN 0x10  // Sleep In
#define CO5300_C_SLPOUT 0x11 // Sleep Out
#define CO5300_C_PTLON 0x12  // Partial Display On
#define CO5300_C_NORON 0x13  // Normal Display mode on

#define CO5300_C_INVOFF 0x20  // Inversion Off
#define CO5300_C_INVON 0x21   // Inversion On
#define CO5300_C_ALLPOFF 0x22 // All pixels off
#define CO5300_C_ALLPON 0x23  // All pixels on
#define CO5300_C_DISPOFF 0x28 // Display off
#define CO5300_C_DISPON 0x29  // Display on
#define CO5300_W_CASET 0x2A   // Column Address Set
#define CO5300_W_PASET 0x2B   // Page Address Set
#define CO5300_W_RAMWR 0x2C   // Memory Write Start

#define CO5300_W_PTLAR 0x30   // Partial Area Row Set
#define CO5300_W_PTLAC 0x31   // Partial Area Column Set
#define CO5300_C_TEAROFF 0x34 // Tearing effect off
#define CO5300_WC_TEARON 0x35 // Tearing effect on
#define CO5300_W_MADCTL 0x36  // Memory data access control
#define CO5300_C_IDLEOFF 0x38 // Idle Mode Off
#define CO5300_C_IDLEON 0x39  // Idle Mode On
#define CO5300_W_PIXFMT 0x3A  // Write Display Pixel Format
#define CO5300_W_WRMC 0x3C    // Memory Write Continue

#define CO5300_W_SETTSL 0x44             // Write Tearing Effect Scan Line
#define CO5300_R_GETSL 0x45              // Read Scan Line Number
#define CO5300_C_SPIROFF 0x46            // SPI read Off
#define CO5300_C_SPIRON 0x47             // SPI read On
#define CO5300_C_AODMOFF 0x48            // AOD Mode Off
#define CO5300_C_AODMON 0x49             // AOD Mode On
#define CO5300_W_WDBRIGHTNESSVALAOD 0x4A // Write Display Brightness Value in AOD Mode
#define CO5300_R_RDBRIGHTNESSVALAOD 0x4B // Read Display Brightness Value in AOD Mode
#define CO5300_W_DEEPSTMODE 0x4F         // Deep Standby Mode On

#define CO5300_W_WDBRIGHTNESSVALNOR 0x51 // Write Display Brightness Value in Normal Mode
#define CO5300_R_RDBRIGHTNESSVALNOR 0x52 // Read display brightness value in Normal Mode
#define CO5300_W_WCTRLD1 0x53            // Write CTRL Display1
#define CO5300_R_RCTRLD1 0x54            // Read CTRL Display1
#define CO5300_W_WCTRLD2 0x55            // Write CTRL Display2
#define CO5300_R_RCTRLD2 0x56            // Read CTRL Display2
#define CO5300_W_WCE 0x58                // Write CE
#define CO5300_R_RCE 0x59                // Read CE

#define CO5300_W_WDBRIGHTNESSVALHBM 0x63 // Write Display Brightness Value in HBM Mode
#define CO5300_R_WDBRIGHTNESSVALHBM 0x64 // Read Display Brightness Value in HBM Mode
#define CO5300_W_WHBMCTL 0x66            // Write HBM Control

#define CO5300_W_COLORSET0 0x70  // Color Set 0
#define CO5300_W_COLORSET1 0x71  // Color Set 1
#define CO5300_W_COLORSET2 0x72  // Color Set 2
#define CO5300_W_COLORSET3 0x73  // Color Set 3
#define CO5300_W_COLORSET4 0x74  // Color Set 4
#define CO5300_W_COLORSET5 0x75  // Color Set 5
#define CO5300_W_COLORSET6 0x76  // Color Set 6
#define CO5300_W_COLORSET7 0x77  // Color Set 7
#define CO5300_W_COLORSET8 0x78  // Color Set 8
#define CO5300_W_COLORSET9 0x79  // Color Set 9
#define CO5300_W_COLORSET10 0x7A // Color Set 10
#define CO5300_W_COLORSET11 0x7B // Color Set 11
#define CO5300_W_COLORSET12 0x7C // Color Set 12
#define CO5300_W_COLORSET13 0x7D // Color Set 13
#define CO5300_W_COLORSET14 0x7E // Color Set 14
#define CO5300_W_COLORSET15 0x7F // Color Set 15

#define CO5300_W_COLOROPTION 0x80 // Color Option

#define CO5300_R_RDDBSTART 0xA1         // Read DDB start
#define CO5300_R_DDBCONTINUE 0xA8       // Read DDB Continue
#define CO5300_R_RFIRCHECKSUN 0xAA      // Read First Checksum
#define CO5300_R_RCONTINUECHECKSUN 0xAF // Read Continue Checksum

#define CO5300_W_SPIMODECTL 0xC4 // SPI mode control

#define CO5300_R_RDID1 0xDA // Read ID1
#define CO5300_R_RDID2 0xDB // Read ID2
#define CO5300_R_RDID3 0xDC // Read ID3

// Flip
#define CO5300_MADCTL_X_AXIS_FLIP 0x02 // Flip Horizontal
#define CO5300_MADCTL_Y_AXIS_FLIP 0x05 // Flip Vertical

// Color Order
#define CO5300_MADCTL_RGB 0x00                      // Red-Green-Blue pixel order
#define CO5300_MADCTL_BGR 0x08                      // Blue-Green-Red pixel order
#define CO5300_MADCTL_COLOR_ORDER CO5300_MADCTL_RGB // RGB

enum
{
    CO5300_ContrastOff = 0,
    CO5300_LowContrast,
    CO5300_MediumContrast,
    CO5300_HighContrast
};

typedef enum
{
  BEGIN_WRITE,
  WRITE_COMMAND_8,
  WRITE_COMMAND_16,
  WRITE_DATA_8,
  WRITE_DATA_16,
  WRITE_BYTES,
  WRITE_C8_D8,
  WRITE_C8_D16,
  WRITE_C16_D16,
  END_WRITE,
  DELAY,
} spi_operation_type_t;

static const uint8_t co5300_init_operations[] = {

    BEGIN_WRITE, 

    WRITE_COMMAND_8, CO5300_C_SLPOUT, // Sleep Out

    //WRITE_COMMAND_8, CO5300_C_INVOFF, // Inversion Off

    END_WRITE,

    DELAY, CO5300_SLPOUT_DELAY,

    BEGIN_WRITE,

    //WRITE_C8_D8, CO5300_WC_TEARON, 0x00,

    WRITE_C8_D8, 0xFE, 0x00,

    WRITE_C8_D8, CO5300_W_SPIMODECTL, 0x80,

    WRITE_C8_D8, CO5300_W_MADCTL, CO5300_MADCTL_COLOR_ORDER, // RGB/BGR

    WRITE_C8_D8, CO5300_W_PIXFMT, 0x55, // Interface Pixel Format 16bit/pixel
    // WRITE_C8_D8, CO5300_W_PIXFMT, 0x66, // Interface Pixel Format 18bit/pixel
    // WRITE_C8_D8, CO5300_W_PIXFMT, 0x77, // Interface Pixel Format 24bit/pixel

    WRITE_C8_D8, CO5300_W_WCTRLD1, 0x20,

    WRITE_C8_D8, CO5300_W_WDBRIGHTNESSVALHBM, 0xFF,

    WRITE_COMMAND_8, CO5300_W_CASET,
    // WRITE_BYTES, 4,
    // //0x00, 0x14, 0x01, 0x2B,
    // 0x00, 0x10, 0x01, 0x7F,
    // WRITE_COMMAND_8, CO5300_W_PASET,
    // WRITE_BYTES, 4,
    // //0x00, 0x00, 0x01, 0xC7,
    // 0x00, 0x00, 0x01, 0xBF,

    WRITE_COMMAND_8, CO5300_C_DISPON, // Display ON

    WRITE_C8_D8, CO5300_W_WDBRIGHTNESSVALNOR, 0x00, // Brightness adjustment

    // High contrast mode (Sunlight Readability Enhancement)
    WRITE_C8_D8, CO5300_W_WCE, 0x00, // Off
    // WRITE_C8_D8, CO5300_W_WCE, 0x05, // On Low
    // WRITE_C8_D8, CO5300_W_WCE, 0x06, // On Medium
    // WRITE_C8_D8, CO5300_W_WCE, 0x07, // On High

    END_WRITE,

    DELAY, 10};


bool begin();
bool TFT_begin();
void tra_test();
void writeCommand(uint8_t c);
void writeC8D8(uint8_t c, uint8_t d);
void POLL_START();
void POLL_END();
void CS_HIGH(void);
void CS_LOW(void);
void beginWrite();
void endWrite();

void writeAddrWindow(int16_t x, int16_t y, uint16_t w, uint16_t h);
void writePixelPreclipped(int16_t x, int16_t y, uint16_t zcolor);
void draw16bitBeRGBBitmap(int16_t x, int16_t y,uint16_t *bitmap,int16_t w,int16_t h);
void writeFastHLine(int16_t x, int16_t y,int16_t w, uint16_t color);
void batchOperation(const uint8_t *operations, size_t len);
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                uint16_t color);
void fillScreen(uint16_t color);
void drawBitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t data);
void Display_Brightness(uint8_t brightness);
void writePixels(uint16_t *data, uint32_t len);
void writePixel(int16_t x, int16_t y, uint16_t color);
void flushArea(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t  *color_p);

// Global variables declaration
extern uint8_t _xStart, _yStart;
extern int16_t _currentX, _currentY;
extern uint16_t _currentW, _currentH;
extern int16_t _max_x, _max_y;

#endif // CO5300_DRIVER_H