/*
* ESP32-S3 SF2 Sampler
* An SF2 (SoundFont 2) based synthesizer designed specifically for the ESP32-S3 microcontroller.
* This project leverages the enhanced memory capabilities of the ESP32-S3 (with PSRAM)
* to efficiently load and play SoundFont samples, providing a compact and powerful sampler solution.
* The ESP32-S3 SF2 Sampler is a sampler firmware that runs exclusively on the ESP32-S3 variant
* due to its improved PSRAM and memory management compared to the original ESP32. 
* It supports external DACs like the PCM5102 for high-quality audio output and 
* uses the built-in USB hardware of the ESP32-S3 to function as a USB MIDI device.
* GM/GS/XG support is partlially implemented (i.e. with 2MBGMGS.sf2 bank).
*
* Libraries used:
* Arduino MIDI library https://github.com/FortySevenEffects/arduino_midi_library
* ESP32 usb midi host library by Enudenki https://github.com/enudenki/esp32-usb-host-midi-library
* Optional. Using RGB LEDs requires FastLED library https://github.com/FastLED/FastLED

*
* (c) Copych 2025, License: MIT https://github.com/copych/SF2_Sampler?tab=MIT-1-ov-file#readme
* This fork by Vadims Maksimovs (Ledlaux) adds USB MIDI host functionality to the original project by Evgeny Aslovskiy (Copych).

* More info:
* https://github.com/copych/SF2_Sampler
*/
#pragma packed(16)
#pragma GCC optimize ("O3")
#pragma GCC optimize ("fast-math")
#pragma GCC optimize ("unsafe-math-optimizations")
#pragma GCC optimize ("no-math-errno")

static const char* TAG = "Main";


#define FORMAT_LITTLEFS_IF_FAILED

#include <Arduino.h>
#include "esp_task_wdt.h"
#include <float.h>
#include "config.h"
#include "misc.h" 
#include "esp_log.h"
#include <FS.h>
#include "SD_MMC.h"
#include <LittleFS.h>
#include <MIDI.h>
#include "synth.h"
#include "SF2Parser.h"
#include "adsr.h"
#include "voice.h"
#include "SynthState.h"

#ifdef ENABLE_RGB_LED
    #include "rgb_led.h"
#endif

#if MIDI_IN_DEV == USE_USB_MIDI_DEVICE
    #include "src/usbmidi/src/USB-MIDI.h"
#endif

#if MIDI_IN_DEV == USE_USB_HOST
#include "src/usbhost/src/SimpleTimer.h"
#include "src/usbhost/src/UsbMidi.h"
#endif

#include "i2s_in_out.h" 

// tasks for Core0 and Core1
TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;

int Voice::usage; // counts voices internally


// ========================== MIDI Instance ===============================================================================================
#if MIDI_IN_DEV == USE_MIDI_STANDARD
    MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);
#endif

#if MIDI_IN_DEV == USE_USB_MIDI_DEVICE
    USBMIDI_CREATE_INSTANCE(0, MIDI); 
#endif

#if MIDI_IN_DEV == USE_USB_HOST
    UsbMidi usbMidi;
#endif


// ========================== Global devices ===============================================================================================
#ifdef ENABLE_CHORUS
    #include "fx_chorus.h"
    FxChorus    DRAM_ATTR   chorus;
#endif

#ifdef ENABLE_REVERB
    #include "fx_reverb.h"
    FxReverb    DRAM_ATTR   reverb;
#endif

#ifdef ENABLE_DELAY
    #include "fx_delay.h"
    FxDelay     DRAM_ATTR   delayfx;
#endif



I2S_Audio   AudioPort(I2S_Audio::MODE_OUT);
SF2Parser   parser(SF2_PATH);
Synth       synth(parser);

// ========================== Synth Settings ===================================================================================
SynthState state {
  synth.channels
#ifdef ENABLE_REVERB
  , reverb
#endif
#ifdef ENABLE_DELAY
  , delayfx
#endif
#ifdef ENABLE_CHORUS
  , chorus
#endif
};

// ========================== GUI ==============================================================================================
#ifdef ENABLE_GUI
    #include "TextGui.h"
    TextGUI gui(synth, state);
#endif

// ========================== MIDI handlers ===============================================================================================
#if MIDI_IN_DEV == USE_MIDI_STANDARD || MIDI_IN_DEV == USE_USB_MIDI_DEVICE

void handleNoteOn(byte ch, byte note, byte vel) {
#ifdef ENABLE_RGB_LED
    triggerLedFlash();
#endif
    //synth.printState();
    synth.noteOn(ch-1, note, vel);
}

void handleNoteOff(byte ch, byte note, byte vel) {
    synth.noteOff(ch-1, note);
}

void handlePitchBend(byte ch, int bend) {
    synth.pitchBend(ch-1, bend);
}

void handleControlChange(byte ch, byte control, byte value) {
    synth.controlChange(ch-1, control, value);
}

void handleProgramChange(uint8_t ch, uint8_t program) {
//    ESP_LOGI("MIDI", "Program change on channel 0%u → program %u", ch-1, program);
    synth.programChange(ch-1, program);
}

void handleSystemExclusive( uint8_t* data, size_t len) {
    synth.handleSysEx( data,  len); 
}
#endif
// ========================== USB MIDI HOST Callback ===========================================
#if MIDI_IN_DEV == USE_USB_HOST
void handleMidiMessage(const uint8_t (&data)[4]) {
    auto cin = static_cast<MidiCin>(data[0] & 0x0F);
    uint8_t channel = (data[1] & 0x0F) + 1; // channels 1-16

    switch (cin) {
        case MidiCin::NOTE_ON: {
            uint8_t note = data[2];
            uint8_t vel  = data[3];
            synth.noteOn(channel, note, vel);
            break;
        }

        case MidiCin::NOTE_OFF: {
            uint8_t note = data[2];
            synth.noteOff(channel, note);
            break;
        }

        case MidiCin::CONTROL_CHANGE: {
            uint8_t control = data[2];
            uint8_t value   = data[3];
            synth.controlChange(channel, control, value);
            break;
        }

        case MidiCin::PROGRAM_CHANGE: {
            uint8_t program = data[2];
            synth.programChange(channel, program);
            break;
        }

        default:
            break;
    }
}
#endif

#ifdef TASK_BENCHMARKING
// globals to be unsafely shared between tasks
    uint32_t DRAM_ATTR t0,t1,t2,t3,t4,t5,t6;
    uint32_t DRAM_ATTR dt1,dt2,dt3,dt4,dt5,dt6;
    uint32_t DRAM_ATTR total_render = 0;
    uint32_t DRAM_ATTR total_write  = 0;
#endif

    volatile uint32_t DRAM_ATTR frame_count  = 0;
    float DRAM_ATTR blockL[DMA_BUFFER_LEN];
    float DRAM_ATTR blockR[DMA_BUFFER_LEN];


// ========================== Core 0 Task 1 ===============================================================================================
// Core0 task -- AUDIO
static void IRAM_ATTR audio_task(void *userData) {
    vTaskDelay(20); 
 //   ESP_LOGI(TAG, "Starting Task1");

    while (true) {
        
#ifdef TASK_BENCHMARKING
        t0 = esp_cpu_get_cycle_count();
#endif


        synth.renderLRBlock(blockL, blockR);


#ifdef TASK_BENCHMARKING
        t1 = esp_cpu_get_cycle_count();
#endif


        AudioPort.writeBuffers(blockL, blockR);


#ifdef TASK_BENCHMARKING
        t2 = esp_cpu_get_cycle_count();

        total_render += (t1 - t0);
        total_write  += (t2 - t1);
#endif

        frame_count++;
    }
}


// ========================== Core 1 Task 2 ===============================================================================================
static void IRAM_ATTR control_task(void *userData) { 
    vTaskDelay(50);
 //   ESP_LOGI(TAG, "Starting Task2");
    
    while (true) { 
    #if MIDI_IN_DEV == USE_MIDI_STANDARD || MIDI_IN_DEV == USE_USB_MIDI_DEVICE
    MIDI.read();
#elif MIDI_IN_DEV == USE_USB_HOST
    usbMidi.update();
#endif
   
        synth.updateScores();
#ifdef ENABLE_RGB_LED
        updateLed();
#endif
        vTaskDelay(1);
        taskYIELD();

#ifdef ENABLE_GUI
        if (__builtin_expect((gui_blocker == 0), 1)) {
            // Read GUI input
            gui.encA = digitalRead(ENC0_A_PIN);
            gui.encB = digitalRead(ENC0_B_PIN);
            gui.btnState = digitalRead(BTN0_PIN);
            
            gui.process();
        } else {
            gui_blocker--;
            if (gui_blocker < 0) { gui_blocker = 0; }
        }
#endif
        
        if (frame_count >= 64) {

#ifdef TASK_BENCHMARKING
            uint32_t avg_render = total_render / frame_count;
            uint32_t avg_write  = total_write  / frame_count;

         //   ESP_LOGI(TAG, "Avg cycles over %u frames: render = %u, write = %u",
                     frame_count, avg_render, avg_write);

            total_render = 0;
            total_write  = 0;
#endif
            synth.updateActivity();
            frame_count  = 0;
        }

    }
}

#ifdef ENABLE_GUI
// ========================== Core 1 Task 3 ===============================================================================================
static void IRAM_ATTR gui_task(void *userData) { 
    vTaskDelay(50);
//    ESP_LOGI(TAG, "Starting Task3");
    
    while (true) {
        if (gui_blocker == 0) {
            gui.draw();
        }
        taskYIELD();
    }

}
#endif

// ========================== SETUP ===============================================================================================
void setup() {
    if (!psramFound()) {
  //      ESP_LOGE(TAG, "PSRAM not found!");
        vTaskDelay(10);
        while(true);
    }
    btStop(); 

#if MIDI_IN_DEV == USE_USB_MIDI_DEVICE
    // Set USB Device descriptors
    USB.VID(0x1209);
    USB.PID(0x1304);
    USB.productName("S3 SF2 Synth");
    USB.manufacturerName("copych");
    USB.usbVersion(0x0200);
    USB.usbClass(TUSB_CLASS_AUDIO);
    USB.usbSubClass(0x00);
    USB.usbProtocol(0x00);
    USB.usbAttributes(0x80);

    USB.begin();
    delay(50);

    //  ESP_LOGI(TAG, "USB MIDI Device started");
#endif


#if MIDI_IN_DEV == USE_MIDI_STANDARD || MIDI_IN_DEV == USE_USB_MIDI_DEVICE

    MIDI.begin(MIDI_CHANNEL_OMNI);
    MIDI.setHandleNoteOn(handleNoteOn);
    MIDI.setHandleNoteOff(handleNoteOff);
    MIDI.setHandlePitchBend(handlePitchBend);
    MIDI.setHandleControlChange(handleControlChange);    
    MIDI.setHandleProgramChange(handleProgramChange);
    MIDI.setHandleSystemExclusive(handleSystemExclusive);

    delay(800);
    //   ESP_LOGI(TAG, "Standard MIDI started");
#endif

#if MIDI_IN_DEV == USE_USB_HOST
    usbMidi.onMidiMessage(handleMidiMessage);

    usbMidi.onDeviceConnected([]() {
    //    ESP_LOGI(TAG, "USB MIDI device connected");
    });

    usbMidi.onDeviceDisconnected([]() {
    //    ESP_LOGI(TAG, "USB MIDI device disconnected");
    });

    usbMidi.begin();
#endif

  //  SDMMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_D0, SDMMC_D1, SDMMC_D2, SDMMC_D3)
    SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_D0, SDMMC_D1, SDMMC_D2, SDMMC_D3);
    if (!SD_MMC.begin()) {
//        ESP_LOGE(TAG, "SD init failed");
    } else {
//        ESP_LOGE(TAG, "SD initialized");
    }

    if (!LittleFS.begin()) {
 //       ESP_LOGE(TAG, "LittleFS init failed");
    } else {
 //       ESP_LOGE(TAG, "LittleFS initialized");
    }


#ifdef ENABLE_GUI
    gui.begin();
    gui.busyMessage( "Synth Loading...");
//    ESP_LOGI(TAG, "GUI splash");
#endif

#ifdef ENABLE_REVERB
    reverb.init();
 //   ESP_LOGI(TAG, "Reverb FX started");
#endif

#ifdef ENABLE_DELAY
    delayfx.init();
 //   ESP_LOGI(TAG, "Delay FX started");
#endif
 

    synth.begin();
 //   ESP_LOGI(TAG, "Synth is starting");

#ifdef ENABLE_GUI
    gui.startMenu();
 //  ESP_LOGI(TAG, "GUI started");
#endif


    AudioPort.init(I2S_Audio::MODE_OUT);
  //  ESP_LOGI(TAG, "I2S Audio port started");

#ifdef ENABLE_RGB_LED
    setupLed();
  //  ESP_LOGI(TAG, "RGB LED started");
#endif


    xTaskCreatePinnedToCore( audio_task, "SynthTask", 5000, NULL, 8, &Task1, 0 );
    xTaskCreatePinnedToCore( control_task, "ControlTask", 5000, NULL, 8, &Task2, 1 );

#ifdef ENABLE_GUI
    xTaskCreatePinnedToCore( gui_task, "GUITask", 5000, NULL, 5, &Task3, 1 );
#endif

    vTaskDelay(30);

  //  ESP_LOGI(TAG, "SF2 Synth ready");
}


// ====================== LOOP ================= KILL IT OR NOT =========================================================
void loop() {
    vTaskDelete(NULL);
}




