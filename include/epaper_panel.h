#ifndef _EPAPER_PANEL_H_
#define _EPAPER_PANEL_H_

#include <stdint.h>
#include "esp_err.h"
#include "epaper_config.h"

// Forward declaration
typedef struct epd_device epd_device_t;

// Panel driver interface (vtable)
typedef struct {
    const char *name;
    uint16_t default_width;
    uint16_t default_height;
    epd_color_mode_t color_mode;
    
    // Driver functions
    esp_err_t (*init)(epd_device_t *dev);
    esp_err_t (*init_fast)(epd_device_t *dev);
    esp_err_t (*init_partial)(epd_device_t *dev);
    esp_err_t (*update)(epd_device_t *dev);
    esp_err_t (*update_fast)(epd_device_t *dev);
    esp_err_t (*update_partial)(epd_device_t *dev);
    esp_err_t (*write_ram)(epd_device_t *dev, const uint8_t *data, uint32_t len);
    esp_err_t (*write_base_image)(epd_device_t *dev, const uint8_t *data, uint32_t len);  // For partial
    esp_err_t (*write_ram_partial)(epd_device_t *dev, uint16_t x, uint16_t y, 
                                    uint16_t w, uint16_t h, const uint8_t *data);
    esp_err_t (*sleep)(epd_device_t *dev);
    esp_err_t (*wake)(epd_device_t *dev);
} epd_panel_driver_t;

// Get panel driver by type
const epd_panel_driver_t* epd_get_panel_driver(epd_panel_type_t type);

// Panel drivers declarations - Specific panels (custom LUT/features)
extern const epd_panel_driver_t epd_panel_gdey0154d67;  // 1.54" BW (custom LUT)
extern const epd_panel_driver_t epd_panel_gdep073e01;   // 7.3" 6-Color

// Generic SSD16xx drivers (same code, different resolutions)
extern const epd_panel_driver_t epd_panel_ssd16xx_154;  // 1.54" 200x200
extern const epd_panel_driver_t epd_panel_ssd16xx_213;  // 2.13" 122x250
extern const epd_panel_driver_t epd_panel_ssd16xx_266;  // 2.66" 152x296
extern const epd_panel_driver_t epd_panel_ssd16xx_270;  // 2.7" 176x264
extern const epd_panel_driver_t epd_panel_ssd16xx_290;  // 2.9" 128x296
extern const epd_panel_driver_t epd_panel_ssd16xx_370;  // 3.7" 280x480
extern const epd_panel_driver_t epd_panel_ssd16xx_420;  // 4.2" 400x300

#endif // _EPAPER_PANEL_H_
