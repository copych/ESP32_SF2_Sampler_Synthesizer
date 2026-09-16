# ESP32 USB Host MIDI Library (Omocha)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**An easy-to-use library that turns your ESP3S3 into a USB Host MIDI, inspired by the Japanese concept of "OMOCHA" — a toy.**

Please also refer to my project page on [Hackster.io](https://www.hackster.io/ndenki/esp32-usb-host-midi-library-032d95).

Many people probably feel the same — USB MIDI devices often feel like silent boxes without a PC. With traditional serial MIDI, connecting to a synth produces sound right away. But USB MIDI isn't that simple. This library solves that problem.

By acting as a USB Host, an ESP3S3 can receive USB MIDI data, process it, and send out converted serial MIDI to a synthesizer, enabling sound generation without a PC. This project was created to make that process as simple and playful as picking up a new toy.

## The "Omocha" Philosophy: Start with Play

This library is designed for everyone, regardless of background or skill level. It's an environment where you can freely explore and have fun with USB MIDI without worrying about complexity or performance.

Like a child who eventually understands the deeper workings of their toys, this library is your gateway. Start with something simple and playful, and when you're ready to dive deeper, the path to more advanced tools like the ESP-IDF awaits.

## ✨ Key Features

- **Simplicity First: Just 6 Lines to Start**
  This library was built with ease of use as the top priority. It handles the complexities of USB and RTOS, so you can get started with just a few lines of code.
  ```cpp
  #include "UsbMidi.h"

  UsbMidi usbMidi;

  void onMidiMessage(const uint8_t (&data)[4]) {
    // USB-MIDI message received. See examples for more info.
  }

  void setup() {
    usbMidi.onMidiMessage(onMidiMessage);
    usbMidi.begin();
  }

  void loop() {
    usbMidi.update();
  }
  ```

- **Go PC-less: Standalone Operation**
  Turn your USB MIDI gear into a standalone synthesizer. This library empowers your ESP3S3 to act as the "brain," connecting your controller directly to a sound module or synthesizer without needing a computer.

- **Built-in Wi-Fi Debugging**
  Using the ESP3S3 for USB MIDI can sometimes occupy the built-in USB serial port. To solve this, the library includes an optional Wi-Fi logger, allowing you to see debug messages wirelessly.

- **A Gateway to Advanced Development**
  To achieve simplicity, some trade-offs were made. For example, incoming MIDI messages are handled in the main task for convenience rather than maximum performance. This library is designed to be a friendly stepping stone, not the final destination for high-performance applications.

## 🔌 What You'll Need

### Hardware
- **ESP3S3 Development Board**
  - M5 series devices equipped with the ESP32-S3 (such as the M5Capsule, CoreS3, and AtomS3 Lite)
- **USB MIDI Device**: A MIDI keyboard, controller, etc.
- **Optional M5Stack Units**:
    - [Unit Synth](https://docs.m5stack.com/en/unit/Unit-Synth): A compact synthesizer module used in the `03_synth.ino` example. A connection diagram for the Unit Synth will be added later.
    - [Unit MIDI](https://docs.m5stack.com/en/unit/Unit-MIDI): A serial MIDI I/O unit for connecting to traditional MIDI hardware (e.g., DIN-5 or 3.5mm jack devices).
- PC (Windows/Linux) / Mac
  - The author has tested the operation on Windows.
- Requires a 5V power source (like a power bank).

### Software
- **Arduino IDE** or **PlatformIO**
- **Python 3.x**: Only required if you want to use the developer utility scripts.

---

## 🚀 Getting Started

### For Arduino IDE Users

The most reliable way to use this library is to manually copy the necessary files into your sketch folder. This method is guaranteed to work for everyone.

1.  **Download and Unzip the Project**:
    -   Go to the main page of this GitHub repository.
    -   Click the green `<> Code` button and select `Download ZIP`.
    -   Save the ZIP file to your computer and unzip it.

2.  **Create and Prepare Your Sketch**:
    -   Open the Arduino IDE.
    -   Create a new sketch and save it. This will create a folder for your project.

3.  **Copy Library Files**:
    -   Open the unzipped project folder you downloaded.
    -   Navigate into the `src` folder.
    -   Copy **all files** inside the `src` folder.
    -   Paste these files directly into your new sketch folder.

4.  **Load an Example**:
    -   In the unzipped project folder, navigate to the `examples` folder and open an example `.ino` file (e.g., `01_hello.ino`) with any text editor.
    -   Copy the entire content of the example file.
    -   Go back to the Arduino IDE. Paste the copied code into your main sketch window, replacing all of its default content.
    -   **Note:** Although the Arduino IDE opens all copied files in tabs, please make sure you are working within the main `.ino` file for your sketch.

5.  **Select Board and Upload**:
    -   In the `Tools` menu, select your ESP3S3 board and port.
    -   Click the "Upload" button (the right-arrow icon).

6.  **For a More Advanced Setup**:
    -   You are now ready to go! For more elegant installation methods, such as accessing examples directly from the IDE menu, see the **Developer Utilities** section below.

### For PlatformIO Users

This project can be used as a standalone project or added to an existing workspace. It starts with a default `main.ino` which is a copy of the `01_hello` example.

1.  **Clone the Repository**:
    ```bash
    git clone https://github.com/enudenki/esp32-usb-host-midi-library.git
    cd esp32-usb-host-midi-library
    ```

2.  **Upload**:
    Click the "Upload and Monitor" button in the PlatformIO toolbar, or run `pio run -t upload -t monitor` in the terminal.

3.  **Switching Examples**:
    To try other examples, please see the **Example-Picker Script** in the **Developer Utilities** section.

---

## 📖 Examples

Here is a brief overview of the available examples to help you get started.

-   **`00_tutorial.ino`**
    This is the minimal code shown in the introduction, intended for documentation purposes. It initializes the library but does nothing with the MIDI data. **Please use `01_hello.ino` as a practical starting point.**

-   **`01_hello.ino`**
    Receives MIDI messages from a connected device and prints them to the Serial Monitor. A great starting point for understanding incoming data.

-   **`02_wifi_debug.ino`**
    This example is designed for ESP32S3 boards that have only one USB port. When that port is used as a USB Host for MIDI, you can no longer use it for Serial communication with your PC. This sketch demonstrates how to work around that limitation by sending debug messages over Wi-Fi.

-   **`03_synth.ino`**
    A practical application that turns your USB MIDI keyboard into a standalone synthesizer using the M5UnitSynth.

-   **`04_timer.ino`**
    Demonstrates the basic usage of the included non-blocking timer to perform actions at regular intervals.

-   **`05_timer_advanced.ino`**
    Shows how to run multiple independent timers simultaneously, each at a different speed. It demonstrates a clear and effective way to create rhythmic events using toggle state management and count-based triggering, while keeping the code simple and approachable.

-   **`06_midi_out.ino`**
    Shows how to send MIDI messages from the ESP32S3. MIDI Out messages are queued and sent in the background by the `update()` function, so it's important not to block the main task.

-   **`10_my_demo.ino`**
    This is the actual source code used in the project's demonstration video.

-   **`20_wifi_debug_safe.ino`**
    An alternative Wi-Fi debugging example that shows how to set credentials from within your code using a function. This is a safer practice than editing the `wifi_logger.h` file directly.

-   **`30_m5capsule.ino`**
    This is an example using the built-in battery of the M5 Capsule.
---

## 🛠️ Developer Utilities

This project includes handy Python scripts in the `utility` folder to make development easier. **Using these scripts requires Python 3.x.**

> Please move to the `utility` folder and run the Python script.

### A Smarter Way to Install (for Arduino IDE)

While manual copy-pasting is reliable, you can use the provided Python script to create a perfectly formatted ZIP file for the Arduino IDE's "Add .ZIP Library..." feature.


**The biggest advantage:** Once installed this way, you can access all examples directly from the `File > Examples` menu in the Arduino IDE, with no need for copy-pasting!

1.  **Run the script:**
    ```bash
    python create_arduino_lib.py
    ```
    This will create a distributable ZIP file in a new `release` directory.

2.  **Install the ZIP in Arduino IDE:**
    -   Open the Arduino IDE.
    -   Navigate to `Sketch > Include Library > Add .ZIP Library...`.
    -   Select the newly created ZIP file.

3.  **Restart the Arduino IDE:**
    This step is crucial for the examples to appear in the menu.

#### Locating and Managing the Installed Library
Libraries installed via ZIP don't appear in the Library Manager, so they must be managed manually.

1.  **Find your Sketchbook Location:** In the Arduino IDE, go to `File > Preferences` to find the path to your "Sketchbook location".
2.  **Navigate to the Folder:** Open that folder on your computer. Inside it, you will find a `libraries` folder.
3.  **Manage the Library:** The `ESP3S3_USB_MIDI_Omocha` folder is located inside `libraries`. You can now:
    -   **Edit the code:** Open the files inside this folder to make modifications.
    -   **Uninstall:** Delete the entire `ESP3S3_USB_MIDI_Omocha` folder.

Remember to restart the Arduino IDE after making changes.

### Wireless Debugger (Wi-Fi Logger)

This utility is compatible with both Arduino IDE and PlatformIO.

1.  **Enable the Feature**: In `src/wifi_logger.h`, change `#define DEBUG_WIFI 0` to `1`.
2.  **Set Credentials**: There are a few ways to set your Wi-Fi credentials:
    -   **Easy:** Set them from within your sketch using a function. See the Wi-Fi examples for details.
    -   **Convenient:** To avoid retyping credentials for every sketch, you can directly edit the `WIFI_SSID` and `WIFI_PASSWORD` definitions in `src/wifi_logger.h`.
    -   **Advanced:** For security-conscious users, run the `set_wifi.py` script to keep credentials separate from your source code.
3.  **Receive Logs**: On your PC, run `python wifi_logger.py` to view the debug output. For Windows users, you can simply double-click `wifi_logger.bat`.
    -   When the ESP3S3 successfully connects to your Wi-Fi, you will see a message like this in your terminal:
        ```
        Logger initialized. IP: 192.168.1.123
        ```

> **Note on Disabling Wi-Fi:**
> When you are done debugging, set `#define DEBUG_WIFI 0` to enhance security and reduce firmware size.

> **Wi-Fi Connection Issues:**
> If you're having trouble connecting, it's worth checking your firewall settings, as well as your network configuration.

### Example-Picker Script (for PlatformIO)

This script simplifies switching between examples in a PlatformIO project.

-   **List all available examples:** `python pick_example.py --list`
-   **Switch to a different example:** `python pick_example.py 3 --run`

---

## 📚 API Reference (`UsbMidi` Class)

| Method                        | Description                                                 |
|:------------------------------|:------------------------------------------------------------|
| `UsbMidi(Stream* debugSerial)`| Constructor. Pass a `Stream` object (e.g., `&Serial`) for debug output. |
| `void begin()`                | Initializes the USB host functionality. Call in `setup()`.   |
| `void update()`               | Handles all USB events. Must be called continuously in `loop()`.|
| `void onMidiMessage(callback)`| Registers a callback function for incoming MIDI messages.   |
| `void onDeviceConnected(callback)`| Registers a callback for when a MIDI device is connected. |
| `void onDeviceDisconnected(callback)`| Registers a callback for when a MIDI device is disconnected.|
| `bool noteOn(ch, note, vel)`  | Sends a Note On message on the specified channel.           |
| `bool noteOff(ch, note, vel)` | Sends a Note Off message on the specified channel.          |
| `bool controlChange(ch, cc, val)`| Sends a Control Change message.                           |
| `bool programChange(ch, prog)`| Sends a Program Change message.                             |
| `bool sendMidiMessage(msg, size)`| Sends a raw 4-byte USB-MIDI packet.                         |

Here is the documentation for the USB MIDI format: [USB MIDI Format](usb_midi_format.md)


## 📂 Project Structure
```
.
├── src/                # Library source code and the main sketch
├── examples/           # A collection of sketches demonstrating usage
├── utility/            # Python scripts to aid development
├── library.properties  # Arduino library metadata
├── platformio.ini      # PlatformIO configuration file
└── ...
```
> **A Note on `platformio.ini`:** This file provides a sample configuration. You should review and adapt it to your specific board and environment.

## 🙏 Acknowledgements

This project includes the M5Unit-Synth library, generously provided by M5Stack, which powers the synthesizer example (`03_synth.ino`). Their excellent work makes it incredibly easy to turn this project into a real, physical instrument.

The M5Unit-Synth library is distributed under the terms of the MIT License. For full license details, please see the `src/M5UnitSynthLicense.txt` file.

You can find the original repository for the M5Unit-Synth library [here](https://github.com/m5stack/M5Unit-Synth).

It is also inspired by and incorporates logic from the [esp32-usb-host-demos](https://github.com/touchgadget/esp32-usb-host-demos) repository by touchgadget.

## 📄 License
This project is released under the [MIT License](LICENSE).
