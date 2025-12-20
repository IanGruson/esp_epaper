/**
 * @file ssd16xx_generic.c
 * @brief Generic driver for SSD1680/SSD1681 based B/W e-paper displays
 * 
 * Supports many panels with same controller but different resolutions:
 * - GDEY0154D67 (200x200)
 * - GDEY0213B74 (122x250)
 * - GDEY0266T90 (152x296)
 * - GDEY027T91 (176x264)
 * - GDEY029T94 (128x296)
 * - GDEY042T81 (400x300)
 * - And more...
 * 
 * To add a new panel, just add entry in epaper_config.h with correct dimensions.
 */

#include "epaper_panel.h"
#include "../epaper_spi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ssd16xx";

// Forward declarations
struct epd_device;
typedef struct epd_device epd_device_t;

extern epd_spi_t* epd_get_spi(epd_device_t *dev);
extern uint16_t epd_get_width(epd_device_t *dev);
extern uint16_t epd_get_height(epd_device_t *dev);

// Helper macros
#define CMD(spi, c)    epd_spi_write_cmd(spi, c)
#define DATA(spi, d)   epd_spi_write_data(spi, d)
#define WAIT(spi)      epd_spi_wait_busy(spi, 0)
#define RESET(spi)     epd_spi_reset(spi)

/*=============================================================================
 * Generic SSD1680/SSD1681 Initialization
 * Works for most BW panels with this controller family
 *============================================================================*/

static esp_err_t ssd16xx_init(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    uint16_t w = epd_get_width(dev);
    uint16_t h = epd_get_height(dev);
    
    ESP_LOGI(TAG, "Init SSD16xx panel: %dx%d", w, h);
    
    RESET(spi);
    vTaskDelay(pdMS_TO_TICKS(10));
    WAIT(spi);
    
    CMD(spi, 0x12);  // SWRESET
    WAIT(spi);
    
    CMD(spi, 0x01);  // Driver output control
    DATA(spi, (h - 1) & 0xFF);
    DATA(spi, (h - 1) >> 8);
    DATA(spi, 0x00);
    
    CMD(spi, 0x11);  // Data entry mode
    DATA(spi, 0x03);  // Y increment, X increment (normal orientation)
    
    CMD(spi, 0x44);  // Set RAM X start/end
    DATA(spi, 0x00);
    DATA(spi, (w / 8) - 1);
    
    CMD(spi, 0x45);  // Set RAM Y start/end
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    DATA(spi, (h - 1) & 0xFF);
    DATA(spi, (h - 1) >> 8);
    
    CMD(spi, 0x3C);  // Border waveform
    DATA(spi, 0x05);
    
    CMD(spi, 0x21);  // Display update control
    DATA(spi, 0x00);
    DATA(spi, 0x80);
    
    CMD(spi, 0x18);  // Temperature sensor
    DATA(spi, 0x80);  // Internal sensor
    
    CMD(spi, 0x4E);  // Set RAM X counter
    DATA(spi, 0x00);
    
    CMD(spi, 0x4F);  // Set RAM Y counter
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    
    WAIT(spi);
    
    ESP_LOGI(TAG, "Panel initialized");
    return ESP_OK;
}

static esp_err_t ssd16xx_init_fast(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    RESET(spi);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    CMD(spi, 0x12);  // SWRESET
    WAIT(spi);
    
    CMD(spi, 0x18);  // Temperature sensor
    DATA(spi, 0x80);
    
    CMD(spi, 0x22);  // Load temperature
    DATA(spi, 0xB1);
    CMD(spi, 0x20);
    WAIT(spi);
    
    CMD(spi, 0x1A);  // Write temperature register
    DATA(spi, 0x64);  // 100°C for faster refresh
    DATA(spi, 0x00);
    
    CMD(spi, 0x22);  // Load temperature
    DATA(spi, 0x91);
    CMD(spi, 0x20);
    WAIT(spi);
    
    return ESP_OK;
}

static esp_err_t ssd16xx_init_partial(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x3C);  // Border waveform
    DATA(spi, 0x80);  // Different for partial
    
    return ESP_OK;
}

/*=============================================================================
 * Display Update Functions
 *============================================================================*/

static esp_err_t ssd16xx_update(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x22);  // Display update control
    DATA(spi, 0xF7);  // Full update sequence
    CMD(spi, 0x20);  // Master activation
    WAIT(spi);
    
    return ESP_OK;
}

static esp_err_t ssd16xx_update_fast(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x22);
    DATA(spi, 0xC7);  // Fast update sequence
    CMD(spi, 0x20);
    WAIT(spi);
    
    return ESP_OK;
}

static esp_err_t ssd16xx_update_partial(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x22);
    DATA(spi, 0xFF);  // Partial update sequence
    CMD(spi, 0x20);
    WAIT(spi);
    
    return ESP_OK;
}

/*=============================================================================
 * RAM Write Functions
 *============================================================================*/

static esp_err_t ssd16xx_write_ram(epd_device_t *dev, const uint8_t *data, uint32_t len)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    // Reset RAM address to (0, 0)
    CMD(spi, 0x4E);
    DATA(spi, 0x00);
    CMD(spi, 0x4F);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    
    // Write RAM
    CMD(spi, 0x24);
    epd_spi_write_data_bulk(spi, data, len);
    
    return ESP_OK;
}

static esp_err_t ssd16xx_write_base_image(epd_device_t *dev, const uint8_t *data, uint32_t len)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    // Write to current RAM (0x24)
    CMD(spi, 0x4E);
    DATA(spi, 0x00);
    CMD(spi, 0x4F);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    
    CMD(spi, 0x24);
    epd_spi_write_data_bulk(spi, data, len);
    
    // Write to previous RAM (0x26) for partial refresh base
    CMD(spi, 0x4E);
    DATA(spi, 0x00);
    CMD(spi, 0x4F);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    
    CMD(spi, 0x26);
    epd_spi_write_data_bulk(spi, data, len);
    
    return ESP_OK;
}

/*=============================================================================
 * Power Management
 *============================================================================*/

static esp_err_t ssd16xx_sleep(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x10);  // Deep sleep
    DATA(spi, 0x01);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "Panel in deep sleep");
    return ESP_OK;
}

static esp_err_t ssd16xx_wake(epd_device_t *dev)
{
    return ssd16xx_init(dev);
}

/*=============================================================================
 * Driver Registration
 * 
 * This single driver can be used for multiple panel types.
 * Just set the correct default_width and default_height for each panel.
 * 
 * To add a new panel size:
 * 1. Copy one of the structs below
 * 2. Change name, default_width, default_height
 * 3. Add enum in epaper_config.h
 * 4. Add case in epaper.c epd_get_panel_driver()
 * 5. Add extern declaration in epaper_panel.h
 *============================================================================*/

#define SSD16XX_DRIVER_COMMON \
    .color_mode = EPD_COLOR_BW, \
    .init = ssd16xx_init, \
    .init_fast = ssd16xx_init_fast, \
    .init_partial = ssd16xx_init_partial, \
    .update = ssd16xx_update, \
    .update_fast = ssd16xx_update_fast, \
    .update_partial = ssd16xx_update_partial, \
    .write_ram = ssd16xx_write_ram, \
    .write_base_image = ssd16xx_write_base_image, \
    .sleep = ssd16xx_sleep, \
    .wake = ssd16xx_wake

// 1.54" 200x200 (GDEY0154D67 generic version - no custom LUT)
const epd_panel_driver_t epd_panel_ssd16xx_154 = {
    .name = "SSD16xx_154",
    .default_width = 200,
    .default_height = 200,
    SSD16XX_DRIVER_COMMON,
};

// 2.13" 122x250 (GDEY0213B74, GDEM0213I61, etc.)
const epd_panel_driver_t epd_panel_ssd16xx_213 = {
    .name = "SSD16xx_213",
    .default_width = 122,
    .default_height = 250,
    SSD16XX_DRIVER_COMMON,
};

// 2.66" 152x296 (GDEY0266T90, etc.)
const epd_panel_driver_t epd_panel_ssd16xx_266 = {
    .name = "SSD16xx_266",
    .default_width = 152,
    .default_height = 296,
    SSD16XX_DRIVER_COMMON,
};

// 2.7" 176x264 (GDEY027T91, GDEM027Q72, etc.)
const epd_panel_driver_t epd_panel_ssd16xx_270 = {
    .name = "SSD16xx_270",
    .default_width = 176,
    .default_height = 264,
    SSD16XX_DRIVER_COMMON,
};

// 2.9" 128x296 (GDEY029T94, GDEY029T71H, etc.)
const epd_panel_driver_t epd_panel_ssd16xx_290 = {
    .name = "SSD16xx_290",
    .default_width = 128,
    .default_height = 296,
    SSD16XX_DRIVER_COMMON,
};

// 3.7" 280x480 (GDEY037T03, etc.)
const epd_panel_driver_t epd_panel_ssd16xx_370 = {
    .name = "SSD16xx_370",
    .default_width = 280,
    .default_height = 480,
    SSD16XX_DRIVER_COMMON,
};

// 4.2" 400x300 (GDEY042T81, GDEQ0426T82, etc.)
const epd_panel_driver_t epd_panel_ssd16xx_420 = {
    .name = "SSD16xx_420",
    .default_width = 400,
    .default_height = 300,
    SSD16XX_DRIVER_COMMON,
};
