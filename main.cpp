// Copyright (c) 2024 - 2024 kio@little-bat.de
// BSD-2-Clause license
// https://opensource.org/licenses/BSD-2-Clause

#include "AudioController.h"
#include "Graphics/AnsiTerm.h"
#include "Graphics/Pixmap_wAttr.h"
#include "USBHost/USBKeyboard.h"
#include "USBHost/hid_handler.h"
#include "Video/FrameBuffer.h"
#include "Video/MousePointer.h"
#include "Video/VideoController.h"
#include "utilities/LoadSensor.h"
#include "utilities/sm_utilities.h"
#include "utilities/utilities.h"
#include <memory>
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

static Settings settings_in_flash; // TODO: in flash
static Settings settings;

static constexpr const VgaMode* vga_modes[] = {&vga_mode_320x240_60, &vga_mode_400x300_60, &vga_mode_512x384_60,
											   &vga_mode_640x480_60, &vga_mode_800x600_60, &vga_mode_1024x768_60,
											   &vga_mode_640x384_60};

static constexpr uint baud_rates[] = {2400, 4800, 9600, 19200, 38400, 57600, 115200};

static constexpr const HidKeyTable* keyboards[] = {&USB::key_table_us, &USB::key_table_ger};

static void read_settings()
{
	settings = settings_in_flash;
	if (settings.magic == Settings::MAGIC) return;

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
	settings.auto_wrap		  = ANSITERM_DEFAULT_AUTO_WRAP;
	settings.application_mode = ANSITERM_DEFAULT_APPLICATION_MODE;
	settings.utf8_mode		  = ANSITERM_DEFAULT_UTF8_MODE;
	settings.c1_codes_8bit	  = ANSITERM_DEFAULT_C1_CODES_8BIT;
	settings.newline_mode	  = ANSITERM_DEFAULT_NEWLINE_MODE;
	settings.local_echo		  = ANSITERM_DEFAULT_LOCAL_ECHO;
	settings.sgr_cumulative	  = ANSITERM_DEFAULT_SGR_CUMULATIVE;
	settings.log_unhandled	  = ANSITERM_DEFAULT_LOG_UNHANDLED;
}

static void write_settings() // TODO
{
	settings.magic	  = Settings::MAGIC;
	settings_in_flash = settings; // TODO
}

void print_heap_free(AnsiTerm& terminal, bool r)
{
	// print list of all free chunks on the heap:

	uint32 sz = heap_free();
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

	terminal.printf("running on core %u\n", get_core_num());
	terminal.printf("total heap size = %u\n", heap_size());
	print_heap_free(terminal, 0);
	terminal.printf("stack free: %u bytes\n", stack_free());

	const uint32 xa = core1_scratch_x_start();
	const uint32 xe = core1_scratch_x_end();
	if (xa != xe) terminal.printf("0x%08x to 0x%08x: core1 scratch_x\n", xa, xe);
	else terminal.printf("core1 scratch_x not used\n");
	terminal.printf("0x%08x to 0x%08x: core1 stack\n", core1_stack_bottom(), core1_stack_top());

	const size_t fa = flash_binary_start();
	const size_t fe = flash_binary_end();

	terminal.printf("0x%08x to 0x%08x: flash, used %u, free %u\n", fa, fe, flash_used(), flash_free());

	terminal.printf("system clock = %u MHz\n", clock_get_hz(clk_sys) / 1000000);
	uint min, max, avg;
	LoadSensor::getLoad(1 /*core*/, min, avg, max);
	min = (min + 50000) / 100000;
	max = (max + 50000) / 100000;
	avg = (avg + 50000) / 100000;
	terminal.printf(
		"load core 1: %u.%u, %u.%u, %u.%uMHz (min,avg,max)\n", //
		min / 10, min % 10, avg / 10, avg % 10, max / 10, max % 10);
	terminal.printf(
		"serial port: %u 8N1%s%s\n", baud_rates[settings.baud_rate_idx], //
		terminal.utf8_mode ? ", utf-8" : "", terminal.c1_codes_8bit ? ", 8bit c1 codes" : "");
	terminal.puts(USB::keyboardPresent() ? "keyboard detected\n" : "***no keyboard!\n");
	terminal.puts(USB::mousePresent() ? "mouse detected\n" : "no mouse\n");
	terminal.newline_mode = newline;
}

static void run_sm()
{
	sm_blink_onboard_led();
	USB::pollUSB();
	sm_throttle();
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
			run_sm();
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
				while (terminal.getc() == nochar) { run_sm(); } // wait for key
				while (terminal.getc() != nochar) {}			// eat rest of multi-char keys
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
				kio::Audio::beep(440, 0.3f, 1000);
				write_settings();
				continue;
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
		else run_sm();
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

	// usb needs some time to mount the keyboard, if present
	for (CC wait_end = now() + 2 * 1000 * 1000; now() < wait_end && !USB::keyboardPresent();) run_sm();

	VideoController& videocontroller = VideoController::getRef();
	Audio::AudioController::getRef().startAudio(true);
	Error error = NO_ERROR;

	for (;;)
	{
		try
		{
			USB::setHidKeyTranslationTable(*keyboards[settings.keyboard_idx]);
			uart_set_baudrate(uart_default, baud_rates[settings.baud_rate_idx]);

			constexpr ColorMode colormode = colormode_a1w8_rgb;
			const VgaMode&		vgamode	  = error ? vga_mode_320x240_60 : *vga_modes[settings.vga_mode_idx];

			CanvasPtr				 pixmap = new Pixmap<colormode>(vgamode.width, vgamode.height, attrheight_12px);
			std::unique_ptr<Color[]> colors {newColorMap(colormode)};

			videocontroller.addPlane(new FrameBuffer<colormode>(pixmap, colors.get()));
			if (!error && settings.enable_mouse) videocontroller.addPlane(new MousePointer<Sprite<Shape>>);
			videocontroller.startVideo(vgamode, 0, VIDEO_SCANLINE_BUFFER_SIZE);

			AnsiTerm terminal {pixmap, colors.get()};
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

		videocontroller.stopVideo();
	}
}

/*

 


























*/
