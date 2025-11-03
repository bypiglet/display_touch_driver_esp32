#include "cst820.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "CST820";
static i2c_port_t i2c_port = I2C_NUM_0;


/** I2C初始化 */
static void cst820_i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = IIC_SDA,
        .scl_io_num = IIC_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(i2c_port, &conf);
    i2c_driver_install(i2c_port, conf.mode, 0, 0, 0);

    ESP_LOGI(TAG, "I2C initialized: SDA=%d SCL=%d", IIC_SDA, IIC_SCL);
}

static esp_err_t cst820_write_byte(uint8_t reg, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (CST820_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(20));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t cst820_read_byte(uint8_t reg, uint8_t *data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (CST820_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (CST820_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(20));
    i2c_cmd_link_delete(cmd);
    return ret;
}

bool cst820_init(void)
{
    cst820_i2c_init();

    gpio_set_direction(TP_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(TP_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TP_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(200));

    uint8_t t;
    if (cst820_read_byte(CST820_REG_GESTURE, &t) != ESP_OK) {
        ESP_LOGE(TAG, "Touch not detected!");
        return false;
    }

    ESP_LOGI(TAG, "CST820 Ready ✅");
    return true;
}

bool cst820_read_touch(cst820_info_t *info)
{
    uint8_t buf[6];
    for(int i=0;i<6;i++){
        if(cst820_read_byte(CST820_REG_GESTURE + i, &buf[i]) != ESP_OK)
            return false;
    }

    info->gesture = buf[0];
    info->finger  = buf[1] & 0x0F;
    if (info->finger == 0) return false;

    info->x = ((buf[2] & 0x0F) << 8) | buf[3];
    info->y = ((buf[4] & 0x0F) << 8) | buf[5];

    return true;
}
