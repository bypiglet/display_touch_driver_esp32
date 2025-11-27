#include "cst820.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DATA_LENGTH 100

static const char *TAG = "CST820";
static i2c_port_t I2C_PORT = I2C_NUM_0;

static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;

/** I2C初始化 */
static esp_err_t cst820_i2c_init(void)
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

    return ESP_OK;
}

//从设备读取写入1字节
static esp_err_t cst820_write_byte(uint8_t reg, uint8_t data)
{
    uint8_t data_wr[DATA_LENGTH]={0};
    i2c_master_bus_config_t i2c_mst_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = I2C_PORT,
    .scl_io_num = IIC_SCL,
    .sda_io_num = IIC_SDA,
    .glitch_ignore_cnt = 7,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x2A,
        .scl_speed_hz = 100000,
    };
    data_wr[0] = reg;
    data_wr[1] = data;
    i2c_master_dev_handle_t dev_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, data_wr, DATA_LENGTH, -1));
}   

//从设备读取1字节
uint8_t* cst820_read_byte(uint8_t reg, uint8_t *data_rd)
{
    i2c_master_bus_config_t i2c_mst_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = I2C_PORT,
    .scl_io_num = IIC_SCL,
    .sda_io_num = IIC_SDA,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,   
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));
    i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x15,
    .scl_speed_hz = 100000,
    };

    i2c_master_dev_handle_t dev_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));
    uint8_t reg_addr[1] = {reg};  // 你要读取的寄存器地址
    memset(&data_rd[0],0,1);
    i2c_master_transmit(dev_handle,reg_addr,1,1000 / portTICK_PERIOD_MS);
    esp_err_t ret = i2c_master_receive(dev_handle,data_rd,5,1000 / portTICK_PERIOD_MS);
    printf("寄存器地址:0X%02X:0X%02X ", reg,data_rd[0]);
    i2c_master_bus_rm_device(dev_handle);
    i2c_del_master_bus(bus_handle);
    return data_rd;
}



//从设备读取多字节
static esp_err_t cst820_read_bytes(uint8_t reg, uint8_t *data_rd, size_t len)
{
    i2c_master_bus_config_t i2c_mst_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = I2C_PORT,
    .scl_io_num = IIC_SCL,
    .sda_io_num = IIC_SDA,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,   
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));
    i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x15,
    .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t dev_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));
    uint8_t reg_addr[1] = {reg};  // 你要读取的寄存器地址
    memset(&data_rd[0],0,len);
    i2c_master_transmit(dev_handle,reg_addr,len,1000 / portTICK_PERIOD_MS);
    esp_err_t ret = i2c_master_receive(dev_handle,data_rd,5,1000 / portTICK_PERIOD_MS);
    printf("寄存器地址:0X%02X:0X%02X ", reg,data_rd[0]);
    printf("寄存器地址:0X%02X:0X%02X ", reg,data_rd[1]);
    printf("寄存器地址:0X%02X:0X%02X ", reg,data_rd[2]);
    printf("寄存器地址:0X%02X:0X%02X ", reg,data_rd[3]);
    i2c_master_bus_rm_device(dev_handle);
    i2c_del_master_bus(bus_handle);
    //return data_rd;
}

bool cst820_read_touch(cst820_info_t *info)
{
    uint8_t buf[4] = {0};
    // for(int i=0;i<6;i++){
    //     if(cst820_read_byte(CST820_REG_GESTURE + i, &buf[i]) != ESP_OK)
    //         return false;
    // }
    cst820_read_byte(0x03, &buf[0]);
    info->Xpos =  ((buf[0] & 0xF0)<<4 | buf[1]);
    info->Ypos =   ((buf[2] & 0xF0)<<4 | buf[3]);
    return true;
}

// void read_allreg()
// {
//     uint8_t data[1] ={0};
//     cst820_read_byte(CST816S_GESTURE_ID_REG, data);        // 读取寄存器 0x01
//     cst820_read_byte(CST816S_FINGER_NUM_REG, data);        // 读取寄存器 0x02
//     cst820_read_byte(CST816S_XPOS_H_REG, data);            // 读取寄存器 0x03
//     cst820_read_byte(CST816S_XPOS_L_REG, data);            // 读取寄存器 0x04
//     cst820_read_byte(CST816S_YPOS_H_REG, data);            // 读取寄存器 0x05
//     cst820_read_byte(CST816S_YPOS_L_REG, data);            // 读取寄存器 0x06
//     cst820_read_byte(CST816S_BPC0_H_REG, data);            // 读取寄存器 0xB0
//     cst820_read_byte(CST816S_BPC0_L_REG, data);            // 读取寄存器 0xB1
//     cst820_read_byte(CST816S_BPC1_H_REG, data);            // 读取寄存器 0xB2
//     cst820_read_byte(CST816S_BPC1_L_REG, data);            // 读取寄存器 0xB3
//     cst820_read_byte(CST816S_CHIP_ID_REG, data);           // 读取寄存器 0xA7
//     cst820_read_byte(CST816S_PROJ_ID_REG, data);           // 读取寄存器 0xA8
//     cst820_read_byte(CST816S_FW_VERSION_REG, data);        // 读取寄存器 0xA9
//     cst820_read_byte(CST816S_MOTION_MASK_REG, data);       // 读取寄存器 0xEC
//     cst820_read_byte(CST816S_IRQ_PULSE_WIDTH_REG, data);   // 读取寄存器 0xED
//     cst820_read_byte(CST816S_NOR_SCAN_PER_REG, data);      // 读取寄存器 0xEE
//     cst820_read_byte(CST816S_MOTION_SL_ANGLE_REG, data);  // 读取寄存器 0xEF
//     cst820_read_byte(CST816S_LP_SCAN_RAW1_H_REG, data);    // 读取寄存器 0xF0
//     cst820_read_byte(CST816S_LP_SCAN_RAW1_L_REG, data);    // 读取寄存器 0xF1
//     cst820_read_byte(CST816S_LP_SCAN_RAW2_H_REG, data);    // 读取寄存器 0xF2
//     cst820_read_byte(CST816S_LP_SCAN_RAW2_L_REG, data);    // 读取寄存器 0xF3
//     cst820_read_byte(CST816S_LP_AUTO_WAKE_TIME_REG, data); // 读取寄存器 0xF4
//     cst820_read_byte(CST816S_LP_SCAN_TH_REG, data);        // 读取寄存器 0xF5
//     cst820_read_byte(CST816S_LP_SCAN_WIN_REG, data);       // 读取寄存器 0xF6
//     cst820_read_byte(CST816S_LP_SCAN_FREQ_REG, data);      // 读取寄存器 0xF7
//     cst820_read_byte(CST816S_LP_SCAN_IDAC_REG, data);      // 读取寄存器 0xF8
//     cst820_read_byte(CST816S_AUTO_SLEEP_TIME_REG, data);   // 读取寄存器 0xF9
//     cst820_read_byte(CST816S_IRQ_CTL_REG, data);           // 读取寄存器 0xFA
//     cst820_read_byte(CST816S_DEBOUNCE_TIME_REG, data);     // 读取寄存器 0xFB
//     cst820_read_byte(CST816S_LONG_PRESS_TIME_REG, data);   // 读取寄存器 0xFC
//     cst820_read_byte(CST816S_IO_CTL_REG, data);            // 读取寄存器 0xFD
//     cst820_read_byte(CST816S_DIS_AUTO_SLEEP_REG, data);    // 读取寄存器 0xFE
// }