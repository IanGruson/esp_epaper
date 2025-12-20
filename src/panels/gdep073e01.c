/**
 * @file gdep073e01.c
 * @brief Driver for 7.3" 6-Color E-Paper Display (800x480)
 * 
 * Colors: Black, White, Yellow, Red, Blue, Green
 * Format: 4-bit per pixel (2 pixels per byte)
 */

#include "epaper_panel.h"
#include "../epaper_spi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "gdep073e01";

#define EPD_WIDTH   800
#define EPD_HEIGHT  480

// External functions from epaper.c
extern epd_spi_t* epd_get_spi(epd_device_t *dev);
extern uint16_t epd_get_width(epd_device_t *dev);
extern uint16_t epd_get_height(epd_device_t *dev);

// Macros for panel operations
#define CMD(spi, c)     epd_spi_write_cmd(spi, c)
#define DATA(spi, d)    epd_spi_write_data(spi, d)
#define WAIT(spi)       gdep073e01_wait_busy(spi)
#define RESET(spi)      epd_spi_reset(spi)

// Note: This panel uses inverted busy signal (HIGH = ready, LOW = busy)
static void gdep073e01_wait_busy(epd_spi_t *spi)
{
    ESP_LOGD(TAG, "Waiting for busy...");
    while (!epd_spi_is_busy(spi)) {  // Wait while LOW (busy)
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGD(TAG, "Busy released");
}

static esp_err_t gdep073e01_init(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    RESET(spi);
    vTaskDelay(pdMS_TO_TICKS(50));
    WAIT(spi);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Command sequence for 7.3" 6-color e-paper
    CMD(spi, 0xAA);  // CMDH
    DATA(spi, 0x49);
    DATA(spi, 0x55);
    DATA(spi, 0x20);
    DATA(spi, 0x08);
    DATA(spi, 0x09);
    DATA(spi, 0x18);
    
    CMD(spi, 0x01);
    DATA(spi, 0x3F);
    
    CMD(spi, 0x00);
    DATA(spi, 0x5F);
    DATA(spi, 0x69);
    
    CMD(spi, 0x03);
    DATA(spi, 0x00);
    DATA(spi, 0x54);
    DATA(spi, 0x00);
    DATA(spi, 0x44);
    
    CMD(spi, 0x05);
    DATA(spi, 0x40);
    DATA(spi, 0x1F);
    DATA(spi, 0x1F);
    DATA(spi, 0x2C);
    
    CMD(spi, 0x06);
    DATA(spi, 0x6F);
    DATA(spi, 0x1F);
    DATA(spi, 0x17);
    DATA(spi, 0x49);
    
    CMD(spi, 0x08);
    DATA(spi, 0x6F);
    DATA(spi, 0x1F);
    DATA(spi, 0x1F);
    DATA(spi, 0x22);
    
    CMD(spi, 0x30);
    DATA(spi, 0x03);
    
    CMD(spi, 0x50);
    DATA(spi, 0x3F);
    
    CMD(spi, 0x60);
    DATA(spi, 0x02);
    DATA(spi, 0x00);
    
    // Resolution: 800x480
    CMD(spi, 0x61);
    DATA(spi, (EPD_WIDTH >> 8) & 0xFF);   // 0x03
    DATA(spi, EPD_WIDTH & 0xFF);          // 0x20
    DATA(spi, (EPD_HEIGHT >> 8) & 0xFF);  // 0x01
    DATA(spi, EPD_HEIGHT & 0xFF);         // 0xE0
    
    CMD(spi, 0x84);
    DATA(spi, 0x01);
    
    CMD(spi, 0xE3);
    DATA(spi, 0x2F);
    
    CMD(spi, 0x04);  // Power on
    WAIT(spi);
    
    ESP_LOGI(TAG, "Panel initialized (800x480 6-color)");
    return ESP_OK;
}

static esp_err_t gdep073e01_init_fast(epd_device_t *dev)
{
    // 6-color panels don't have fast mode
    return gdep073e01_init(dev);
}

static esp_err_t gdep073e01_init_partial(epd_device_t *dev)
{
    // 6-color panels typically don't support partial refresh
    return gdep073e01_init(dev);
}

static esp_err_t gdep073e01_update(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x04);  // POWER_ON
    WAIT(spi);
    
    CMD(spi, 0x06);
    DATA(spi, 0x6F);
    DATA(spi, 0x1F);
    DATA(spi, 0x17);
    DATA(spi, 0x49);
    
    CMD(spi, 0x12);  // DISPLAY_REFRESH
    DATA(spi, 0x00);
    WAIT(spi);
    
    CMD(spi, 0x02);  // POWER_OFF
    DATA(spi, 0x00);
    WAIT(spi);
    
    return ESP_OK;
}

static esp_err_t gdep073e01_update_fast(epd_device_t *dev)
{
    return gdep073e01_update(dev);
}

static esp_err_t gdep073e01_update_partial(epd_device_t *dev)
{
    return gdep073e01_update(dev);
}

static esp_err_t gdep073e01_write_ram(epd_device_t *dev, const uint8_t *data, uint32_t len)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x10);  // Write RAM
    epd_spi_write_data_bulk(spi, data, len);
    
    return ESP_OK;
}

static esp_err_t gdep073e01_write_base_image(epd_device_t *dev, const uint8_t *data, uint32_t len)
{
    // 6-color panels don't have separate base image register
    return gdep073e01_write_ram(dev, data, len);
}

static esp_err_t gdep073e01_write_ram_partial(epd_device_t *dev, 
                                               uint16_t x, uint16_t y,
                                               uint16_t w, uint16_t h,
                                               const uint8_t *data)
{
    // 6-color panels typically don't support windowed write
    // Fall back to full RAM write
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t gdep073e01_sleep(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x07);  // DEEP_SLEEP
    DATA(spi, 0xA5);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "Panel entering deep sleep");
    return ESP_OK;
}

static esp_err_t gdep073e01_wake(epd_device_t *dev)
{
    return gdep073e01_init(dev);
}

// Panel driver definition
const epd_panel_driver_t epd_panel_gdep073e01 = {
    .name = "GDEP073E01",
    .default_width = EPD_WIDTH,
    .default_height = EPD_HEIGHT,
    .color_mode = EPD_COLOR_6COLOR,
    
    .init = gdep073e01_init,
    .init_fast = gdep073e01_init_fast,
    .init_partial = gdep073e01_init_partial,
    .update = gdep073e01_update,
    .update_fast = gdep073e01_update_fast,
    .update_partial = gdep073e01_update_partial,
    .write_ram = gdep073e01_write_ram,
    .write_base_image = gdep073e01_write_base_image,
    .write_ram_partial = gdep073e01_write_ram_partial,
    .sleep = gdep073e01_sleep,
    .wake = gdep073e01_wake,
};
