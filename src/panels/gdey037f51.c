/**
 * @file gdey037f51.c
 * @brief Driver for GDEY037F51 3.7" 4-Color E-Paper Display (240x416)
 * 
 * Colors: Black, White, Yellow, Red (BWRY)
 * Format: 2-bit per pixel (4 pixels per byte)
 * 
 * Color mapping (image format):
 *   00 = Black
 *   01 = White
 *   10 = Yellow
 *   11 = Red
 * 
 * Panel format (internal, after conversion):
 *   00 = Black
 *   01 = White
 *   10 = Yellow
 *   11 = Red
 * 
 * Note: This panel does NOT support partial refresh
 * 
 * @see https://www.good-display.com/product/505.html
 */

#include "epaper_panel.h"
#include "../epaper_spi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "gdey037f51";

#define EPD_WIDTH   240
#define EPD_HEIGHT  416

// External functions from epaper.c
extern epd_spi_t* epd_get_spi(epd_device_t *dev);
extern uint16_t epd_get_width(epd_device_t *dev);
extern uint16_t epd_get_height(epd_device_t *dev);

// Macros for panel operations
#define CMD(spi, c)     epd_spi_write_cmd(spi, c)
#define DATA(spi, d)    epd_spi_write_data(spi, d)
#define WAIT(spi)       gdey037f51_wait_busy(spi)
#define RESET(spi)      epd_spi_reset(spi)

// This panel uses normal busy signal (HIGH = ready)
static void gdey037f51_wait_busy(epd_spi_t *spi)
{
    ESP_LOGD(TAG, "Waiting for busy...");
    while (!epd_spi_is_busy(spi)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGD(TAG, "Busy released");
}

static esp_err_t gdey037f51_init(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    uint16_t w = epd_get_width(dev);
    uint16_t h = epd_get_height(dev);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    RESET(spi);
    vTaskDelay(pdMS_TO_TICKS(50));
    WAIT(spi);
    
    // PSR - Panel Setting
    CMD(spi, 0x00);
    DATA(spi, 0x0F);  // Normal scan direction
    DATA(spi, 0x29);
    
    // PWR - Power Setting
    CMD(spi, 0x01);
    DATA(spi, 0x07);
    DATA(spi, 0x00);
    DATA(spi, 0x22);  // RED
    DATA(spi, 0x78);
    DATA(spi, 0x0A);  // WHITE
    DATA(spi, 0x22);  // RED
    
    // PFS - Power off sequence setting
    CMD(spi, 0x03);
    DATA(spi, 0x10);
    DATA(spi, 0x54);
    DATA(spi, 0x44);
    
    // BTST - Booster soft start
    CMD(spi, 0x06);
    DATA(spi, 0xC0);
    DATA(spi, 0xC0);
    DATA(spi, 0xC0);
    
    // PLL - PLL control
    CMD(spi, 0x30);
    DATA(spi, 0x08);
    
    // TSE - Temperature sensor enable
    CMD(spi, 0x41);
    DATA(spi, 0x00);
    
    // CDI - VCOM and data interval setting
    CMD(spi, 0x50);
    DATA(spi, 0x37);
    
    // TCON - TCON setting
    CMD(spi, 0x60);
    DATA(spi, 0x02);
    DATA(spi, 0x02);
    
    // TRES - Resolution setting
    CMD(spi, 0x61);
    DATA(spi, w >> 8);
    DATA(spi, w & 0xFF);
    DATA(spi, h >> 8);
    DATA(spi, h & 0xFF);
    
    // FLG - Get flag status
    CMD(spi, 0x65);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    
    // PWM settings
    CMD(spi, 0xE7);
    DATA(spi, 0x1C);
    
    CMD(spi, 0xE3);
    DATA(spi, 0x22);
    
    // Enter special register mode
    CMD(spi, 0xFF);
    DATA(spi, 0xA5);
    
    CMD(spi, 0xEF);
    DATA(spi, 0x01);
    DATA(spi, 0x1E);
    DATA(spi, 0x0A);
    DATA(spi, 0x1B);
    DATA(spi, 0x0B);
    DATA(spi, 0x17);
    
    CMD(spi, 0xC3);
    DATA(spi, 0xFD);
    
    CMD(spi, 0xDC);
    DATA(spi, 0x01);
    
    CMD(spi, 0xDD);
    DATA(spi, 0x08);
    
    CMD(spi, 0xDE);
    DATA(spi, 0x41);
    
    CMD(spi, 0xFD);
    DATA(spi, 0x01);
    
    CMD(spi, 0xE8);
    DATA(spi, 0x03);
    
    CMD(spi, 0xDA);
    DATA(spi, 0x07);
    
    CMD(spi, 0xC9);
    DATA(spi, 0x00);
    
    CMD(spi, 0xA8);
    DATA(spi, 0x0F);
    
    // Exit special register mode
    CMD(spi, 0xFF);
    DATA(spi, 0xE3);
    
    CMD(spi, 0xE9);
    DATA(spi, 0x01);
    
    // Power on
    CMD(spi, 0x04);
    WAIT(spi);
    
    // Additional PWM settings after power on
    CMD(spi, 0xFF);
    DATA(spi, 0xA5);
    
    CMD(spi, 0xEF);
    DATA(spi, 0x03);
    DATA(spi, 0x1E);
    DATA(spi, 0x0A);
    DATA(spi, 0x1B);
    DATA(spi, 0x0E);
    DATA(spi, 0x15);
    
    CMD(spi, 0xDC);
    DATA(spi, 0x01);
    
    CMD(spi, 0xDD);
    DATA(spi, 0x08);
    
    CMD(spi, 0xDE);
    DATA(spi, 0x41);
    
    CMD(spi, 0xFF);
    DATA(spi, 0xE3);
    
    ESP_LOGI(TAG, "Panel initialized (%dx%d 4-color BWRY)", w, h);
    return ESP_OK;
}

static esp_err_t gdey037f51_init_180(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    uint16_t w = epd_get_width(dev);
    uint16_t h = epd_get_height(dev);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    RESET(spi);
    vTaskDelay(pdMS_TO_TICKS(50));
    WAIT(spi);
    
    // PSR - Panel Setting (180 degree rotation)
    CMD(spi, 0x00);
    DATA(spi, 0x03);  // Rotated scan direction
    DATA(spi, 0x29);
    
    // Same as normal init from here
    CMD(spi, 0x01);
    DATA(spi, 0x07);
    DATA(spi, 0x00);
    DATA(spi, 0x22);
    DATA(spi, 0x78);
    DATA(spi, 0x0A);
    DATA(spi, 0x22);
    
    CMD(spi, 0x03);
    DATA(spi, 0x10);
    DATA(spi, 0x54);
    DATA(spi, 0x44);
    
    CMD(spi, 0x06);
    DATA(spi, 0xC0);
    DATA(spi, 0xC0);
    DATA(spi, 0xC0);
    
    CMD(spi, 0x30);
    DATA(spi, 0x08);
    
    CMD(spi, 0x41);
    DATA(spi, 0x00);
    
    CMD(spi, 0x50);
    DATA(spi, 0x37);
    
    CMD(spi, 0x60);
    DATA(spi, 0x02);
    DATA(spi, 0x02);
    
    CMD(spi, 0x61);
    DATA(spi, w >> 8);
    DATA(spi, w & 0xFF);
    DATA(spi, h >> 8);
    DATA(spi, h & 0xFF);
    
    CMD(spi, 0x65);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    
    CMD(spi, 0xE7);
    DATA(spi, 0x1C);
    
    CMD(spi, 0xE3);
    DATA(spi, 0x22);
    
    CMD(spi, 0xFF);
    DATA(spi, 0xA5);
    
    CMD(spi, 0xEF);
    DATA(spi, 0x01);
    DATA(spi, 0x1E);
    DATA(spi, 0x0A);
    DATA(spi, 0x1B);
    DATA(spi, 0x0B);
    DATA(spi, 0x17);
    
    CMD(spi, 0xC3);
    DATA(spi, 0xFD);
    
    CMD(spi, 0xDC);
    DATA(spi, 0x01);
    
    CMD(spi, 0xDD);
    DATA(spi, 0x08);
    
    CMD(spi, 0xDE);
    DATA(spi, 0x41);
    
    CMD(spi, 0xFD);
    DATA(spi, 0x01);
    
    CMD(spi, 0xE8);
    DATA(spi, 0x03);
    
    CMD(spi, 0xDA);
    DATA(spi, 0x07);
    
    CMD(spi, 0xC9);
    DATA(spi, 0x00);
    
    CMD(spi, 0xA8);
    DATA(spi, 0x0F);
    
    CMD(spi, 0xFF);
    DATA(spi, 0xE3);
    
    CMD(spi, 0xE9);
    DATA(spi, 0x01);
    
    CMD(spi, 0x04);
    WAIT(spi);
    
    CMD(spi, 0xFF);
    DATA(spi, 0xA5);
    
    CMD(spi, 0xEF);
    DATA(spi, 0x03);
    DATA(spi, 0x1E);
    DATA(spi, 0x0A);
    DATA(spi, 0x1B);
    DATA(spi, 0x0E);
    DATA(spi, 0x15);
    
    CMD(spi, 0xDC);
    DATA(spi, 0x01);
    
    CMD(spi, 0xDD);
    DATA(spi, 0x08);
    
    CMD(spi, 0xDE);
    DATA(spi, 0x41);
    
    CMD(spi, 0xFF);
    DATA(spi, 0xE3);
    
    ESP_LOGI(TAG, "Panel initialized (180 degree rotation)");
    return ESP_OK;
}

static esp_err_t gdey037f51_init_fast(epd_device_t *dev)
{
    // 4-color panels don't have fast mode
    return gdey037f51_init(dev);
}

static esp_err_t gdey037f51_init_partial(epd_device_t *dev)
{
    // 4-color panels don't support partial refresh
    return gdey037f51_init(dev);
}

static esp_err_t gdey037f51_update(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x12);  // Display Refresh
    DATA(spi, 0x00);
    WAIT(spi);
    
    return ESP_OK;
}

static esp_err_t gdey037f51_update_fast(epd_device_t *dev)
{
    return gdey037f51_update(dev);
}

static esp_err_t gdey037f51_update_partial(epd_device_t *dev)
{
    return gdey037f51_update(dev);
}

/**
 * @brief Write RAM - direct format (no conversion needed)
 * 
 * LVGL/esp_epaper uses panel format directly:
 *   EPD_PIXEL_BLACK  = 0x0 -> 00 = Black
 *   EPD_PIXEL_WHITE  = 0x1 -> 01 = White
 *   EPD_PIXEL_YELLOW = 0x2 -> 10 = Yellow
 *   EPD_PIXEL_RED    = 0x3 -> 11 = Red
 * 
 * This matches the panel's native format, so no conversion is required.
 */
static esp_err_t gdey037f51_write_ram(epd_device_t *dev, const uint8_t *data, uint32_t len)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x10);  // Write RAM
    epd_spi_write_data_bulk(spi, data, len);
    
    return ESP_OK;
}

static esp_err_t gdey037f51_write_base_image(epd_device_t *dev, const uint8_t *data, uint32_t len)
{
    // 4-color panels don't have separate base image
    return gdey037f51_write_ram(dev, data, len);
}

static esp_err_t gdey037f51_write_ram_partial(epd_device_t *dev, 
                                               uint16_t x, uint16_t y,
                                               uint16_t w, uint16_t h,
                                               const uint8_t *data)
{
    // 4-color panels don't support partial write
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t gdey037f51_sleep(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x02);  // Power off
    DATA(spi, 0x00);
    WAIT(spi);
    
    CMD(spi, 0x07);  // Deep sleep
    DATA(spi, 0xA5);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "Panel entering deep sleep");
    return ESP_OK;
}

static esp_err_t gdey037f51_wake(epd_device_t *dev)
{
    return gdey037f51_init(dev);
}

// Fill functions for solid colors
static esp_err_t gdey037f51_fill_color(epd_device_t *dev, uint8_t color_byte)
{
    epd_spi_t *spi = epd_get_spi(dev);
    uint16_t w = epd_get_width(dev);
    uint16_t h = epd_get_height(dev);
    uint32_t total_bytes = (w * h) / 4;
    
    CMD(spi, 0x10);
    for (uint32_t i = 0; i < total_bytes; i++) {
        DATA(spi, color_byte);
    }
    
    return ESP_OK;
}

// Panel driver definition
const epd_panel_driver_t epd_panel_gdey037f51 = {
    .name = "GDEY037F51",
    .default_width = EPD_WIDTH,
    .default_height = EPD_HEIGHT,
    .color_mode = EPD_COLOR_4COLOR,
    
    .init = gdey037f51_init,
    .init_fast = gdey037f51_init_fast,
    .init_partial = gdey037f51_init_partial,
    .update = gdey037f51_update,
    .update_fast = gdey037f51_update_fast,
    .update_partial = gdey037f51_update_partial,
    .write_ram = gdey037f51_write_ram,
    .write_base_image = gdey037f51_write_base_image,
    .write_ram_partial = gdey037f51_write_ram_partial,
    .sleep = gdey037f51_sleep,
    .wake = gdey037f51_wake,
};

