
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "co5300_driver.h"
#include "esp_timer.h"
#include "lvgl.h"          
#include "lv_port/lv_port_disp.h"  
#include "lv_demo_benchmark.h"
#include "lv_demo_widgets.h"
#include "lv_conf.h"
#include <math.h>

#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565_SWAPPED))

static uint8_t buf1[LCD_WIDTH * LCD_HEIGHT  * BYTES_PER_PIXEL /10];

static uint32_t my_tick_get_cb(void) {
    return esp_timer_get_time() / 1000;  // 将微秒转换为毫秒
}


void my_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map)
{
    /* The most simple case (also the slowest) to send all rendered pixels to the
     * screen one-by-one.  `put_px` is just an example.  It needs to be implemented by you. */
    uint16_t * buf16 = (uint16_t *)px_map; /* Let's say it's a 16 bit (RGB565) display */
    int32_t x, y;
    // for(y = area->y1; y <= area->y2; y++) {
    //     for(x = area->x1; x <= area->x2; x++) {
    //         writePixel(x, y, *buf16);
    //         buf16++;
    //     }
    // }
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    draw16bitBeRGBBitmap(area->x1, area->y1, buf16, w, h);
    /* IMPORTANT!!!
     * Inform LVGL that flushing is complete so buffer can be modified again. */
    lv_display_flush_ready(display);
}


void my_input_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    if(cst820_read_touch(data)) {
        data->point.x = touchpad_x;
        data->point.y = touchpad_y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    return false; 
}

void lvgl_initialization(void)
{   
    lv_init();
    lv_tick_set_cb(my_tick_get_cb);
    lv_display_t * display1 = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_set_buffers(display1, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display1, my_flush_cb);

    cst820_init();                     /* 初始化触摸屏驱动 */
    lv_indev_t * indev = lv_indev_create();        /* Create input device */
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);   /* Set the device type */
    lv_indev_set_read_cb(indev, my_input_read);    /* Set the read callback */
}

void show_large_text(lv_obj_t *scr) {

        //创建一个简单的矩形，测试显示
        lv_obj_t * rect = lv_obj_create(lv_scr_act());
        lv_obj_set_size(rect, 300, 300);
        lv_obj_set_style_bg_color(rect, lv_color_hex(0xFF0000), 0);  // 红色矩形
        lv_obj_align(rect, LV_ALIGN_CENTER, 0, 0);
            
        // 创建一个简单的标签
        lv_obj_t * label = lv_label_create(lv_scr_act());
        lv_label_set_text(label, "Hello LVGL!");
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}


void app_main(void)
{  
    tra_test();
    lvgl_initialization();
    //show_large_text(lv_scr_act());          // 显示文字
    //lv_demo_benchmark();                    // 初始化 LVGL 示例
    lv_demo_widgets();
    
    while(1)
    {
        lv_task_handler();  // LVGL 任务管理
        vTaskDelay(pdMS_TO_TICKS(10));  // 延迟 10ms，确保看门狗正常工作
    }
}
