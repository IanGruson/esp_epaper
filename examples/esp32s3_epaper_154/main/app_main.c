/**
 * @file app_main.c
 * @brief Example for Waveshare ESP32-S3-ePaper-1.54
 * 
 * Board: Waveshare ESP32-S3-ePaper-1.54
 * Panel: GDEY0154D67 (200x200 Black/White)
 * Features: Partial refresh support
 * 
 * @see https://www.waveshare.com/wiki/ESP32-S3-ePaper-1.54
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "epaper.h"
#include "epaper_lvgl.h"
#include "lvgl.h"

static const char *TAG = "epaper_154";

// Board power pin (ESP32-S3-ePaper-1.54 specific)
#define EPD_PWR_PIN     GPIO_NUM_6   // E-Paper power enable (LOW = ON)

// LVGL configuration
#define LVGL_TICK_PERIOD_MS     5
#define LVGL_TASK_MAX_DELAY_MS  500
#define LVGL_TASK_MIN_DELAY_MS  100

static lv_obj_t *counter_label = NULL;
static int counter = 0;
static SemaphoreHandle_t lvgl_mux = NULL;

/**
 * @brief Initialize board power for ESP32-S3-ePaper-1.54
 * 
 * The board requires GPIO6 to be LOW to enable e-paper power
 */
static void board_power_init(void)
{
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << EPD_PWR_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&gpio_conf);
    gpio_set_level(EPD_PWR_PIN, 0);  // LOW = Power ON
    ESP_LOGI(TAG, "E-Paper power ON (GPIO %d = LOW)", EPD_PWR_PIN);
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void lv_tick_timer_cb(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static bool lvgl_lock(int timeout_ms)
{
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

static void lvgl_unlock(void)
{
    xSemaphoreGive(lvgl_mux);
}

static void lv_handler_task(void *arg)
{
    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    while (1) {
        if (lvgl_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            lvgl_unlock();
        }
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

/**
 * @brief Create demo UI for 200x200 display
 */
static void create_demo_ui(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    
    // Title
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "E-Paper 1.54\"");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Board info
    lv_obj_t *info = lv_label_create(scr);
    lv_label_set_text(info, "ESP32-S3-ePaper\n200x200 BW");
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 35);
    
    // Separator line
    static lv_point_precise_t line_points[] = {{20, 75}, {180, 75}};
    lv_obj_t *line = lv_line_create(scr);
    lv_line_set_points(line, line_points, 2);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_color(line, lv_color_black(), 0);
    
    // Counter with border
    lv_obj_t *box = lv_obj_create(scr);
    lv_obj_set_size(box, 120, 40);
    lv_obj_align(box, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_border_color(box, lv_color_black(), 0);
    lv_obj_set_style_radius(box, 5, 0);
    
    counter_label = lv_label_create(scr);
    lv_label_set_text(counter_label, "Count: 0");
    lv_obj_align(counter_label, LV_ALIGN_CENTER, 0, 10);
    
    // Footer
    lv_obj_t *footer = lv_label_create(scr);
    lv_label_set_text(footer, "Partial Refresh");
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Waveshare ESP32-S3-ePaper-1.54 Demo ===");
    ESP_LOGI(TAG, "Panel: GDEY0154D67 (200x200 BW)");
    
    // Initialize board power
    board_power_init();
    
    // Initialize LVGL
    lv_init();
    
    // Configure e-paper with preset for ESP32-S3-ePaper-1.54
    epd_config_t epd_cfg = EPD_CONFIG_ESP32S3_154();
    epd_handle_t epd = NULL;
    
    esp_err_t ret = epd_init(&epd_cfg, &epd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init e-paper: %s", esp_err_to_name(ret));
        return;
    }
    
    // Get panel info
    epd_panel_info_t panel_info;
    epd_get_info(epd, &panel_info);
    ESP_LOGI(TAG, "Panel: %dx%d, buffer: %lu bytes", 
             panel_info.width, panel_info.height, panel_info.buffer_size);
    
    // Initialize LVGL display with partial refresh
    epd_lvgl_config_t lvgl_cfg = EPD_LVGL_CONFIG_DEFAULT();
    lvgl_cfg.epd = epd;
    lvgl_cfg.update_mode = EPD_UPDATE_PARTIAL;
    lvgl_cfg.use_partial_refresh = true;
    lvgl_cfg.partial_threshold = 2000;  // Force full refresh every N partial updates
    
    lv_display_t *disp = epd_lvgl_init(&lvgl_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "Failed to init LVGL display");
        epd_deinit(epd);
        return;
    }
    
    // Setup LVGL tick timer
    esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lv_tick_timer_cb,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));
    
    // Create LVGL task
    lvgl_mux = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(lv_handler_task, "LVGL", 8 * 1024, NULL, 4, NULL, 1);
    
    // Create UI
    if (lvgl_lock(-1)) {
        create_demo_ui();
        lvgl_unlock();
    }
    
    ESP_LOGI(TAG, "Demo started - counter updates every 10 seconds");
    
    // Main loop - update counter periodically
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        counter++;
        ESP_LOGI(TAG, "Update counter: %d", counter);
        
        if (lvgl_lock(100)) {
            if (counter_label) {
                char buf[32];
                snprintf(buf, sizeof(buf), "Count: %d", counter);
                lv_label_set_text(counter_label, buf);
            }
            
            // Force full refresh every 5 updates to clear ghosting
            if (counter % 5 == 0) {
                ESP_LOGI(TAG, "Full refresh to clear ghosting");
                epd_lvgl_force_full_refresh(disp);
            }
            
            epd_lvgl_refresh(disp);
            lvgl_unlock();
        }
    }
}
