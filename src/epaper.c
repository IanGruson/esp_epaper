#include "epaper.h"
#include "epaper_panel.h"
#include "epaper_spi.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "epaper";

// Device structure
struct epd_device {
    epd_config_t config;
    epd_spi_t spi;
    const epd_panel_driver_t *driver;
    uint16_t width;
    uint16_t height;
    uint32_t buffer_size;
    uint8_t *framebuffer;
    bool initialized;
    bool partial_ready;  // Partial mode initialized
};

const epd_panel_driver_t* epd_get_panel_driver(epd_panel_type_t type)
{
    switch (type) {
        // Specific panels with custom LUT/features
        case EPD_PANEL_GDEY0154D67:
            return &epd_panel_gdey0154d67;
        case EPD_PANEL_GDEP073E01:
            return &epd_panel_gdep073e01;
        case EPD_PANEL_GDEY037F51:
            return &epd_panel_gdey037f51;
        
        // Generic SSD16xx BW panels - same driver, different sizes
        case EPD_PANEL_SSD16XX_154:
            return &epd_panel_ssd16xx_154;
        case EPD_PANEL_SSD16XX_213:
            return &epd_panel_ssd16xx_213;
        case EPD_PANEL_SSD16XX_266:  // Also handles EPD_PANEL_GDEY0266T90 (alias)
            return &epd_panel_ssd16xx_266;
        case EPD_PANEL_SSD16XX_270:
            return &epd_panel_ssd16xx_270;
        case EPD_PANEL_SSD16XX_290:
            return &epd_panel_ssd16xx_290;
        case EPD_PANEL_SSD16XX_370:
            return &epd_panel_ssd16xx_370;
        case EPD_PANEL_SSD16XX_420:
            return &epd_panel_ssd16xx_420;
            
        default:
            return NULL;
    }
}

esp_err_t epd_init(const epd_config_t *config, epd_handle_t *handle)
{
    if (!config || !handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Allocate device
    epd_device_t *dev = calloc(1, sizeof(epd_device_t));
    if (!dev) {
        return ESP_ERR_NO_MEM;
    }
    
    // Copy config
    memcpy(&dev->config, config, sizeof(epd_config_t));
    
    // Get panel driver
    dev->driver = epd_get_panel_driver(config->panel.type);
    if (!dev->driver) {
        ESP_LOGE(TAG, "Unknown panel type: %d", config->panel.type);
        free(dev);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // Set dimensions
    dev->width = config->panel.width > 0 ? config->panel.width : dev->driver->default_width;
    dev->height = config->panel.height > 0 ? config->panel.height : dev->driver->default_height;
    
    // Calculate buffer size based on color mode
    if (dev->driver->color_mode == EPD_COLOR_6COLOR || 
        dev->driver->color_mode == EPD_COLOR_7COLOR) {
        // 4-bit per pixel (2 pixels per byte)
        dev->buffer_size = (dev->width * dev->height) / 2;
    } else if (dev->driver->color_mode == EPD_COLOR_4GRAY ||
               dev->driver->color_mode == EPD_COLOR_4COLOR) {
        // 2-bit per pixel (4 pixels per byte)
        dev->buffer_size = (dev->width * dev->height) / 4;
    } else {
        // 1-bit per pixel (BW, BWR, BWY)
        dev->buffer_size = (dev->width * dev->height) / 8;
    }
    
    // Allocate framebuffer (prefer PSRAM for large buffers)
    if (dev->buffer_size > 32768) {
        // Large buffer: use PSRAM
        dev->framebuffer = heap_caps_malloc(dev->buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    } else {
        // Small buffer: use DMA-capable internal RAM
        dev->framebuffer = heap_caps_malloc(dev->buffer_size, MALLOC_CAP_DMA);
    }
    if (!dev->framebuffer) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer (%lu bytes)", dev->buffer_size);
        free(dev);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Framebuffer allocated: %lu bytes", dev->buffer_size);
    memset(dev->framebuffer, 0xFF, dev->buffer_size);  // White
    
    // Initialize SPI
    esp_err_t ret = epd_spi_init(&dev->spi, &config->pins, &config->spi);
    if (ret != ESP_OK) {
        free(dev->framebuffer);
        free(dev);
        return ret;
    }
    
    // Initialize panel
    ret = dev->driver->init(dev);
    if (ret != ESP_OK) {
        epd_spi_deinit(&dev->spi);
        free(dev->framebuffer);
        free(dev);
        return ret;
    }
    
    dev->initialized = true;
    *handle = dev;
    
    ESP_LOGI(TAG, "Initialized %s (%dx%d)", dev->driver->name, dev->width, dev->height);
    return ESP_OK;
}

esp_err_t epd_deinit(epd_handle_t handle)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    
    epd_device_t *dev = handle;
    
    if (dev->driver && dev->driver->sleep) {
        dev->driver->sleep(dev);
    }
    
    epd_spi_deinit(&dev->spi);
    
    if (dev->framebuffer) {
        free(dev->framebuffer);
    }
    
    free(dev);
    return ESP_OK;
}

esp_err_t epd_get_info(epd_handle_t handle, epd_panel_info_t *info)
{
    if (!handle || !info) return ESP_ERR_INVALID_ARG;
    
    epd_device_t *dev = handle;
    info->width = dev->width;
    info->height = dev->height;
    info->buffer_size = dev->buffer_size;
    info->color_mode = dev->driver->color_mode;
    return ESP_OK;
}

esp_err_t epd_clear(epd_handle_t handle)
{
    return epd_fill(handle, 0xFF);
}

esp_err_t epd_fill(epd_handle_t handle, uint8_t color)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    
    epd_device_t *dev = handle;
    memset(dev->framebuffer, color, dev->buffer_size);
    
    // Use update flow (which only inits if needed)
    return epd_update(handle, dev->framebuffer, EPD_UPDATE_FULL);
}

esp_err_t epd_update(epd_handle_t handle, const uint8_t *buffer, epd_update_mode_t mode)
{
    if (!handle || !buffer) return ESP_ERR_INVALID_ARG;
    
    epd_device_t *dev = handle;
    esp_err_t ret;
    
    // Copy to framebuffer
    memcpy(dev->framebuffer, buffer, dev->buffer_size);
    
    switch (mode) {
        case EPD_UPDATE_FAST:
            ret = dev->driver->init_fast(dev);
            if (ret != ESP_OK) return ret;
            ret = dev->driver->write_ram(dev, buffer, dev->buffer_size);
            if (ret != ESP_OK) return ret;
            return dev->driver->update_fast(dev);
            
        case EPD_UPDATE_PARTIAL:
            // Only init partial once, then just write and update
            if (!dev->partial_ready) {
                // First time: do full refresh to set base image, then init partial
                ret = dev->driver->init(dev);
                if (ret != ESP_OK) return ret;
                // Write base image (0x24 + 0x26)
                if (dev->driver->write_base_image) {
                    ret = dev->driver->write_base_image(dev, buffer, dev->buffer_size);
                } else {
                    ret = dev->driver->write_ram(dev, buffer, dev->buffer_size);
                }
                if (ret != ESP_OK) return ret;
                ret = dev->driver->update(dev);
                if (ret != ESP_OK) return ret;
                // Now init partial mode
                if (dev->driver->init_partial) {
                    ret = dev->driver->init_partial(dev);
                    if (ret != ESP_OK) return ret;
                }
                dev->partial_ready = true;
                ESP_LOGI(TAG, "Partial mode ready");
                return ESP_OK;
            }
            // Subsequent partial updates: just write and update
            ret = dev->driver->write_ram(dev, buffer, dev->buffer_size);
            if (ret != ESP_OK) return ret;
            return dev->driver->update_partial(dev);
            
        case EPD_UPDATE_FULL:
        default:
            dev->partial_ready = false;  // Reset partial mode
            // Only init if not initialized or waking from sleep
            if (!dev->initialized) {
                ret = dev->driver->init(dev);
                if (ret != ESP_OK) return ret;
                dev->initialized = true;
            }
            ret = dev->driver->write_ram(dev, buffer, dev->buffer_size);
            if (ret != ESP_OK) return ret;
            return dev->driver->update(dev);
    }
}

esp_err_t epd_update_partial(epd_handle_t handle, uint16_t x, uint16_t y, 
                              uint16_t w, uint16_t h, const uint8_t *buffer)
{
    if (!handle || !buffer) return ESP_ERR_INVALID_ARG;
    
    epd_device_t *dev = handle;
    
    if (dev->driver->write_ram_partial) {
        esp_err_t ret = dev->driver->write_ram_partial(dev, x, y, w, h, buffer);
        if (ret != ESP_OK) return ret;
        return dev->driver->update_partial(dev);
    }
    
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t epd_set_base_image(epd_handle_t handle, const uint8_t *buffer)
{
    if (!handle || !buffer) return ESP_ERR_INVALID_ARG;
    
    epd_device_t *dev = handle;
    memcpy(dev->framebuffer, buffer, dev->buffer_size);
    return ESP_OK;
}

esp_err_t epd_sleep(epd_handle_t handle)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    
    epd_device_t *dev = handle;
    if (dev->driver->sleep) {
        return dev->driver->sleep(dev);
    }
    return ESP_OK;
}

esp_err_t epd_wake(epd_handle_t handle)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    
    epd_device_t *dev = handle;
    if (dev->driver->wake) {
        return dev->driver->wake(dev);
    }
    return dev->driver->init(dev);
}

bool epd_is_busy(epd_handle_t handle)
{
    if (!handle) return false;
    
    epd_device_t *dev = handle;
    return epd_spi_is_busy(&dev->spi) != 0;
}

esp_err_t epd_wait_busy(epd_handle_t handle, uint32_t timeout_ms)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    
    epd_device_t *dev = handle;
    epd_spi_wait_busy(&dev->spi, timeout_ms);
    return ESP_OK;
}

// LVGL Integration
uint8_t* epd_get_framebuffer(epd_handle_t handle)
{
    if (!handle) return NULL;
    return handle->framebuffer;
}

esp_err_t epd_flush_framebuffer(epd_handle_t handle, epd_update_mode_t mode)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    return epd_update(handle, handle->framebuffer, mode);
}

esp_err_t epd_lvgl_flush(epd_handle_t handle, const void *area, const uint8_t *px_map)
{
    // This is a simple implementation for LVGL 9
    // area is lv_area_t*, px_map is the pixel data
    // For full integration, user should implement based on their LVGL setup
    
    if (!handle || !px_map) return ESP_ERR_INVALID_ARG;
    
    epd_device_t *dev = handle;
    memcpy(dev->framebuffer, px_map, dev->buffer_size);
    return ESP_OK;
}

// Expose SPI for panel drivers
epd_spi_t* epd_get_spi(epd_handle_t handle) {
    return handle ? &handle->spi : NULL;
}

uint16_t epd_get_width(epd_handle_t handle) {
    return handle ? handle->width : 0;
}

uint16_t epd_get_height(epd_handle_t handle) {
    return handle ? handle->height : 0;
}
