/**
 * @file gdey0266t90.c
 * @brief Driver for GDEY0266T90 2.66" B/W e-paper display (152x296)
 */

#include "epaper_panel.h"
#include "../epaper_spi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Panel dimensions
#define EPD_WIDTH   152
#define EPD_HEIGHT  296

// Forward declarations
struct epd_device;
typedef struct epd_device epd_device_t;

// Access internal device
extern epd_spi_t* epd_get_spi(epd_device_t *dev);
extern uint16_t epd_get_width(epd_device_t *dev);
extern uint16_t epd_get_height(epd_device_t *dev);

// Helper macros
#define CMD(spi, c)    epd_spi_write_cmd(spi, c)
#define DATA(spi, d)   epd_spi_write_data(spi, d)
#define WAIT(spi)      epd_spi_wait_busy(spi, 0)
#define RESET(spi)     epd_spi_reset(spi)

static esp_err_t gdey0266t90_init(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    uint16_t h = epd_get_height(dev);
    uint16_t w = epd_get_width(dev);
    
    RESET(spi);
    WAIT(spi);
    
    CMD(spi, 0x12);  // SWRESET
    WAIT(spi);
    
    CMD(spi, 0x01);  // Driver output control
    DATA(spi, (h - 1) % 256);
    DATA(spi, (h - 1) / 256);
    DATA(spi, 0x00);
    
    CMD(spi, 0x11);  // Data entry mode
    DATA(spi, 0x01);
    
    CMD(spi, 0x44);  // Set RAM X start/end
    DATA(spi, 0x00);
    DATA(spi, w / 8 - 1);
    
    CMD(spi, 0x45);  // Set RAM Y start/end
    DATA(spi, (h - 1) % 256);
    DATA(spi, (h - 1) / 256);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    
    CMD(spi, 0x3C);  // Border waveform
    DATA(spi, 0x05);
    
    CMD(spi, 0x21);  // Display update control
    DATA(spi, 0x00);
    DATA(spi, 0x80);
    
    CMD(spi, 0x18);  // Temperature sensor
    DATA(spi, 0x80);
    
    CMD(spi, 0x4E);  // Set RAM X counter
    DATA(spi, 0x00);
    
    CMD(spi, 0x4F);  // Set RAM Y counter
    DATA(spi, (h - 1) % 256);
    DATA(spi, (h - 1) / 256);
    
    WAIT(spi);
    return ESP_OK;
}

static esp_err_t gdey0266t90_init_fast(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    RESET(spi);
    
    CMD(spi, 0x12);  // SWRESET
    WAIT(spi);
    
    CMD(spi, 0x18);  // Temperature sensor
    DATA(spi, 0x80);
    
    CMD(spi, 0x22);  // Load temperature value
    DATA(spi, 0xB1);
    CMD(spi, 0x20);
    WAIT(spi);
    
    CMD(spi, 0x1A);  // Write temperature register
    DATA(spi, 0x64);
    DATA(spi, 0x00);
    
    CMD(spi, 0x22);  // Load temperature value
    DATA(spi, 0x91);
    CMD(spi, 0x20);
    WAIT(spi);
    
    return ESP_OK;
}

static esp_err_t gdey0266t90_init_partial(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    RESET(spi);
    
    CMD(spi, 0x3C);  // Border waveform
    DATA(spi, 0x80);
    
    return ESP_OK;
}

static esp_err_t gdey0266t90_update(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x22);  // Display update control
    DATA(spi, 0xF7);
    CMD(spi, 0x20);  // Activate display update
    WAIT(spi);
    
    return ESP_OK;
}

static esp_err_t gdey0266t90_update_fast(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x22);
    DATA(spi, 0xC7);
    CMD(spi, 0x20);
    WAIT(spi);
    
    return ESP_OK;
}

static esp_err_t gdey0266t90_update_partial(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x22);
    DATA(spi, 0xFF);
    CMD(spi, 0x20);
    WAIT(spi);
    
    return ESP_OK;
}

static esp_err_t gdey0266t90_write_ram(epd_device_t *dev, const uint8_t *data, uint32_t len)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x24);  // Write RAM black/white
    epd_spi_write_data_bulk(spi, data, len);
    
    return ESP_OK;
}

static esp_err_t gdey0266t90_write_ram_partial(epd_device_t *dev, 
                                                uint16_t x, uint16_t y,
                                                uint16_t w, uint16_t h,
                                                const uint8_t *data)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    uint16_t x_start = x / 8;
    uint16_t x_end = x_start + w / 8 - 1;
    uint16_t y_end = y + h - 1;
    
    RESET(spi);
    
    CMD(spi, 0x3C);  // Border waveform
    DATA(spi, 0x80);
    
    CMD(spi, 0x44);  // Set RAM X start/end
    DATA(spi, x_start);
    DATA(spi, x_end);
    
    CMD(spi, 0x45);  // Set RAM Y start/end
    DATA(spi, y % 256);
    DATA(spi, y / 256);
    DATA(spi, y_end % 256);
    DATA(spi, y_end / 256);
    
    CMD(spi, 0x4E);  // Set RAM X counter
    DATA(spi, x_start);
    
    CMD(spi, 0x4F);  // Set RAM Y counter
    DATA(spi, y % 256);
    DATA(spi, y / 256);
    
    CMD(spi, 0x24);  // Write RAM
    epd_spi_write_data_bulk(spi, data, (w / 8) * h);
    
    return ESP_OK;
}

static esp_err_t gdey0266t90_sleep(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x10);  // Enter deep sleep
    DATA(spi, 0x01);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    return ESP_OK;
}

static esp_err_t gdey0266t90_wake(epd_device_t *dev)
{
    return gdey0266t90_init(dev);
}

// Panel driver definition
const epd_panel_driver_t epd_panel_gdey0266t90 = {
    .name = "GDEY0266T90",
    .default_width = EPD_WIDTH,
    .default_height = EPD_HEIGHT,
    .color_mode = EPD_COLOR_BW,
    
    .init = gdey0266t90_init,
    .init_fast = gdey0266t90_init_fast,
    .init_partial = gdey0266t90_init_partial,
    .update = gdey0266t90_update,
    .update_fast = gdey0266t90_update_fast,
    .update_partial = gdey0266t90_update_partial,
    .write_ram = gdey0266t90_write_ram,
    .write_ram_partial = gdey0266t90_write_ram_partial,
    .sleep = gdey0266t90_sleep,
    .wake = gdey0266t90_wake,
};
