// Copyright (c) 2024 - 2025 kio@little-bat.de
// BSD-2-Clause license
// https://opensource.org/licenses/BSD-2-Clause

#include "AudioController.h"
#include "Devices/Preferences.h"
#include "Dispatcher.h"
#include "Graphics/AnsiTerm.h"
#include "Graphics/Pixmap_wAttr.h"
#include "USBHost/USBKeyboard.h"
#include "USBHost/hid_handler.h"
#include "Video/FrameBuffer.h"
#include "Video/MousePointer.h"
#include "Video/VideoController.h"
#include "common/cdefs.h"
#include "common/cstrings.h"
#include "malloc.h"
#include "utilities/LoadSensor.h"
#include "utilities/utilities.h"
#include <pico/printf.h>
#include <pico/stdio.h>

#if PICO_STDIO_USB
  #error "PICO_STDIO_USB must be OFF"
#endif

#if !PICO_STDIO_UART
  #error "PICO_STDIO_UART must be ON"
#endif

#if defined VIDEO_SUPPORT_400x300_A1W8_RGB && !VIDEO_SUPPORT_400x300_A1W8_RGB
  #error "VIDEO_SUPPORT_400x300_A1W8_RGB must be ON"
#endif

#ifndef DEFAULT_VGA_MODE
  #define DEFAULT_VGA_MODE vga_mode_640x480_60
#endif

#ifndef USB_DEFAULT_KEYTABLE
  #define USB_DEFAULT_KEYTABLE key_table_ger
#endif

#ifndef VIDEO_SCANLINE_BUFFER_SIZE
  #define VIDEO_SCANLINE_BUFFER_SIZE 4
#endif

#ifndef PICO_DEFAULT_UART_BAUD_RATE
  #define PICO_DEFAULT_UART_BAUD_RATE 9600
#endif


using namespace kio;
using namespace kio::Graphics;
using namespace kio::Video;
using namespace kio::Audio;
using HidKeyTable = kio::USB::HidKeyTable;

static constexpr char XON	 = 17;
static constexpr char XOFF	 = 19;
static constexpr int  nochar = -1;

struct Settings
{
	static constexpr uint MAGIC = 0x0123afd3;

	uint  magic			   = 0xffffffffu;
	uint8 baud_rate_idx	   = 0xff;
	uint8 vga_mode_idx	   = 0xff;
	uint8 keyboard_idx	   = 0xff;
	bool  enable_mouse	   = 0xff;
	bool  auto_wrap		   = 0xff;
	bool  application_mode = 0xff;
	bool  utf8_mode		   = 0xff;
	bool  c1_codes_8bit	   = 0xff;
	bool  newline_mode	   = 0xff;
	bool  local_echo	   = 0xff;
	bool  sgr_cumulative   = 0xff;
	bool  log_unhandled	   = 0xff;
};

static Settings settings;

static constexpr const VgaMode* vga_modes[] = {&vga_mode_320x240_60, &vga_mode_400x300_60, &vga_mode_512x384_60,
											   &vga_mode_640x480_60, &vga_mode_800x600_60, &vga_mode_1024x768_60,
											   &vga_mode_640x384_60};

static constexpr uint baud_rates[] = {2400, 4800, 9600, 19200, 38400, 57600, 115200};

static constexpr const HidKeyTable* keyboards[] = {&USB::key_table_us, &USB::key_table_ger};

static void read_settings()
{
	settings = Preferences().read(0, settings);
	if (settings.magic == Settings::MAGIC)
	{
		if (settings.baud_rate_idx < NELEM(baud_rates) && //
			settings.vga_mode_idx < NELEM(vga_modes) &&	  //
			settings.keyboard_idx < NELEM(keyboards))
			return;
	}

	uint8 kbd_idx = NELEM(keyboards);
	while (kbd_idx && keyboards[--kbd_idx] != &USB::USB_DEFAULT_KEYTABLE) {}

	uint8 baudrate_idx = NELEM(baud_rates);
	while (baudrate_idx && baud_rates[--baudrate_idx] != PICO_DEFAULT_UART_BAUD_RATE) {}

	uint8 vga_idx = NELEM(vga_modes);
	while (vga_idx && vga_modes[--vga_idx] != &DEFAULT_VGA_MODE) {}

	settings.baud_rate_idx	  = baudrate_idx;
	settings.vga_mode_idx	  = vga_idx;
	settings.keyboard_idx	  = kbd_idx;
	settings.enable_mouse	  = false;
	settings.auto_wrap		  = false;
	settings.application_mode = false;
	settings.utf8_mode		  = false;
	settings.c1_codes_8bit	  = false;
	settings.newline_mode	  = false;
	settings.local_echo		  = false;
	settings.sgr_cumulative	  = false;
	settings.log_unhandled	  = false;
}

static void write_settings()
{
	settings.magic = Settings::MAGIC;
	Preferences().write(0, settings);
}

void print_heap_free(AnsiTerm& terminal, bool r)
{
	// print list of all free chunks on the heap:

	uint32 sz = heap_largest_free_block();
	if (sz == 0) return;

	terminal.printf("%s: %u bytes\n", r ? "+fragment" : "heap free", sz);

	void* volatile p = malloc(sz);
	assert(p);

	print_heap_free(terminal, 1);
	free(p);
}

void print_system_info(AnsiTerm& terminal)
{
	bool newline		  = terminal.newline_mode;
	terminal.newline_mode = true;

	const uint32 xa = core1_scratch_x_start();
	const uint32 xe = core1_scratch_x_end();

	terminal.printf("system clock = %u MHz\n", clock_get_hz(clk_sys) / 1000000);
	terminal.printf("heap size    = %u bytes\n", heap_total_size());
	terminal.printf("heap free    = %u bytes\n", heap_largest_free_block());
	terminal.printf("scratch_x    = %u bytes\n", xe - xa);
	terminal.printf("program size = %u bytes\n", flash_used());

	terminal.printf(
		"serial port: %u 8N1%s%s\n", baud_rates[settings.baud_rate_idx], //
		terminal.utf8_mode ? ", utf-8" : "", terminal.c1_codes_8bit ? ", 8bit c1 codes" : "");
	terminal.puts(USB::keyboardPresent() ? "keyboard detected\n" : "***no keyboard!\n");
	terminal.puts(USB::mousePresent() ? "mouse detected\n" : "no mouse\n");
	terminal.newline_mode = newline;
}

void print_system_colors(AnsiTerm& terminal)
{
	terminal.printf("VGA colors:\n");

	for (int i = 0; i < 16; i++) { terminal.printf("\x1b[48;5;%im ", i); }
	terminal.printf("\x1b[49m\n"); // reset background color

	for (int r = 0; r < 6; r++)
	{
		for (int g = 0; g < 6; g++)
		{
			for (int b = 0; b < 6; b++) { terminal.printf("\x1b[48;5;%im ", 16 + r * 36 + g * 6 + b); }
			terminal.printf("\x1b[49m "); // reset background color
		}
		terminal.printf("\x1b[49m\n"); // reset background color
	}

	for (int i = 16 + 6 * 6 * 6; i < 256; i++) { terminal.printf("\x1b[48;5;%im ", i); }
	terminal.printf("\x1b[49m\n"); // reset background color
}

static cstr inked(bool a, bool b)
{
	return usingstr("[%s%s%s] ", a == b ? "\x1b[32m" : "\x1b[31m", a ? "ON" : "OFF", "\x1b[39m");
}
static cstr inked(const VgaMode* a, const VgaMode* b)
{
	return usingstr("[%s%ix%i%s] ", a == b ? "\x1b[32m" : "\x1b[31m", a->width, a->height, "\x1b[39m");
}
static cstr inked(uint a, uint b)
{
	return usingstr("[%s%u%s] ", a == b ? "\x1b[32m" : "\x1b[31m", a, "\x1b[39m"); //
}
static cstr inked(const HidKeyTable* a, const HidKeyTable* b)
{
	return usingstr("[%s%s%s] ", a == b ? "\x1b[32m" : "\x1b[31m", a->name, "\x1b[39m"); //
}

void run_osm(AnsiTerm& terminal, cstr msg)
{
	// run the on-screen menu

	Settings s = settings;

again:
	terminal.newline_mode	= true;
	terminal.utf8_mode		= false;
	terminal.cursor_visible = false;
	terminal.puts("\x1b[2J"); // cls
	terminal.puts("\x1b[H");  // locate cursor to (1,1)
	terminal.printf("%s\n\n", msg);
	if (!USB::keyboardPresent()) terminal.puts("***no keyboard present!***\n");
	else terminal.puts("use cursor keys, space, enter, esc");

	constexpr uint first_row = 5;
	constexpr uint last_row	 = 19;
	uint		   row		 = first_row;

	while (true)
	{
		terminal.puts("\x1b[5H"); // locate cursor to (5,1)

		/*5*/ terminal.printf("  show system info\n");
		/*6*/ terminal.printf(
			"  screen size          %s  \n", inked(vga_modes[s.vga_mode_idx], vga_modes[settings.vga_mode_idx]));
		/*7*/ terminal.printf(
			"  baud rate            %s  \n", inked(baud_rates[s.baud_rate_idx], baud_rates[settings.baud_rate_idx]));
		/*8*/ terminal.printf(
			"  keyboard             %s  \n", inked(keyboards[s.keyboard_idx], keyboards[settings.keyboard_idx]));
		/*9*/ terminal.printf("  enable mouse         %s\n", inked(s.enable_mouse, settings.enable_mouse));
		/*10*/ terminal.printf("  utf-8 encoding       %s\n", inked(s.utf8_mode, settings.utf8_mode));
		/*11*/ terminal.printf("  8 bit C1 codes       %s\n", inked(s.c1_codes_8bit, settings.c1_codes_8bit));
		/*12*/ terminal.printf("  newline mode         %s\n", inked(s.newline_mode, settings.newline_mode));
		/*13*/ terminal.printf("  auto-wrap mode       %s\n", inked(s.auto_wrap, settings.auto_wrap));
		/*14*/ terminal.printf("  kbd application mode %s\n", inked(s.application_mode, settings.application_mode));
		/*15*/ terminal.printf("  local echo           %s\n", inked(s.local_echo, settings.local_echo));
		/*16*/ terminal.printf("  SGR accumulative     %s\n", inked(s.sgr_cumulative, settings.sgr_cumulative));
		/*17*/ terminal.printf("  log unhandled codes  %s\n", inked(s.log_unhandled, settings.log_unhandled));
		/*18*/ terminal.printf("  save to flash\n");
		/*19*/ terminal.printf("  exit");

		terminal.printf("\x1b[%uH>\r", row); // locate cursor and print a prompt ">"
		terminal.puts("\x1b[?25h");			 // show cursor
		int c = nochar;
		while (c == nochar)
		{
			Dispatcher::run(1000);
			c = terminal.getc();
			if (USB::ctrl_alt_del_detected) return;
		}
		terminal.puts("\x1b[?25l"); // hide cursor

		if (c == 0x1b) // esc
		{
			for (c = nochar; c == nochar;) { c = terminal.getc(); }
			if (c == 0x1b) return;	// esc key pressed
			if (c != '[') continue; // expect cursor key
			for (c = nochar; c == nochar;) { c = terminal.getc(); }
			switch (c)
			{
			case 'A': //up
				row = row == first_row ? last_row : row - 1;
				continue;
			case 'B': //down
				row = row == last_row ? first_row : row + 1;
				continue;
			case 'C': //right
				c = ' ';
				break;
			case 'D': //left
				if (row == 6) { s.vga_mode_idx = (s.vga_mode_idx + NELEM(vga_modes) - 1u) % NELEM(vga_modes); }
				if (row == 7) { s.baud_rate_idx = (s.baud_rate_idx + NELEM(baud_rates) - 1u) % NELEM(baud_rates); }
				if (row == 8) { s.keyboard_idx = (s.keyboard_idx + NELEM(keyboards) - 1u) % NELEM(keyboards); }
				continue;
			default: continue; //no cursor key
			}
		}

		if (c == ' ' || c == 13) // space or enter
		{
			switch (row)
			{
			case 5: // show system info
			{
				terminal.puts("\x1b[5H"); // locate cursor to (5,1)
				terminal.puts("\x1b[J");  // erase to end of screen
				print_system_info(terminal);
				while (terminal.getc() == nochar) { Dispatcher::run(1000); } // wait for key
				while (terminal.getc() != nochar) {}						 // eat rest of multi-char keys
				goto again;
			}
			case 6: s.vga_mode_idx = (s.vga_mode_idx + 1u) % NELEM(vga_modes); continue;
			case 7: s.baud_rate_idx = (s.baud_rate_idx + 1u) % NELEM(baud_rates); continue;
			case 8: s.keyboard_idx = (s.keyboard_idx + 1u) % NELEM(keyboards); continue;
			case 9: s.enable_mouse = !s.enable_mouse; continue;
			case 10: s.utf8_mode = !s.utf8_mode; continue;
			case 11: s.c1_codes_8bit = !s.c1_codes_8bit; continue;
			case 12: s.newline_mode = !s.newline_mode; continue;
			case 13: s.auto_wrap = !s.auto_wrap; continue;
			case 14: s.application_mode = !s.application_mode; continue;
			case 15: s.local_echo = !s.local_echo; continue;
			case 16: s.sgr_cumulative = !s.sgr_cumulative; continue;
			case 17: s.log_unhandled = !s.log_unhandled; continue;
			case 18: //save to flash: TODO
				settings = s;
				write_settings();
				msg = "settings saved to flash";
				goto again;
			case 19: //exit
				settings = s;
				return;
			default: continue;
			}
		}
	}
}

void run_ansiterm(AnsiTerm& terminal)
{
	terminal.default_utf8_mode		  = settings.utf8_mode;
	terminal.default_application_mode = settings.application_mode;
	terminal.default_local_echo		  = settings.local_echo;
	terminal.default_newline_mode	  = settings.newline_mode;
	terminal.default_sgr_cumulative	  = settings.sgr_cumulative;
	terminal.default_c1_codes_8bit	  = settings.c1_codes_8bit;
	terminal.default_auto_wrap		  = settings.auto_wrap;
	terminal.log_unhandled			  = settings.log_unhandled;
	terminal.reset(true);

	terminal.display->identify();
	print_system_colors(terminal);
	print_system_info(terminal);
	terminal.printf("press ctrl-alt-del to enter setup\n\r");
	terminal.puts("READY\n\n\r");

	bool xoff = false;

	while (!USB::ctrl_alt_del_detected)
	{
		int c = getchar_timeout_us(0);
		if (c == XON || c == XOFF) xoff = c == XOFF;
		else if (c != PICO_ERROR_TIMEOUT) terminal.putc(char(c));
		if (!xoff && (c = terminal.getc()) != nochar) putchar_raw(c);
		else Dispatcher::run(1000);
	}
}

void sysclockChanged(uint32 /*new_clock*/) noexcept
{
	uart_set_baudrate(uart_default, baud_rates[settings.baud_rate_idx]);
}

int main()
{
	stdio_init_all();
	USB::initUSBHost();
	LoadSensor::start();
	read_settings();
	Dispatcher::addHandler(blinkOnboardLed);
	Dispatcher::addHandler(USB::pollUSB);

	// usb needs some time to mount the keyboard, if present
	for (CC wait_end = now() + 2 * 1000 * 1000; now() < wait_end && !USB::keyboardPresent();) Dispatcher::run(1000);

	AudioController::startAudio(true);
	Error error = NO_ERROR;

	for (;;)
	{
		try
		{
			USB::setHidKeyTranslationTable(*keyboards[settings.keyboard_idx]);
			uart_set_baudrate(uart_default, baud_rates[settings.baud_rate_idx]);

			constexpr ColorMode colormode = colormode_a1w8_rgb;
			const VgaMode&		vgamode	  = error ? vga_mode_320x240_60 : *vga_modes[settings.vga_mode_idx];

			CanvasPtr pixmap = new Pixmap<colormode>(vgamode.width, vgamode.height, attrheight_12px);
			VideoController::addPlane(new FrameBuffer<colormode>(pixmap));
			if (!error && settings.enable_mouse) VideoController::addPlane(new MousePointer<Sprite<Shape>>);
			VideoController::startVideo(vgamode, 0, VIDEO_SCANLINE_BUFFER_SIZE);

			AnsiTerm terminal {pixmap};
			if (error) run_osm(terminal, error);
			else run_ansiterm(terminal);

			error = NO_ERROR;
			if (USB::ctrl_alt_del_detected) error = "ctrl-alt-del pressed";
			USB::ctrl_alt_del_detected = false;
		}
		catch (std::exception& e)
		{
			error = e.what();
		}
		catch (Error e)
		{
			error = e;
		}
		catch (...)
		{
			error = UNKNOWN_ERROR;
		}

		VideoController::stopVideo();
	}
}

/*

 


























*/
