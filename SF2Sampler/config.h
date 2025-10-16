/*
 * ----------------------------------------------------------------------------
 * ESP32-S3 SF2 Synthesizer Firmware
 * 
 * Description:
 *   Real-time SF2 (SoundFont) compatible wavetable synthesizer with USB MIDI, I2S audio,
 *   multi-layer voice allocation, per-channel filters, reverb, chorus and delay.
 *   GM/GS/XG support is partly implemented
 * 
 * Hardware:
 *   - ESP32-S3 with PSRAM
 *   - I2S DAC output (44100Hz stereo, 16-bit PCM)
 *   - USB MIDI input
 *   - Optional SD card and/or LittleFS
 * 
 * Author: Evgeny Aslovskiy AKA Copych
 * Usb midi host functionality added by Vadims Maksimovs AKA Ledlaux
 * License: MIT
 * Repository: https://github.com/copych/ESP32-S3_SF2_Sampler_Synthesizer
 * 
 * File: config.h
 * Purpose: Main
 * ----------------------------------------------------------------------------
 */

#pragma once

// ===================== AUDIO ======================================================================================
#define   DMA_BUFFER_NUM        2     // number of internal DMA buffers
#define   DMA_BUFFER_LEN        64    // length of each buffer in samples
#define   CHANNEL_SAMPLE_BYTES  2     // can be 1, 2, 3 or 4 (2 and 4 only supported yet)
#define   SAMPLE_RATE           44100

// ===================== MIDI =======================================================================================
#define   USE_USB_MIDI_DEVICE   1     // definition: the synth appears as a USB MIDI Device "S3 SF2 Synth"
#define   USE_MIDI_STANDARD     2     // definition: the synth receives MIDI messages via serial 31250 bps
#define   USE_USB_HOST          3
#define   MIDI_IN_DEV           USE_USB_HOST // select the appropriate (one of the above) 
#define   NUM_MIDI_CHANNELS		16

// ===================== SYNTHESIZER ================================================================================
#define MAX_VOICES 19 // for now 20 is max for per-channel filtering + chorus + reverb
#define MAX_VOICES_PER_NOTE 2
#define PITCH_BEND_CENTER 0

//#define ENABLE_IN_VOICE_FILTERS       // comment this out to disable voice SF2 filters
#define ENABLE_REVERB                 // comment this out to disable reverb 
#define ENABLE_CHORUS                 // comment this out to disable chorus
#define ENABLE_CH_FILTER_M           // uncomment this line to mono per-channel filtering before stereo split
//#define ENABLE_DELAY                  // comment this out to disable delay
//#define ENABLE_OVERDRIVE             // comment this out to disable overdrive effect
//#define ENABLE_CH_FILTER             // not recommended, use ENABLE_CH_FILTER_M instead 

#define CH_FILTER_MAX_FREQ 12000.0f
#define CH_FILTER_MIN_FREQ 50.0f
#define FILTER_MAX_Q 7.0f

static const char* SF2_PATH = "/"; 
#define DEFAULT_CONFIG_FILE "/default_config.bin"
// ===================== MIDI PINS ==================================================================================
#define MIDI_IN         15      // if USE_MIDI_STANDARD is selected as MIDI_IN, this pin receives MIDI messages

// ===================== I2S PINS ===================================================================================
#define I2S_BCLK_PIN    5       // I2S BIT CLOCK pin (BCL BCK CLK)
#define I2S_DOUT_PIN    6       // MCU Data Out: connect to periph. DATA IN (DIN D DAT)
#define I2S_WCLK_PIN    7       // I2S WORD CLOCK pin (WCK WCL LCK)
#define I2S_DIN_PIN     -1      // MCU Data In: connect to periph. DATA OUT (DOUT D SD)

// ===================== SD MMC PINS ================================================================================
// ESP32S3 allows almost any GPIOs for any particular needs
#define SDMMC_CMD 38
#define SDMMC_CLK 39
#define SDMMC_D0  10
#define SDMMC_D1  11
#define SDMMC_D2  12
#define SDMMC_D3  13

// ===================== GUI SETTINGS ==========================================================================
// #define ENABLE_GUI

#ifdef ENABLE_GUI
  #define DISPLAY_CONTROLLER SH1106
  // #define DISPLAY_CONTROLLER SSD1306

  #define ACTIVE_STATE  LOW   // LOW = switch connects to GND, HIGH = switch connects to 3V3

  #define BTN0_PIN 	14
  #define ENC0_A_PIN 	15
  #define ENC0_B_PIN 	16

  #define DISPLAY_SDA 8 // SDA GPIO
  #define DISPLAY_SCL 9 // SCL GPIO

  #define DISPLAY_W 128
  #define DISPLAY_H 64
  #define DISPLAY_ROTATE 0 // can be 0, 90, 180 or 270
#endif

// ===================== RGB LED ====================================================================================
// #define ENABLE_RGB_LED
// #define FASTLED_INTERNAL                  // remove annoying pragma messages


// ===================== DEBUGGING ==================================================================================

// #define TASK_BENCHMARKING

 




// !!!!!!!!!!!!!=======  DO NOT CHANGE  =======!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// U8G2 CONSTRUCTOR MACROS
#if (DISPLAY_ROTATE == 180)
  #define U8_ROTATE U8G2_R2
#elif (DISPLAY_ROTATE == 90)
  #define U8_ROTATE U8G2_R1
#elif (DISPLAY_ROTATE == 270)
  #define U8_ROTATE U8G2_R3
#else
  #define U8_ROTATE U8G2_R0
#endif

#define W_H_DIV X
#define _U8_CONCAT(ctrl, w, div, h) U8G2_ ## ctrl ## _ ## w ## div ## h ## _NONAME_F_HW_I2C
#define U8_CONCAT(ctrl, w, div, h) _U8_CONCAT(ctrl, w, div, h)
#define U8_OBJECT U8_CONCAT(DISPLAY_CONTROLLER, DISPLAY_W, W_H_DIV, DISPLAY_H)

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#if ACTIVE_STATE == LOW
  #define SIG_INPUT_MODE    INPUT_PULLUP  
#else
  #define SIG_INPUT_MODE    INPUT_PULLDOWN  
#endif
