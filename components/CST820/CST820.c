#include "cst820.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DATA_LENGTH 2

static const char *TAG = "CST820";
static i2c_port_t I2C_PORT = I2C_NUM_0;

static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;
static volatile bool touchflag = false;

/** I2C初始化 */
static void cst820_i2c_init(void)
{
    i2c_master_bus_config_t i2c_mst_config  = {
        .sda_io_num = IIC_SDA,
        .scl_io_num = IIC_SCL,
        .i2c_port = I2C_PORT,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = I2C_ADDR,
        .scl_speed_hz    = I2C_SPEED,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));
}

//从设备读取写入1字节
static void cst820_write_byte(uint8_t reg, uint8_t data)
{
    uint8_t data_wr[DATA_LENGTH]={0};
    data_wr[0] = reg;
    data_wr[1] = data;

    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, data_wr, DATA_LENGTH, -1));
}   

//从设备读取1字节
esp_err_t cst820_read_byte(uint8_t reg, uint8_t *data_rd)
{
    memset(&data_rd[0],0,2);
    i2c_master_transmit(dev_handle,&reg,1,1000 / portTICK_PERIOD_MS);
    esp_err_t ret = i2c_master_receive(dev_handle,data_rd,5,1000 / portTICK_PERIOD_MS);
    printf("寄存器地址:0X%02X:0X%02X ", reg,data_rd[0]);

    return ret;
}



//从设备读取多字节
esp_err_t cst820_read_bytes(uint8_t reg, uint8_t *data_rd, size_t len)
{
    memset(&data_rd[0],0,len);
    
    esp_err_t ret = i2c_master_transmit_receive(dev_handle,&reg,1,data_rd,len,1000 / portTICK_PERIOD_MS);
    for (size_t i = 0; i < len; i++) {
        printf("寄存器地址:0X%02X:0X%02X ", reg + i, data_rd[i]); // 打印每个寄存器的地址和值
    }
    printf("X坐标是: %d ", ((data_rd[0] & 0x0F)<<8 | data_rd[1]));
    printf("Y坐标是: %d \n", ((data_rd[2] & 0x0F)<<8 | data_rd[3]));
    return ret;
}

bool cst820_read_touch(cst820_info_t *info)
{
    uint8_t buf[4] = {0};
    cst820_read_bytes(0x03, &buf[0], 4);
    info->Xpos =  ((buf[0] & 0x0F)<<8 | buf[1]);
    info->Ypos =   ((buf[2] & 0x0F)<<8 | buf[3]);
    return true;
}


#define ESP_INTR_FLAG_DEFAULT 0

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    touchflag = true;
}

void input_init()
{
    gpio_config_t io_conf;
    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_LOW_LEVEL;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = (1ULL<<TP_INT);
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 1;
    gpio_config(&io_conf);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(TP_INT, gpio_isr_handler, (void*) TP_INT);

    //cst820_write_byte(CST816S_IRQ_CTL_REG,0x60); // 配置IO控制寄存器，设置IIC电阻上拉模式，IO电平VDD
}


void cst820_init(void)
{
    //初始化I2C
    cst820_i2c_init();
    //初始化中断
    input_init();
}



bool my_touchpad_is_pressed(void* arg)
{
    uint32_t io_num;
        if (touchflag == true) {
            //printf("Touch detected\n");
            touchflag = false;
            return true;
        }
        //printf("No touch detected\n");
        return false;
}





void read_allreg()
{
    uint8_t data[1] ={0};
    cst820_read_byte(CST816S_GESTURE_ID_REG, data);        // 读取寄存器 0x01
    cst820_read_byte(CST816S_FINGER_NUM_REG, data);        // 读取寄存器 0x02
    cst820_read_byte(CST816S_XPOS_H_REG, data);            // 读取寄存器 0x03
    cst820_read_byte(CST816S_XPOS_L_REG, data);            // 读取寄存器 0x04
    cst820_read_byte(CST816S_YPOS_H_REG, data);            // 读取寄存器 0x05
    cst820_read_byte(CST816S_YPOS_L_REG, data);            // 读取寄存器 0x06
    cst820_read_byte(CST816S_BPC0_H_REG, data);            // 读取寄存器 0xB0
    cst820_read_byte(CST816S_BPC0_L_REG, data);            // 读取寄存器 0xB1
    cst820_read_byte(CST816S_BPC1_H_REG, data);            // 读取寄存器 0xB2
    cst820_read_byte(CST816S_BPC1_L_REG, data);            // 读取寄存器 0xB3
    cst820_read_byte(CST816S_CHIP_ID_REG, data);           // 读取寄存器 0xA7
    cst820_read_byte(CST816S_PROJ_ID_REG, data);           // 读取寄存器 0xA8
    cst820_read_byte(CST816S_FW_VERSION_REG, data);        // 读取寄存器 0xA9
    cst820_read_byte(CST816S_MOTION_MASK_REG, data);       // 读取寄存器 0xEC
    cst820_read_byte(CST816S_IRQ_PULSE_WIDTH_REG, data);   // 读取寄存器 0xED
    cst820_read_byte(CST816S_NOR_SCAN_PER_REG, data);      // 读取寄存器 0xEE
    cst820_read_byte(CST816S_MOTION_SL_ANGLE_REG, data);  // 读取寄存器 0xEF
    cst820_read_byte(CST816S_LP_SCAN_RAW1_H_REG, data);    // 读取寄存器 0xF0
    cst820_read_byte(CST816S_LP_SCAN_RAW1_L_REG, data);    // 读取寄存器 0xF1
    cst820_read_byte(CST816S_LP_SCAN_RAW2_H_REG, data);    // 读取寄存器 0xF2
    cst820_read_byte(CST816S_LP_SCAN_RAW2_L_REG, data);    // 读取寄存器 0xF3
    cst820_read_byte(CST816S_LP_AUTO_WAKE_TIME_REG, data); // 读取寄存器 0xF4
    cst820_read_byte(CST816S_LP_SCAN_TH_REG, data);        // 读取寄存器 0xF5
    cst820_read_byte(CST816S_LP_SCAN_WIN_REG, data);       // 读取寄存器 0xF6
    cst820_read_byte(CST816S_LP_SCAN_FREQ_REG, data);      // 读取寄存器 0xF7
    cst820_read_byte(CST816S_LP_SCAN_IDAC_REG, data);      // 读取寄存器 0xF8
    cst820_read_byte(CST816S_AUTO_SLEEP_TIME_REG, data);   // 读取寄存器 0xF9
    cst820_read_byte(CST816S_IRQ_CTL_REG, data);           // 读取寄存器 0xFA
    cst820_read_byte(CST816S_DEBOUNCE_TIME_REG, data);     // 读取寄存器 0xFB
    cst820_read_byte(CST816S_LONG_PRESS_TIME_REG, data);   // 读取寄存器 0xFC
    cst820_read_byte(CST816S_IO_CTL_REG, data);            // 读取寄存器 0xFD
    cst820_read_byte(CST816S_DIS_AUTO_SLEEP_REG, data);    // 读取寄存器 0xFE
}