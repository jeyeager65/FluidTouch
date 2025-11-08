# FluidTouch

**A touchscreen CNC controller for FluidNC-based machines**

FluidTouch provides an intuitive 800×480 touchscreen interface for controlling CNC machines running FluidNC firmware. Built on the Elecrow CrowPanel 7" ESP32-S3 HMI display with hardware-accelerated graphics and a responsive LVGL-based UI.

![Version](https://img.shields.io/badge/version-0.9.1-blue)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-green)
![License](https://img.shields.io/badge/license-MIT-orange)

![FluidTouch Status](./docs/images/status-tab.png)

## ✨ Key Features

- **Real-time Machine Control** - Monitor position, state, feed/spindle rates with live updates
- **Intuitive Jogging** - Button-based and analog joystick interfaces with configurable step sizes
- **Touch Probe Operations** - Automated probing with customizable parameters
- **Macro Support** - Configure and store up to 9 file-based macros
- **Multi-Machine Support** - Store and switch between up to 4 different CNC configurations
- **Terminal** - For running custom commands and debugging
- **WiFi Connectivity** - WebSocket connection to FluidNC

---

## 🚀 Quick Start

### Option 1: Web Installer (Recommended)

The easiest way to install FluidTouch is using our web-based installer:

1. **Visit:** [FluidTouch Web Installer](https://jeyeager65.github.io/FluidTouch/)
2. **Click:** "Install FluidTouch" button
3. **Connect:** Your ESP32-S3 device via USB-C
4. **Done!** Firmware flashes automatically in 30-60 seconds

**Requirements:** Chrome, Edge, or Opera browser (Web Serial API support)

### Option 2: Pre-built Binaries

Download the latest firmware from [Releases](https://github.com/jeyeager65/FluidTouch/releases) and flash using:

- [ESP Web Flasher](https://esp.huhn.me/) (browser-based)
- [ESPHome Flasher](https://github.com/esphome/esphome-flasher) (desktop app)
- esptool.py: `esptool.py --chip esp32s3 --port COM6 write_flash 0x10000 firmware.bin`

### Option 3: Build from Source

Refer to the **[Development Guide](./docs/development.md)** for additional instructions.

---

## 🖥️ Compatible Hardware

**Elecrow CrowPanel 7" Basic ESP32-S3 HMI Display**
- ESP32-S3-WROOM-1-N4R8 (4MB Flash + 8MB PSRAM)
- 800×480 RGB TFT LCD
- GT911 Capacitive Touch
- [Product Page](https://www.elecrow.com/esp32-display-7-inch-hmi-display-rgb-tft-lcd-touch-screen-support-lvgl.html)

---

## 📖 Documentation

Detailed documentation is available in the [`docs/`](./docs/) folder:

- **[User Interface Guide](./docs/ui-guide.md)** - Complete UI walkthrough with screenshots
- **[Usage Instructions](./docs/usage.md)** - Operating instructions and workflows
- **[Configuration](./docs/configuration.md)** - WiFi setup, machine configuration, and settings
- **[Development Guide](./docs/development.md)** - Building, debugging, and contributing

---

## 🔧 First-Time Setup

1. Power on the device
2. On the "Select Machine" screen, click the Edit button.
3. Click the "Add" button to add a new machine:
   - Name
   - Connection (currently only Wireless is supported)
   - WiFi SSID and Password (machine-specific)
   - FluidNC IP address or hostname (e.g. 192.168.0.1 or fluidnc.local)
   - WebSocket port (default: 81)
     - Port 82 is used by WebUI v3 and can also be used.  FluidNC only allows one connection at a time but it will switch cleanly back and forth.
4. Click the "Save" button.
5. Click the "Done" button.
6. Click on the created machine.

---

## 🤝 Contributing

Contributions are welcome! Please read our [Development Guide](./docs/development.md) for:
- Code architecture and design patterns
- Building and testing
- Submitting pull requests

---

## 📝 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

- **[FluidNC](https://github.com/bdring/FluidNC)** - CNC control firmware
- **[LVGL](https://lvgl.io/)** - Embedded graphics library
- **[LovyanGFX](https://github.com/lovyan03/LovyanGFX)** - Hardware-accelerated display driver

---

## 📧 Support

- **Issues:** [GitHub Issues](https://github.com/jeyeager65/FluidTouch/issues)
