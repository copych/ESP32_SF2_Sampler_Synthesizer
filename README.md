This fork provides usb midi host function to the project author Copych code. Logs and gui are disabled. To change midi input mode and enable gui check config.h. For usb midi host function to work on ESP32-S3 enable usb host by soldering pad and in Arduino IDE change USB-Mode to USB-OTG(TinyUSB).

For this fork USB midi host library code by Enudenki was used.
https://github.com/enudenki/esp32-usb-host-midi-library


# ESP32-S3 SoundFont (SF2) Sampler Synthesizer

An SF2 (SoundFont 2) based wavetable synth designed specifically for the ESP32-S3 microcontroller. This project leverages the enhanced memory capabilities of the ESP32-S3 (with PSRAM) to efficiently load and play SoundFont samples, providing a compact and powerful sampler solution. It's cheap and simple, yet powerful.

---

## Overview

The ESP32-S3 SF2 Sampler is a sampler firmware that runs exclusively on the ESP32-S3 variant due to its improved PSRAM and memory management compared to the original ESP32. It supports external DACs like the PCM5102 for high-quality audio output and uses the built-in USB hardware of the ESP32-S3 to function as a USB MIDI device. By default, the BOOT button of the DevBoard is configured to cycle through SF2 files on the current filesystem. Long press on BOOT button will switch between Flash LittleFS and SD filesystems.

<img src="./media/prototype.jpg?raw=true">

---

## Features

- **SF2 playback**: Load SoundFont2 banks up to PSRAM size.
- **Filesystem**: Runtime switch between LittleFS and fast 4-bit SD_MMC.
- **USB MIDI**: Plug-and-play MIDI device support.
- **Per-voice filters**: Optional biquad LPF (configurable in `config.h`).
- **Per-channel filters**: Optional CC#74/71-controlled LPF/resonance.
- **Effects**: Reverb (CC#91), Chorus (CC#93), Delay (CC#95).
- **MIDI control**: GM, partially GS/XG-compatible CCs, PC, RPNs, drums on ch.10, GM reset.
- **External DAC**: Works with PCM5102 and similar I2S DACs.
- **ESP32-S3 optimized**: Dual-core, PSRAM, minimal wiring.
- **Optional OLED GUI**: Use a rotary encoder and a button to navigate. 

---

## Hardware Requirements

- **ESP32-S3 microcontroller** with PSRAM (OPI PSRAM recommended)
- **External DAC** (e.g., PCM5102)
- USB connection for MIDI and power

---

## I2S DAC (PCM 5102) Pin Connections

| Signal | GPIO Pin |
|--------|----------|
| BCK    | GPIO5    |
| DTA    | GPIO6    |
| WCK    | GPIO7    |
| CS     | GND      |

These pins can be changed in config.h if needed

---


## SD CARD Pin Connections:

| Signal | GPIO Pin |
|--------|----------|
| CMD     | GPIO38  |
| CLK     | GPIO39  |
| D0     | GPIO10  |
| D1     | GPIO11  |
| D2     | GPIO12  |
| D3     | GPIO13  |
| VCC    | 3V3  |
| GND    | GND  |

These pins can be changed in config.h if needed

---
## GUI:

GUI requires U8g2 library.

| Signal | GPIO Pin |
|------------|---------------|
|   button | 14 |
| encoder pin A | 15 |
| encoder pin B | 16 |
| | |
| display SDA | 8    |
| display SCL | 9    |


These pins can be changed in config.h if needed

---
## Software Setup

### Arduino IDE Configuration

To build and upload this project using Arduino IDE, configure the following settings:

- **PSRAM**: Select **OPI PSRAM**
- **Partition Scheme**: Choose a partition with the most available SPDIFF space
- **USB Mode**: Select **TINY USB**
- **Core Debug Level**: Set to **Info** or lower (higher debug levels may block USB functionality)
- **Upload SF2 files to internal Flash**: use this plugin for Arduino IDE 2.x.x https://github.com/earlephilhower/arduino-littlefs-upload , or ths one https://github.com/earlephilhower/arduino-esp8266littlefs-plugin if you use Arduino IDE 1.x
- **Wire a microSD or SD card** to have a lot of SF2 files to choose from 

---

## Usage

1. Connect your ESP32-S3 board with PSRAM and external DAC according to the pinout above.
2. Load your preferred SF2 SoundFont files onto the device (refer to project documentation for details on loading SF2 files).
3. Connect the ESP32-S3 via USB to your computer or MIDI host.
4. The device will enumerate as a USB MIDI device, allowing you to play samples via MIDI input.

---

## Notes

- This project is **only compatible with the ESP32-S3** due to memory and PSRAM requirements.
- Using a core debug level above **Info** may interfere with USB MIDI functionality.
- Ensure your external DAC is properly powered and connected for optimal audio quality.

---

## Contributing

Contributions, issues, and feature requests are welcome! Feel free to open a pull request or issue on the GitHub repository.

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

---

## Links
- [Free SoundFonts](https://github.com/ZmeyKolbasnik/Instruments/tree/master)
- [ESP32-S3 Documentation](https://www.espressif.com/en/products/socs/esp32-s3)
- [SoundFont 2 Specification](https://en.wikipedia.org/wiki/SoundFont)
- [ESP Partition Excel calculator](https://github.com/copych/ESP32-S3_SF2_Sampler_Synthesizer/tree/main/partitions)

---

Enjoy your ESP32-S3 powered sampler!

