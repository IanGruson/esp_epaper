# ESP E-Paper Component

[![Component Registry](https://components.espressif.com/components/tuanpmt/esp_epaper/badge.svg)](https://components.espressif.com/components/tuanpmt/esp_epaper)

A flexible e-paper display driver component for ESP-IDF with LVGL 9 integration.

## Features

- **Multi-panel Support**: Black/White, 3-color (BWR/BWY), and 6-color panels
- **Runtime Configuration**: GPIO pins, SPI settings, panel type configurable at runtime
- **LVGL 9 Integration**: Full display driver with automatic color conversion
- **Floyd-Steinberg Dithering**: High-quality image rendering for photos and gradients
- **Partial Refresh**: Optimized updates for supported panels (reduces ghosting)
- **Panel Abstraction**: Easy to add new panel drivers via vtable interface
- **Memory Efficient**: Uses PSRAM for large buffers when available

## Supported Panels

| Panel | Size | Resolution | Colors | Partial Refresh |
|-------|------|------------|--------|-----------------|
| GDEY0266T90 | 2.66" | 152×296 | BW | ✓ |
| GDEY0154D67 | 1.54" | 200×200 | BW | ✓ |
| GDEP073E01 | 7.3" | 800×480 | 6-Color | ✗ |

## Supported Boards

| Board | Panel | Link |
|-------|-------|------|
| ESP32-S3-ePaper-1.54 | 200×200 BW | [Waveshare Wiki](https://www.waveshare.com/wiki/ESP32-S3-ePaper-1.54) |
| ESP32-S3-PhotoPainter | 800×480 6-Color | [Waveshare Wiki](https://www.waveshare.com/wiki/ESP32-S3-PhotoPainter) |

## Installation

### Using ESP-IDF Component Registry (Recommended)

Add to your project's `idf_component.yml`:

```yaml
dependencies:
  tuanpmt/esp_epaper: "^1.0.0"
  lvgl/lvgl: "^9.0.0"
```

Then run:
```bash
idf.py reconfigure
```

### Manual Installation

Clone or copy this repository to your project's `components` folder:

```bash
cd your_project/components
git clone https://github.com/tuanpmt/esp_epaper.git
```

## Quick Start

### Basic Usage

```c
#include "epaper.h"
#include "epaper_lvgl.h"
#include "lvgl.h"

void app_main(void)
{
    lv_init();
    
    // Use preset configuration for ESP32-S3-ePaper-1.54
    epd_config_t cfg = EPD_CONFIG_ESP32S3_154();
    
    // Initialize e-paper
    epd_handle_t epd;
    ESP_ERROR_CHECK(epd_init(&cfg, &epd));
    
    // Initialize LVGL display
    epd_lvgl_config_t lvgl_cfg = EPD_LVGL_CONFIG_DEFAULT();
    lvgl_cfg.epd = epd;
    lv_display_t *disp = epd_lvgl_init(&lvgl_cfg);
    
    // Create UI with LVGL...
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello E-Paper!");
    lv_obj_center(label);
    
    // Refresh display
    epd_lvgl_refresh(disp);
}
```

### 6-Color Panel with Dithering (PhotoPainter)

```c
epd_config_t cfg = EPD_CONFIG_73_6COLOR();
epd_handle_t epd;
epd_init(&cfg, &epd);

epd_lvgl_config_t lvgl_cfg = EPD_LVGL_CONFIG_DEFAULT();
lvgl_cfg.epd = epd;
lvgl_cfg.update_mode = EPD_UPDATE_FULL;
lvgl_cfg.dither_mode = EPD_DITHER_FLOYD_STEINBERG;  // Enable dithering

lv_display_t *disp = epd_lvgl_init(&lvgl_cfg);
```

## Configuration Presets

```c
// Waveshare ESP32-S3-ePaper-1.54 (1.54" BW)
epd_config_t cfg = EPD_CONFIG_ESP32S3_154();

// Waveshare ESP32-S3-PhotoPainter (7.3" 6-Color)
epd_config_t cfg = EPD_CONFIG_73_6COLOR();
```

### Custom Configuration

```c
epd_config_t cfg = {
    .pins = {
        .busy = GPIO_NUM_8,
        .rst = GPIO_NUM_9,
        .dc = GPIO_NUM_10,
        .cs = GPIO_NUM_11,
        .sck = GPIO_NUM_12,
        .mosi = GPIO_NUM_13,
    },
    .spi = {
        .host = SPI2_HOST,
        .speed_hz = 10000000,  // 10MHz
    },
    .panel = {
        .type = EPD_PANEL_GDEY0154D67,
        .width = 0,   // 0 = use panel default
        .height = 0,
    },
};
```

## Update Modes

| Mode | Description | Use Case |
|------|-------------|----------|
| `EPD_UPDATE_FULL` | Full refresh with flashing | Initial display, clearing ghosting |
| `EPD_UPDATE_PARTIAL` | Fast partial update | UI updates, counters |
| `EPD_UPDATE_FAST` | Fast mode (panel dependent) | Animations |

## Dithering

Floyd-Steinberg dithering improves image quality for photos and gradients on limited color displays.

```c
lvgl_cfg.dither_mode = EPD_DITHER_FLOYD_STEINBERG;
```

**Memory Requirement**: Dithering requires an RGB888 buffer (width × height × 3 bytes). Uses PSRAM automatically if available.

## Examples

See the `examples/` folder for complete examples:

| Example | Board | Description |
|---------|-------|-------------|
| [esp32s3_epaper_154](examples/esp32s3_epaper_154) | ESP32-S3-ePaper-1.54 | 1.54" BW with partial refresh |
| [esp32s3_photopainter](examples/esp32s3_photopainter) | ESP32-S3-PhotoPainter | 7.3" 6-Color with dithering |

### Building Examples

```bash
cd examples/esp32s3_epaper_154
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

## API Reference

### Core Functions

```c
esp_err_t epd_init(const epd_config_t *config, epd_handle_t *handle);
esp_err_t epd_deinit(epd_handle_t handle);
esp_err_t epd_get_info(epd_handle_t handle, epd_panel_info_t *info);
uint8_t* epd_get_framebuffer(epd_handle_t handle);
esp_err_t epd_update(epd_handle_t handle, const uint8_t *data, epd_update_mode_t mode);
esp_err_t epd_fill(epd_handle_t handle, uint8_t color);
esp_err_t epd_sleep(epd_handle_t handle);
esp_err_t epd_wake(epd_handle_t handle);
```

### LVGL Functions

```c
lv_display_t* epd_lvgl_init(const epd_lvgl_config_t *config);
void epd_lvgl_deinit(lv_display_t *disp);
void epd_lvgl_refresh(lv_display_t *disp);
void epd_lvgl_force_full_refresh(lv_display_t *disp);
```

## Adding New Panels

See [ADDING_PANELS.md](ADDING_PANELS.md) for detailed instructions on adding support for new e-paper panels.

## Memory Requirements

| Panel | Framebuffer | RGB Buffer (dithering) | Total |
|-------|-------------|------------------------|-------|
| 200×200 BW | 5 KB | 120 KB | 125 KB |
| 152×296 BW | 5.6 KB | 135 KB | 140.6 KB |
| 800×480 6-Color | 192 KB | 1.15 MB | 1.34 MB |

**Note**: Large buffers are allocated from PSRAM when available.

## License

MIT License - See [LICENSE](LICENSE)

## Author

- **tuanpmt** - [GitHub](https://github.com/tuanpmt)
