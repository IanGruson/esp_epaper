/**
 * @file gdey0154d67.c
 * @brief Driver for GDEY0154D67 1.54" B/W e-paper display (200x200)
 */

#include "epaper_panel.h"
#include "../epaper_spi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

// Panel dimensions
#define EPD_WIDTH   200
#define EPD_HEIGHT  200

static const char *TAG = "gdey0154";

// Forward declarations
struct epd_device;
typedef struct epd_device epd_device_t;

extern epd_spi_t* epd_get_spi(epd_device_t *dev);
extern uint16_t epd_get_width(epd_device_t *dev);
extern uint16_t epd_get_height(epd_device_t *dev);

// Waveform LUT for full refresh
static const uint8_t WF_FULL_1IN54[159] = {
    0x80, 0x48, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x40, 0x48, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x80, 0x48, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x40, 0x48, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0xA, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x8, 0x1, 0x0, 0x8, 0x1, 0x0, 0x2,
    0xA, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x0, 0x0, 0x0,
    0x22, 0x17, 0x41, 0x0, 0x32, 0x20
};

// Waveform LUT for partial refresh
static const uint8_t WF_PARTIAL_1IN54[159] = {
    0x0, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x80, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x40, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x0, 0x0, 0x0,
    0x02, 0x17, 0x41, 0xB0, 0x32, 0x28,
};

// Helper macros
#define CMD(spi, c)    epd_spi_write_cmd(spi, c)
#define DATA(spi, d)   epd_spi_write_data(spi, d)
#define WAIT(spi)      epd_spi_wait_busy(spi, 0)
#define RESET(spi)     epd_spi_reset(spi)

static void set_lut(epd_spi_t *spi, const uint8_t *lut)
{
    CMD(spi, 0x32);
    epd_spi_write_data_bulk(spi, lut, 153);
    WAIT(spi);
    
    CMD(spi, 0x3F);
    DATA(spi, lut[153]);
    
    CMD(spi, 0x03);
    DATA(spi, lut[154]);
    
    CMD(spi, 0x04);
    DATA(spi, lut[155]);
    DATA(spi, lut[156]);
    DATA(spi, lut[157]);
    
    CMD(spi, 0x2C);
    DATA(spi, lut[158]);
}

static void set_windows(epd_spi_t *spi, uint16_t x_start, uint16_t y_start, 
                        uint16_t x_end, uint16_t y_end)
{
    CMD(spi, 0x44);  // SET_RAM_X_ADDRESS
    DATA(spi, (x_start >> 3) & 0xFF);
    DATA(spi, (x_end >> 3) & 0xFF);
    
    CMD(spi, 0x45);  // SET_RAM_Y_ADDRESS
    DATA(spi, y_start & 0xFF);
    DATA(spi, (y_start >> 8) & 0xFF);
    DATA(spi, y_end & 0xFF);
    DATA(spi, (y_end >> 8) & 0xFF);
}

static void set_cursor(epd_spi_t *spi, uint16_t x, uint16_t y)
{
    CMD(spi, 0x4E);
    DATA(spi, x & 0xFF);
    
    CMD(spi, 0x4F);
    DATA(spi, y & 0xFF);
    DATA(spi, (y >> 8) & 0xFF);
}

static esp_err_t gdey0154d67_init(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    uint16_t w = epd_get_width(dev);
    uint16_t h = epd_get_height(dev);
    
    RESET(spi);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    WAIT(spi);
    CMD(spi, 0x12);  // SWRESET
    WAIT(spi);
    
    CMD(spi, 0x01);  // Driver output control
    DATA(spi, 0xC7);  // (200-1) & 0xFF
    DATA(spi, 0x00);  // (200-1) >> 8
    DATA(spi, 0x01);
    
    CMD(spi, 0x11);  // Data entry mode
    DATA(spi, 0x01);
    
    set_windows(spi, 0, w - 1, h - 1, 0);
    
    CMD(spi, 0x3C);  // Border waveform
    DATA(spi, 0x01);
    
    CMD(spi, 0x18);  // Temperature sensor
    DATA(spi, 0x80);
    
    CMD(spi, 0x22);  // Load temperature
    DATA(spi, 0xB1);
    CMD(spi, 0x20);
    
    set_cursor(spi, 0, h - 1);
    WAIT(spi);
    
    set_lut(spi, WF_FULL_1IN54);
    
    ESP_LOGD(TAG, "Init complete");
    return ESP_OK;
}

static esp_err_t gdey0154d67_init_fast(epd_device_t *dev)
{
    // Fast init uses same as full init for this panel
    return gdey0154d67_init(dev);
}

static esp_err_t gdey0154d67_init_partial(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    RESET(spi);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    WAIT(spi);
    
    set_lut(spi, WF_PARTIAL_1IN54);
    
    CMD(spi, 0x37);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    DATA(spi, 0x40);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    
    CMD(spi, 0x3C);  // Border waveform
    DATA(spi, 0x80);
    
    CMD(spi, 0x22);
    DATA(spi, 0xC0);
    CMD(spi, 0x20);
    WAIT(spi);
    
    ESP_LOGD(TAG, "Partial init complete");
    return ESP_OK;
}

static esp_err_t gdey0154d67_update(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x22);
    DATA(spi, 0xC7);
    CMD(spi, 0x20);
    WAIT(spi);
    
    return ESP_OK;
}

static esp_err_t gdey0154d67_update_fast(epd_device_t *dev)
{
    return gdey0154d67_update(dev);
}

static esp_err_t gdey0154d67_update_partial(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x22);
    DATA(spi, 0xCF);
    CMD(spi, 0x20);
    WAIT(spi);
    
    return ESP_OK;
}

static esp_err_t gdey0154d67_write_ram(epd_device_t *dev, const uint8_t *data, uint32_t len)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x24);
    epd_spi_write_data_bulk(spi, data, len);
    
    return ESP_OK;
}

// Write base image for partial refresh (both 0x24 and 0x26)
static esp_err_t gdey0154d67_write_base_image(epd_device_t *dev, const uint8_t *data, uint32_t len)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x24);
    epd_spi_write_data_bulk(spi, data, len);
    
    CMD(spi, 0x26);  // Base image register
    epd_spi_write_data_bulk(spi, data, len);
    
    return ESP_OK;
}

static esp_err_t gdey0154d67_write_ram_partial(epd_device_t *dev, 
                                                uint16_t x, uint16_t y,
                                                uint16_t w, uint16_t h,
                                                const uint8_t *data)
{
    epd_spi_t *spi = epd_get_spi(dev);
    uint16_t panel_h = epd_get_height(dev);
    
    uint16_t x_start = x / 8;
    uint16_t x_end = x_start + w / 8 - 1;
    uint16_t y_start = panel_h - 1 - y;
    uint16_t y_end = y_start - h + 1;
    
    CMD(spi, 0x44);
    DATA(spi, x_start);
    DATA(spi, x_end);
    
    CMD(spi, 0x45);
    DATA(spi, y_start & 0xFF);
    DATA(spi, (y_start >> 8) & 0xFF);
    DATA(spi, y_end & 0xFF);
    DATA(spi, (y_end >> 8) & 0xFF);
    
    CMD(spi, 0x4E);
    DATA(spi, x_start);
    
    CMD(spi, 0x4F);
    DATA(spi, y_start & 0xFF);
    DATA(spi, (y_start >> 8) & 0xFF);
    
    CMD(spi, 0x24);
    epd_spi_write_data_bulk(spi, data, (w / 8) * h);
    
    return ESP_OK;
}

static esp_err_t gdey0154d67_sleep(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x10);
    DATA(spi, 0x01);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    return ESP_OK;
}

static esp_err_t gdey0154d67_wake(epd_device_t *dev)
{
    return gdey0154d67_init(dev);
}

// Panel driver definition
const epd_panel_driver_t epd_panel_gdey0154d67 = {
    .name = "GDEY0154D67",
    .default_width = EPD_WIDTH,
    .default_height = EPD_HEIGHT,
    .color_mode = EPD_COLOR_BW,
    
    .init = gdey0154d67_init,
    .init_fast = gdey0154d67_init_fast,
    .init_partial = gdey0154d67_init_partial,
    .update = gdey0154d67_update,
    .update_fast = gdey0154d67_update_fast,
    .update_partial = gdey0154d67_update_partial,
    .write_ram = gdey0154d67_write_ram,
    .write_base_image = gdey0154d67_write_base_image,
    .write_ram_partial = gdey0154d67_write_ram_partial,
    .sleep = gdey0154d67_sleep,
    .wake = gdey0154d67_wake,
};
