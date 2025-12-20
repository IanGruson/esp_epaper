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

// Panel drivers declarations
extern const epd_panel_driver_t epd_panel_gdey0266t90;
extern const epd_panel_driver_t epd_panel_gdey0154d67;
extern const epd_panel_driver_t epd_panel_gdep073e01;  // 7.3" 6-Color

#endif // _EPAPER_PANEL_H_
