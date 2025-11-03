#include "driver/i2c.h"
#include <stdint.h>
#include <stdbool.h>

#define CST820_I2C_ADDR        0x15         // I2C 设备地址（7bit:0x15 → Write:0x2A / Read:0x2B）
#define CST820_REG_GESTURE     0x01         // 手势 ID（Gesture ID）0x00:无 0x01:上滑 0x02:下滑 0x03:左滑 0x04:右滑 0x05:单击 0x0B:双击 0x0C:长按
#define CST820_REG_FINGER_NUM  0x02         // 当前触摸手指数量（0~2）
#define CST820_REG_XH          0x03         // X 坐标高 4bit + 手指状态 Bits[7:4]
#define CST820_REG_XL          0x04         // X 坐标低 8bit，坐标 = ((XH & 0x0F) << 8) | XL
#define CST820_REG_YH          0x05         // Y 坐标高 4bit + 手指状态 Bits[7:4]
#define CST820_REG_YL          0x06         // Y 坐标低 8bit，坐标 = ((YH & 0x0F) << 8) | YL    


typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t gesture;
    uint8_t finger;
} cst820_info_t;

// TOUCH
#define TP_INT 10   //9
#define TP_RST 9  //8
#define IIC_SDA 8 //7
#define IIC_SCL 7   //6

bool cst820_init(void);
bool cst820_read_touch(cst820_info_t *info);