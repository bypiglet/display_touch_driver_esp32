#include "driver/i2c.h"
#include <stdint.h>
#include <stdbool.h>

/**************************************************************************
 * 一、手势识别与触摸数量相关寄存器
 * 功能：获取当前检测到的手势类型、触摸手指个数，是触控交互的核心状态寄存器
 **************************************************************************/
// 1. 手势ID寄存器（GestureID）：存储当前识别到的手势编码，读取后需结合枚举判断手势类型
//    取值范围：
//    0x00 - 无手势
//    0x01 - 上滑
//    0x02 - 下滑
//    0x03 - 左滑
//    0x04 - 右滑
//    0x05 - 单击
//    0x0B - 双击
//    0x0C - 长按
#define CST816S_GESTURE_ID_REG    0x01    

// 2. 手指数量寄存器（FingerNum）：存储当前检测到的触摸手指个数，仅支持0或1个手指检测
//    取值说明：
//    0 - 无手指触摸
//    1 - 1个手指触摸
#define CST816S_FINGER_NUM_REG    0x02    

/**************************************************************************
 * 二、触摸坐标相关寄存器（12位精度）
 * 功能：获取触摸点的X/Y轴坐标，采用“高4位+低8位”拆分存储，需组合为12位值使用
 * 精度说明：12位坐标可表示0~4095的数值范围，适配不同分辨率的触控面板
 **************************************************************************/
// X轴坐标高4位寄存器（XposH）：存储X坐标的高4位数据，低4位无意义（需与XposL组合）
#define CST816S_XPOS_H_REG        0x03    

// X轴坐标低8位寄存器（XposL）：存储X坐标的低8位数据，是X轴坐标的主要组成部分
#define CST816S_XPOS_L_REG        0x04    

// Y轴坐标高4位寄存器（YposH）：存储Y坐标的高4位数据，低4位无意义（需与YposL组合）
#define CST816S_YPOS_H_REG        0x05    

// Y轴坐标低8位寄存器（YposL）：存储Y坐标的低8位数据，是Y轴坐标的主要组成部分
#define CST816S_YPOS_L_REG        0x06    

/**************************************************************************
 * 三、触摸压力相关寄存器（16位精度）
 * 功能：获取触摸点的压力值（BPC，Button Pressure Control），采用“高8位+低8位”拆分存储
 * 应用场景：需区分触摸力度的场景（如按压灵敏度调节），2个通道支持不同区域压力检测
 **************************************************************************/
// 压力通道0高8位寄存器（BPC0H）：存储BPC0压力值的高8位数据
#define CST816S_BPC0_H_REG        0xB0    

// 压力通道0低8位寄存器（BPC0L）：存储BPC0压力值的低8位数据
#define CST816S_BPC0_L_REG        0xB1    

// 压力通道1高8位寄存器（BPC1H）：存储BPC1压力值的高8位数据
#define CST816S_BPC1_H_REG        0xB2    

// 压力通道1低8位寄存器（BPC1L）：存储BPC1压力值的低8位数据
#define CST816S_BPC1_L_REG        0xB3    

/**************************************************************************
 * 四、芯片基础信息寄存器
 * 功能：获取芯片型号、工程编号、固件版本，用于芯片识别、兼容性判断和版本管理
 **************************************************************************/
// 芯片ID寄存器（ChipID）：存储芯片唯一型号标识，用于确认是否为CST816S芯片
#define CST816S_CHIP_ID_REG       0xA7    

// 工程编号寄存器（ProjID）：存储芯片的工程批次编号，用于生产追溯和版本区分
#define CST816S_PROJ_ID_REG       0xA8    

// 固件版本寄存器（FwVersion）：存储芯片当前运行的固件版本号，用于固件升级判断
#define CST816S_FW_VERSION_REG    0xA9    

/**************************************************************************
 * 五、手势功能与扫描配置寄存器
 * 功能：配置手势使能状态、中断脉冲宽度、正常扫描周期、滑动角度，控制触控交互逻辑
 **************************************************************************/
// 手势功能掩码寄存器（MotionMask）：单独使能/禁用特定手势功能，按位控制
#define CST816S_MOTION_MASK_REG   0xEC    

// 中断脉冲宽度寄存器（IrqPluseWidth）：配置中断引脚输出低脉冲的宽度，影响中断响应时间
#define CST816S_IRQ_PULSE_WIDTH_REG 0xED  

// 正常扫描周期寄存器（NorScanPer）：配置正常模式下的触控检测周期，影响响应速度和功耗
#define CST816S_NOR_SCAN_PER_REG  0xEE    

// 滑动角度控制寄存器（MotionSlAngle）：配置手势滑动分区的角度阈值，影响滑动手势识别精度
#define CST816S_MOTION_SL_ANGLE_REG 0xEF  

/**************************************************************************
 * 六、低功耗模式配置寄存器
 * 功能：配置低功耗扫描的基准值、唤醒周期、门限、量程等参数，平衡功耗与检测灵敏度
 **************************************************************************/
// 低功耗扫描1号通道基准值高8位（LpScanRaw1H）：存储1号通道低功耗扫描的基准值高8位
#define CST816S_LP_SCAN_RAW1_H_REG 0xF0   

// 低功耗扫描1号通道基准值低8位（LpScanRaw1L）：存储1号通道低功耗扫描的基准值低8位
#define CST816S_LP_SCAN_RAW1_L_REG 0xF1   

// 低功耗扫描2号通道基准值高8位（LpScanRaw2H）：存储2号通道低功耗扫描的基准值高8位
#define CST816S_LP_SCAN_RAW2_H_REG 0xF2   

// 低功耗扫描2号通道基准值低8位（LpScanRaw2L）：存储2号通道低功耗扫描的基准值低8位
#define CST816S_LP_SCAN_RAW2_L_REG 0xF3   

// 低功耗自动唤醒周期寄存器（LpAutoWakeTime）：配置低功耗模式下的自动重校正周期
#define CST816S_LP_AUTO_WAKE_TIME_REG 0xF4 

// 低功耗扫描唤醒门限寄存器（LpScanTH）：配置低功耗模式下触发唤醒的触摸信号阈值
#define CST816S_LP_SCAN_TH_REG     0xF5    

// 低功耗扫描量程寄存器（LpScanWin）：配置低功耗模式下的扫描检测范围
#define CST816S_LP_SCAN_WIN_REG    0xF6    

// 低功耗扫描频率寄存器（LpScanFreq）：配置低功耗模式下的扫描频率
#define CST816S_LP_SCAN_FREQ_REG   0xF7    

// 低功耗扫描电流寄存器（LpScanIdac）：配置低功耗模式下的扫描电路电流
#define CST816S_LP_SCAN_IDAC_REG   0xF8    

// 自动休眠时间寄存器（AutoSleepTime）：配置无触摸时自动进入低功耗模式的延迟时间
#define CST816S_AUTO_SLEEP_TIME_REG 0xF9  

/**************************************************************************
 * 七、中断控制相关寄存器
 * 功能：配置中断触发条件、防抖时间、长按复位时间，控制中断信号的产生逻辑
 **************************************************************************/
// 中断控制寄存器（IrqCtl）：按位配置中断功能的使能状态，控制不同场景下的中断输出
#define CST816S_IRQ_CTL_REG       0xFA    

// 防抖时间寄存器（DebounceTime）：配置触摸防抖时间，避免触摸抖动导致的误触发
#define CST816S_DEBOUNCE_TIME_REG 0xFB    

// 长按复位时间寄存器（LongPressTime）：配置长按手势触发后自动复位的延迟时间
#define CST816S_LONG_PRESS_TIME_REG 0xFC  

/**************************************************************************
 * 八、芯片控制与IO配置寄存器
 * 功能：配置芯片软复位、IIC驱动模式、IO电平，控制芯片基础工作状态
 **************************************************************************/
// IO控制寄存器（IOCtl）：配置软复位、IIC驱动模式、IO引脚电平
#define CST816S_IO_CTL_REG        0xFD    

// 禁止自动休眠寄存器（DisAutoSleep）：控制是否禁用自动进入低功耗模式的功能
#define CST816S_DIS_AUTO_SLEEP_REG 0xFE   

/**************************************************************************
 * 九、手势类型枚举（CST816S_GestureType）
 * 功能：将GestureID寄存器的数值映射为可读的手势类型，避免直接使用硬编码，提升代码可读性
 **************************************************************************/
typedef enum {
    CST816S_GESTURE_NONE = 0x00,    /* 无手势 */
    CST816S_GESTURE_UP = 0x01,      /* 上滑手势 */
    CST816S_GESTURE_DOWN = 0x02,    /* 下滑手势 */
    CST816S_GESTURE_LEFT = 0x03,    /* 左滑手势 */
    CST816S_GESTURE_RIGHT = 0x04,   /* 右滑手势 */
    CST816S_GESTURE_CLICK = 0x05,   /* 单击手势 */
    CST816S_GESTURE_DOUBLE_CLICK = 0x0B, /* 双击手势 */
    CST816S_GESTURE_LONG_PRESS = 0x0C  /* 长按手势 */
} CST816S_GestureType;

/**************************************************************************
 * 十、寄存器位操作宏（按寄存器分组）
 * 功能：提供精准的位操作接口，避免直接操作寄存器数值导致的误修改
 **************************************************************************/
// MotionMask寄存器位操作宏（手势功能使能）
#define CST816S_MOTION_MASK_EN_CON_LR  (1 << 2)  /* 使能连续左右滑动 */
#define CST816S_MOTION_MASK_DIS_CON_LR (0 << 2)  /* 禁用连续左右滑动 */
#define CST816S_MOTION_MASK_EN_CON_UD  (1 << 1)  /* 使能连续上下滑动 */
#define CST816S_MOTION_MASK_DIS_CON_UD (0 << 1)  /* 禁用连续上下滑动 */
#define CST816S_MOTION_MASK_EN_DCLICK  (1 << 0)  /* 使能双击功能 */
#define CST816S_MOTION_MASK_DIS_DCLICK (0 << 0)  /* 禁用双击功能 */

// IrqCtl寄存器位操作宏（中断功能使能）
#define CST816S_IRQ_CTL_EN_TEST        (1 << 7)  /* 使能中断引脚测试 */
#define CST816S_IRQ_CTL_DIS_TEST       (0 << 7)  /* 禁用中断引脚测试 */
#define CST816S_IRQ_CTL_EN_TOUCH       (1 << 5)  /* 使能触摸检测中断 */
#define CST816S_IRQ_CTL_DIS_TOUCH      (0 << 5)  /* 禁用触摸检测中断 */
#define CST816S_IRQ_CTL_EN_CHANGE      (1 << 4)  /* 使能触摸状态变化中断 */
#define CST816S_IRQ_CTL_DIS_CHANGE     (0 << 4)  /* 禁用触摸状态变化中断 */
#define CST816S_IRQ_CTL_EN_MOTION      (1 << 3)  /* 使能手势检测中断 */
#define CST816S_IRQ_CTL_DIS_MOTION     (0 << 3)  /* 禁用手势检测中断 */
#define CST816S_IRQ_CTL_ONCE_WLP       (1 << 0)  /* 长按单次中断 */
#define CST816S_IRQ_CTL_MULTI_WLP      (0 << 0)  /* 长按多次中断 */

// IOCtl寄存器位操作宏（IO配置）
#define CST816S_IO_CTL_EN_SOFT_RST     (1 << 1)  /* 使能软复位 */
#define CST816S_IO_CTL_DIS_SOFT_RST    (0 << 1)  /* 禁用软复位 */
#define CST816S_IO_CTL_IIC_OD_MODE     (1 << 0)  /* IIC开漏输出模式 */
#define CST816S_IO_CTL_IIC_PULLUP_MODE (0 << 0)  /* IIC电阻上拉模式 */
#define CST816S_IO_CTL_EN_1V8          (1 << 0)  /* IO电平1.8V */
#define CST816S_IO_CTL_EN_VDD          (0 << 0)  /* IO电平VDD */

/**************************************************************************
 * 十一、寄存器默认参数宏
 * 功能：存储手册中明确的默认配置值，初始化时可直接使用，避免重复查询手册
 **************************************************************************/
#define CST816S_DEFAULT_IRQ_PULSE_WIDTH  10    /* 中断脉冲宽度默认值：10（单位0.1ms，对应1ms） */
#define CST816S_DEFAULT_NOR_SCAN_PER     1     /* 正常扫描周期默认值：1（单位10ms，对应10ms） */
#define CST816S_DEFAULT_LP_AUTO_WAKE_TIME 5    /* 低功耗自动唤醒周期默认值：5（单位1分钟，对应5分钟） */
#define CST816S_DEFAULT_LP_SCAN_TH       48    /* 低功耗唤醒门限默认值：48 */
#define CST816S_DEFAULT_LP_SCAN_WIN      3     /* 低功耗扫描量程默认值：3 */
#define CST816S_DEFAULT_LP_SCAN_FREQ     7     /* 低功耗扫描频率默认值：7 */
#define CST816S_DEFAULT_AUTO_SLEEP_TIME  2     /* 自动休眠时间默认值：2（单位1秒，对应2秒） */
#define CST816S_DEFAULT_DEBOUNCE_TIME    5     /* 防抖时间默认值：5（单位1秒，对应5秒） */
#define CST816S_DEFAULT_LONG_PRESS_TIME  10    /* 长按复位时间默认值：10（单位1秒，对应10秒） */

/**************************************************************************
 * 十二、辅助计算宏
 * 功能：简化多字节寄存器的组合计算，避免重复编写组合逻辑，减少代码冗余
 **************************************************************************/
// 1. 组合16位数值（高8位+低8位）：适用于压力值、低功耗扫描基准值等16位数据
#define CST816S_COMBINE_16BIT(high_reg, low_reg)  (((uint16_t)high_reg << 8) | low_reg)

// 2. 组合12位坐标值（高4位+低8位）：适用于X/Y轴坐标（高4位仅取低4位有效数据）
#define CST816S_COMBINE_12BIT(high_reg, low_reg)  (((uint16_t)(high_reg & 0x0F) << 8) | low_reg) 




// TOUCH
#define TP_INT 10   //9
#define TP_RST 9  //8
#define IIC_SDA 8 //7
#define IIC_SCL 7   //6

#define I2C_SPEED 50000 // 100 kHz
#define I2C_ADDR 0x15 

typedef struct CST820
{
//    uint8_t XposH;
//    uint8_t XposL;
//    uint8_t YposH;
//    uint8_t YposL;
   uint16_t Xpos;
   uint16_t Ypos;
} cst820_info_t;


bool cst820_read_touch(cst820_info_t *info);

void cst820_init(void);
esp_err_t cst820_read_byte(uint8_t reg, uint8_t *data_rd);
esp_err_t cst820_read_bytes(uint8_t reg, uint8_t *data_rd, size_t len);
bool my_touchpad_is_pressed(void* arg);
void read_allreg();