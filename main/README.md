# House LED String Lights

ESP32-based controller for multiple addressable LED string lights, designed for house decoration.

## Overview

This project controls up to 6 independent addressable LED strings (e.g., WS2812/NeoPixel) using an ESP32-DevKitC-VE development board. Each LED string can be configured with its own GPIO pin and LED count. The controller updates LED strings at a configurable refresh rate (default 33ms for ~30 FPS) while maintaining a status LED that blinks at 1 second intervals.

## Features

* **Multiple LED Strings**: Support for 1-6 independent LED strings
* **Configurable GPIO Pins**: Each string can be assigned to any available GPIO pin
* **Configurable LED Count**: Each string can have 1-300 LEDs
* **High Refresh Rate**: Configurable update period (16ms-1000ms) for smooth animations
* **Status LED**: Single LED on GPIO 27 blinks at 1 second intervals
* **50% Brightness**: LEDs operate at 50% brightness (RGB 127, 127, 127) when on
* **RMT/SPI Backend**: Supports both RMT and SPI backends for LED control

## Hardware Requirements

* ESP32-DevKitC-VE development board
* Addressable LED strings (WS2812/NeoPixel compatible)
* Power supply capable of handling ESP32 board and LED strings
* USB cable for programming and power (or external 5V supply)

### Default GPIO Configuration

The project is configured for GPIO pins on the same side of the ESP32-DevKitC-VE board:
* String 1: GPIO 2
* String 2: GPIO 4
* String 3: GPIO 5
* String 4: GPIO 18
* String 5: GPIO 19
* String 6: GPIO 21
* Status LED: GPIO 27

**Note**: GPIO pins 6-11 are reserved for SPI flash on ESP32-DevKitC-VE and should not be used.

## Software Requirements

* ESP-IDF v5.5.1 or later
* Python 3.11+
* CMake 3.16+

## Configuration

Configure the project using `idf.py menuconfig`. Navigate to `Example Configuration`:

### LED String Configuration

* **LED String Count**: Number of LED strings to control (1-6)
* **LED String X GPIO**: GPIO pin number for each string
* **LED String X LED Count**: Number of LEDs in each string (1-300, default 50)

### Update Period

Select the LED string update period:
* 16ms (60 FPS)
* 33ms (30 FPS) - **Default**
* 50ms (20 FPS)
* 100ms (10 FPS)
* 200ms (5 FPS)
* 500ms (2 FPS)
* 1000ms (1 FPS)

### Backend Selection

Choose the LED strip backend:
* **RMT**: Available on ESP32, ESP32-S2, ESP32-S3 (recommended)
* **SPI**: Available on all ESP targets

## Build and Flash

1. Set the target chip:
   ```bash
   idf.py set-target esp32
   ```

2. Configure the project:
   ```bash
   idf.py menuconfig
   ```

3. Build the project:
   ```bash
   idf.py build
   ```

4. Flash to the board:
   ```bash
   idf.py -p PORT flash
   ```

5. Monitor serial output:
   ```bash
   idf.py -p PORT monitor
   ```

Replace `PORT` with your serial port (e.g., `COM6` on Windows, `/dev/ttyUSB0` on Linux).

## Operation

On startup, the controller:
1. Configures all LED strings according to the configuration
2. Initializes the status LED on GPIO 27
3. Begins updating LED strings at the configured refresh rate

**LED Behavior**:
* When ON: All LEDs in all strings display white/gray at 50% brightness (RGB 127, 127, 127)
* When OFF: All LEDs are cleared (turned off)
* Status LED: Blinks every 1 second regardless of LED string state

## Project Structure

```
house_led_string_lights/
├── main/
│   ├── blink_example_main.c    # Main application code
│   ├── Kconfig.projbuild       # Configuration options
│   ├── CMakeLists.txt          # Component build configuration
│   └── idf_component.yml       # Component dependencies
├── CMakeLists.txt              # Project build configuration
├── README.md                   # This file
└── .gitignore                  # Git ignore patterns
```

## Troubleshooting

### LED Strings Not Working

* Verify GPIO pins are correctly configured and not reserved
* Check that LED strings are properly connected to GPIO pins
* Ensure power supply can handle both ESP32 and LED strings
* Verify LED string type matches WS2812/NeoPixel protocol

### Build Errors

* Ensure ESP-IDF environment is properly set up
* Run `idf.py fullclean` if experiencing build cache issues
* Check that all dependencies are installed via component manager

### Serial Port Issues

* Close any other applications using the serial port
* Verify correct COM port on Windows (Device Manager)
* Check USB cable connection

## License

This project is based on ESP-IDF example code and is in the Public Domain (CC0 licensed).

## References

* [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
* [LED Strip Component](https://components.espressif.com/component/espressif/led_strip)
* [ESP32-DevKitC-VE User Guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html)

