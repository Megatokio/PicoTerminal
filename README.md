# ANSI Text Terminal
*based on lib kilipili*

*A wannabe-ANSI compliant VGA text terminal for the RP2040.*

This project is for an ANSI text terminal provided at a serial port of the RP2040.  
The video output is via VGA and the keyboard & mouse inputs are via USB.

Various options can be set via cmake variables.  
The description of your board is read from a `boards` header file as this is SDK standard.  

For your first own board take a look at [[https://github.com/Megatokio/kilipili/wiki/Your-first-own-VGA-Board]].


### Ansi Terminal
*lib kilipili* contains a class which provides a wannabe-ANSI-compliant text terminal. 
Here a short excerpt from the feature list in `kilipili/Graphics/ANSITerm`:  

- color (xterm compatible)
- mouse pointer support
- horizontal and vertical scroll region
- bold, inverse, italic, underlined, double width and double height 
- Latin-1 character set, one Graphics character set
- optional utf-8 encoding

### Serial port options
The serial port is set from PICO_DEFAULT_UART in the boards header file.  
The serial port uses 8N1. The Baud rate can be configured from 2400 baud to 115200 baud.  
The baud rate can be changed at runtime in the configuration screen.  
Characters are transmitted as 7 bit ASCII or 8 bit Latin-1 characters, or utf-8 encoded.  
Other character sets are possible but need your work. Your contributions are welcome.

### VGA options
VGA color pins can be any number from 1 to 16, b&w, grey, RGB color or RGBI color.  
This is configurable in the boards header file.  
CLK and DEN signals are supported but this feature has not been tested and may be broken. 
Feedback is welcome.

### Video options
The text terminal uses true color for the display.  
The amount of 'true' in color depends on the number of pins you spent for the VGA output.  
The screen size can be configured from 40x20 characters (320x240 pixels) to 128x64 characters (1024x768 pixels).  
The screen size can be changed at runtime in the configuration screen.  
The high resolutions (i think 1024x768) only display properly when built in Release mode.  

### Keyboard & Mouse options
The keyboard and a mouse can be connected via USB. You need a USB-on-the-go cable/adapter for that. If you also use a mouse you also need a hub.  
There are currently two keyboard maps available: US english and German. You are welcome to add your localization.  
The character set is Latin-1, which is the first page of Unicode and suits most western European languages.  
You are welcome to provide the glyphs (graphics) of other character sets together with a matching keyboard map.  
This is configurable with cmake setting USB_DEFAULT_KEYTABLE or in the configuration screen.  

### Audio options
Audio can be none, a buzzer, PWM, I2S or SigmaDelta. The terminal just plays a beep for char(7).  
This is configurable in the boards header file.

### Terminal settings, setup screen
You can press ctrl-alt-delete (or backspace) to enter the configurartion screen. This allows you to change the following options:

- screen size
- baud rate
- keyboard map
- enable mouse
- enable utf-8 encoding
- enable 8 bit C1 codes
- enable newline mode
- switch on or off keyboard application mode  
  possibly you only want to switch it off
- enable local echo
- set SGR mode to replace or accumulate 
- enable display of unhandled/broken codes
- save configuration to flash
- show some system info

these defaults can also be set with cmake variables:

- USB_DEFAULT_KEYTABLE
- PICO_DEFAULT_UART_BAUD_RATE
- VIDEO_DEFAULT_SCREENSIZE

### The boards.h file
For the Raspberry VGA demonstration board you can use the vgaboard.h as-is.  
For other boards or your own board you must provide your own board header file.  
This can be placed in the SDK's `boards/` directory or, in order not to modify your SDK, in the `kilipili/boards/` subfolder of this project.  
Don't forget to add it to the `.git/modules/kilipili/info/exclude` text file. **B-)**

The following #defines should be present in this file. Replace pin number etc. for your board.  
For reference and for other color setups take a look in `kilipili/boards/`.

```c
// Serial
#define PICO_DEFAULT_UART         1
#define PICO_DEFAULT_UART_TX_PIN  20
#define PICO_DEFAULT_UART_RX_PIN  21

// Video
#define VIDEO_COLOR_PIN_BASE   2
#define VIDEO_COLOR_PIN_COUNT  12
#define VIDEO_SYNC_PIN_BASE    14

#define VIDEO_PIXEL_RSHIFT  0
#define VIDEO_PIXEL_GSHIFT  4
#define VIDEO_PIXEL_BSHIFT  8
#define VIDEO_PIXEL_RCOUNT  4
#define VIDEO_PIXEL_GCOUNT  4
#define VIDEO_PIXEL_BCOUNT  4

// Audio

// #define PICO_AUDIO_NONE

// #define PICO_AUDIO_BUZZER
// #define PICO_AUDIO_BUZZER_PIN  26

// #define PICO_AUDIO_I2S
// #define PICO_AUDIO_I2S_DATA_PIN 	      26
// #define PICO_AUDIO_I2S_CLOCK_PIN_BASE  27

// #define PICO_AUDIO_PWM
// #define PICO_AUDIO_MONO_PIN   28    
// #define PICO_AUDIO_LEFT_PIN   28
// #define PICO_AUDIO_RIGHT_PIN  26
```




