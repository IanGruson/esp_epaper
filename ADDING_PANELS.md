# Adding a New Panel Driver

This guide explains how to add support for a new e-paper panel to the epaper component.

## Overview

Adding a new panel requires:

1. Creating a panel driver file with initialization and update functions
2. Registering the panel type in configuration headers
3. Adding the source file to CMakeLists.txt

## Step 1: Create Panel Driver File

Create a new file in `src/panels/` directory:

```
components/epaper/src/panels/your_panel.c
```

### Driver Template

```c
/**
 * @file your_panel.c
 * @brief Driver for YOUR_PANEL e-paper display
 */

#include "epaper_panel.h"
#include "../epaper_spi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "your_panel";

// Panel dimensions
#define EPD_WIDTH   200
#define EPD_HEIGHT  200

// External functions from epaper.c
extern epd_spi_t* epd_get_spi(epd_device_t *dev);
extern uint16_t epd_get_width(epd_device_t *dev);
extern uint16_t epd_get_height(epd_device_t *dev);

// Helper macros for cleaner code
#define CMD(spi, c)     epd_spi_write_cmd(spi, c)
#define DATA(spi, d)    epd_spi_write_data(spi, d)
#define WAIT(spi)       epd_spi_wait_busy(spi, 10000)
#define RESET(spi)      epd_spi_reset(spi)

/*=============================================================================
 * Panel Initialization
 *============================================================================*/

static esp_err_t your_panel_init(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    // Hardware reset
    RESET(spi);
    vTaskDelay(pdMS_TO_TICKS(50));
    WAIT(spi);
    
    // Panel-specific init sequence from datasheet
    CMD(spi, 0x12);  // Software reset
    WAIT(spi);
    
    CMD(spi, 0x01);  // Driver output control
    DATA(spi, (EPD_HEIGHT - 1) & 0xFF);
    DATA(spi, ((EPD_HEIGHT - 1) >> 8) & 0xFF);
    DATA(spi, 0x00);
    
    CMD(spi, 0x11);  // Data entry mode
    DATA(spi, 0x03);
    
    // Set RAM X/Y address
    CMD(spi, 0x44);
    DATA(spi, 0x00);
    DATA(spi, (EPD_WIDTH / 8) - 1);
    
    CMD(spi, 0x45);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    DATA(spi, (EPD_HEIGHT - 1) & 0xFF);
    DATA(spi, ((EPD_HEIGHT - 1) >> 8) & 0xFF);
    
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
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    
    WAIT(spi);
    
    ESP_LOGI(TAG, "Panel initialized");
    return ESP_OK;
}

static esp_err_t your_panel_init_fast(epd_device_t *dev)
{
    // Fast initialization (if supported)
    // Otherwise, use same as normal init
    return your_panel_init(dev);
}

static esp_err_t your_panel_init_partial(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    // Partial refresh requires special LUT
    // This example loads a partial-refresh LUT
    static const uint8_t lut_partial[] = {
        // Waveform data from datasheet
        0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        // ... more LUT data
    };
    
    CMD(spi, 0x32);  // Write LUT
    for (int i = 0; i < sizeof(lut_partial); i++) {
        DATA(spi, lut_partial[i]);
    }
    
    return ESP_OK;
}

/*=============================================================================
 * Display Update
 *============================================================================*/

static esp_err_t your_panel_update(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x22);  // Display update control
    DATA(spi, 0xC7); // Full update sequence
    CMD(spi, 0x20);  // Master activation
    WAIT(spi);
    
    return ESP_OK;
}

static esp_err_t your_panel_update_fast(epd_device_t *dev)
{
    // Fast update (if different from full)
    return your_panel_update(dev);
}

static esp_err_t your_panel_update_partial(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x22);
    DATA(spi, 0xCF);  // Partial update sequence
    CMD(spi, 0x20);
    WAIT(spi);
    
    return ESP_OK;
}

/*=============================================================================
 * RAM Write Functions
 *============================================================================*/

static esp_err_t your_panel_write_ram(epd_device_t *dev, const uint8_t *data, uint32_t len)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    // Reset RAM address
    CMD(spi, 0x4E);
    DATA(spi, 0x00);
    CMD(spi, 0x4F);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    
    // Write to RAM
    CMD(spi, 0x24);
    epd_spi_write_data_bulk(spi, data, len);
    
    return ESP_OK;
}

static esp_err_t your_panel_write_base_image(epd_device_t *dev, const uint8_t *data, uint32_t len)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    // For partial refresh: write to both current and previous RAM
    // Current RAM (new image)
    CMD(spi, 0x4E);
    DATA(spi, 0x00);
    CMD(spi, 0x4F);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    CMD(spi, 0x24);
    epd_spi_write_data_bulk(spi, data, len);
    
    // Previous RAM (base image for comparison)
    CMD(spi, 0x4E);
    DATA(spi, 0x00);
    CMD(spi, 0x4F);
    DATA(spi, 0x00);
    DATA(spi, 0x00);
    CMD(spi, 0x26);
    epd_spi_write_data_bulk(spi, data, len);
    
    return ESP_OK;
}

static esp_err_t your_panel_write_ram_partial(epd_device_t *dev,
                                               uint16_t x, uint16_t y,
                                               uint16_t w, uint16_t h,
                                               const uint8_t *data)
{
    // Windowed RAM write (if supported by panel)
    // Set RAM address window, then write data
    return ESP_ERR_NOT_SUPPORTED;
}

/*=============================================================================
 * Power Management
 *============================================================================*/

static esp_err_t your_panel_sleep(epd_device_t *dev)
{
    epd_spi_t *spi = epd_get_spi(dev);
    
    CMD(spi, 0x10);  // Enter deep sleep
    DATA(spi, 0x01);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "Panel in deep sleep");
    return ESP_OK;
}

static esp_err_t your_panel_wake(epd_device_t *dev)
{
    // Wake by re-initializing
    return your_panel_init(dev);
}

/*=============================================================================
 * Panel Driver Registration
 *============================================================================*/

const epd_panel_driver_t epd_panel_your_panel = {
    .name = "YOUR_PANEL",
    .default_width = EPD_WIDTH,
    .default_height = EPD_HEIGHT,
    .color_mode = EPD_COLOR_BW,  // EPD_COLOR_6COLOR for multi-color
    
    .init = your_panel_init,
    .init_fast = your_panel_init_fast,
    .init_partial = your_panel_init_partial,
    .update = your_panel_update,
    .update_fast = your_panel_update_fast,
    .update_partial = your_panel_update_partial,
    .write_ram = your_panel_write_ram,
    .write_base_image = your_panel_write_base_image,
    .write_ram_partial = your_panel_write_ram_partial,
    .sleep = your_panel_sleep,
    .wake = your_panel_wake,
};
```

## Step 2: Register Panel Type

### 2.1 Add Enum in `include/epaper_config.h`

```c
typedef enum {
    EPD_PANEL_GDEY0266T90 = 0,
    EPD_PANEL_GDEY0154D67,
    EPD_PANEL_GDEP073E01,
    EPD_PANEL_YOUR_PANEL,      // <-- Add here
    EPD_PANEL_COUNT
} epd_panel_type_t;
```

### 2.2 Declare Driver in `include/epaper_panel.h`

```c
// Existing declarations
extern const epd_panel_driver_t epd_panel_gdey0266t90;
extern const epd_panel_driver_t epd_panel_gdey0154d67;
extern const epd_panel_driver_t epd_panel_gdep073e01;

// Add new panel
extern const epd_panel_driver_t epd_panel_your_panel;
```

### 2.3 Register in `src/epaper.c`

Find the `epd_get_panel_driver()` function and add your panel:

```c
const epd_panel_driver_t* epd_get_panel_driver(epd_panel_type_t type)
{
    switch (type) {
        case EPD_PANEL_GDEY0266T90:
            return &epd_panel_gdey0266t90;
        case EPD_PANEL_GDEY0154D67:
            return &epd_panel_gdey0154d67;
        case EPD_PANEL_GDEP073E01:
            return &epd_panel_gdep073e01;
        case EPD_PANEL_YOUR_PANEL:           // <-- Add here
            return &epd_panel_your_panel;
        default:
            return NULL;
    }
}
```

## Step 3: Update CMakeLists.txt

Add your driver file to the SRCS list:

```cmake
idf_component_register(
    SRCS 
        "src/epaper.c"
        "src/epaper_spi.c"
        "src/epaper_lvgl.c"
        "src/panels/gdey0266t90.c"
        "src/panels/gdey0154d67.c"
        "src/panels/gdep073e01.c"
        "src/panels/your_panel.c"    # <-- Add here
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer
    PRIV_REQUIRES esp_psram
)
```

## Step 4: (Optional) Add Board Preset

Create a configuration macro for easy setup:

```c
// In include/epaper_config.h

#define EPD_CONFIG_YOUR_BOARD() { \
    .pins = { \
        .busy = GPIO_NUM_8, \
        .rst = GPIO_NUM_9, \
        .dc = GPIO_NUM_10, \
        .cs = GPIO_NUM_11, \
        .sck = GPIO_NUM_12, \
        .mosi = GPIO_NUM_13, \
    }, \
    .spi = { \
        .host = SPI2_HOST, \
        .speed_hz = 10000000, \
    }, \
    .panel = { \
        .type = EPD_PANEL_YOUR_PANEL, \
        .width = 0, \
        .height = 0, \
        .mirror_x = false, \
        .mirror_y = false, \
        .rotation = 0, \
    }, \
}
```

## Driver Interface Reference

| Function | Required | Description |
|----------|----------|-------------|
| `init` | ✓ | Full initialization sequence |
| `init_fast` | ✓ | Fast init (can be same as init) |
| `init_partial` | Optional | Partial refresh mode initialization |
| `update` | ✓ | Trigger full display refresh |
| `update_fast` | ✓ | Fast refresh (can be same as update) |
| `update_partial` | Optional | Partial refresh trigger |
| `write_ram` | ✓ | Write framebuffer to display RAM |
| `write_base_image` | Optional | Write to both RAM registers (for partial) |
| `write_ram_partial` | Optional | Windowed RAM write |
| `sleep` | Optional | Enter deep sleep mode |
| `wake` | Optional | Wake from sleep |

## Color Modes

| Mode | Bits/Pixel | Buffer Calc | Description |
|------|------------|-------------|-------------|
| `EPD_COLOR_BW` | 1 | W×H/8 | Black/White |
| `EPD_COLOR_BWR` | 2 | W×H/4 | Black/White/Red |
| `EPD_COLOR_BWY` | 2 | W×H/4 | Black/White/Yellow |
| `EPD_COLOR_4GRAY` | 2 | W×H/4 | 4 Gray levels |
| `EPD_COLOR_6COLOR` | 4 | W×H/2 | 6 Colors (BWYRGG) |
| `EPD_COLOR_7COLOR` | 4 | W×H/2 | 7 Colors |

## SPI Helper Functions

Available in `epaper_spi.h`:

```c
// Write single command byte
void epd_spi_write_cmd(epd_spi_t *spi, uint8_t cmd);

// Write single data byte
void epd_spi_write_data(epd_spi_t *spi, uint8_t data);

// Write bulk data (handles chunking for large transfers)
void epd_spi_write_data_bulk(epd_spi_t *spi, const uint8_t *data, uint32_t len);

// Hardware reset (toggle RST pin)
void epd_spi_reset(epd_spi_t *spi);

// Read busy pin state
int epd_spi_is_busy(epd_spi_t *spi);

// Wait for busy pin with timeout
void epd_spi_wait_busy(epd_spi_t *spi, uint32_t timeout_ms);
```

## Common Issues and Solutions

### Busy Signal Polarity

Some panels use HIGH=busy, others LOW=busy. Check your datasheet!

```c
// Standard: HIGH = busy, LOW = ready
static void wait_busy_high(epd_spi_t *spi, uint32_t timeout_ms) {
    while (epd_spi_is_busy(spi)) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// Inverted: LOW = busy, HIGH = ready (e.g., GDEP073E01)
static void wait_busy_low(epd_spi_t *spi, uint32_t timeout_ms) {
    while (!epd_spi_is_busy(spi)) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

### Multi-Color Panel RAM Format

6/7-color panels use 4-bit per pixel (2 pixels per byte):

```c
// Set pixel in 4-bit framebuffer
static void set_pixel_4bpp(uint8_t *fb, int x, int y, int width, uint8_t color)
{
    uint32_t addr = (y * width + x) / 2;
    if (x % 2 == 0) {
        // High nibble (even pixel)
        fb[addr] = (fb[addr] & 0x0F) | (color << 4);
    } else {
        // Low nibble (odd pixel)
        fb[addr] = (fb[addr] & 0xF0) | (color & 0x0F);
    }
}
```

### Large SPI Transfers

The component automatically chunks large transfers (>4KB) to avoid DMA limits. No special handling needed in panel drivers.

### Partial Refresh Requirements

1. Initialize base image with `write_base_image()` on first display
2. Load partial LUT in `init_partial()`
3. Use `update_partial()` for subsequent updates
4. Periodically do full refresh to clear ghosting

## Testing Your Driver

1. **Compile test**: `idf.py build`
2. **Basic test**: Fill screen with white, then black
3. **Pattern test**: Draw checkerboard or gradient
4. **Partial test**: Update small region repeatedly
5. **Sleep/wake test**: Verify low power mode works

## Resources

- [Good Display Wiki](https://www.good-display.com/companyfile/101.html) - Datasheets and app notes
- [Waveshare E-Paper Wiki](https://www.waveshare.com/wiki/E-Paper) - Reference implementations
- [ESP-IDF SPI Master](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/spi_master.html) - SPI documentation

## License

MIT License
