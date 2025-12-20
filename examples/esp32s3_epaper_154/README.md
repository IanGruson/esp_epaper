# ESP32-S3-ePaper-1.54 Example

Example project for [Waveshare ESP32-S3-ePaper-1.54](https://www.waveshare.com/wiki/ESP32-S3-ePaper-1.54) development board.

## Hardware

- **Board**: Waveshare ESP32-S3-ePaper-1.54
- **MCU**: ESP32-S3 with 4MB Flash + 2MB PSRAM
- **Panel**: GDEY0154D67 (200×200 pixels, Black/White)
- **Features**: Partial refresh support, RTC, Temperature sensor

## Pinout

| Signal | GPIO |
|--------|------|
| BUSY   | 5    |
| RST    | 7    |
| DC     | 4    |
| CS     | 10   |
| SCK    | 1    |
| MOSI   | 2    |
| PWR_EN | 6    |

## Build and Flash

```bash
# Set target
idf.py set-target esp32s3

# Build
idf.py build

# Flash and monitor
idf.py -p PORT flash monitor
```

## Features Demonstrated

- E-paper initialization with `EPD_CONFIG_ESP32S3_154()` preset
- LVGL 9 integration
- Partial refresh for fast updates
- Periodic full refresh to clear ghosting
- Board power control (GPIO6)

## Usage

The example displays a counter that updates every 10 seconds using partial refresh. Every 5 updates, a full refresh is performed to clear any ghosting artifacts.

## License

MIT License
