
cmake_minimum_required(VERSION 3.13)

if(NOT (UNIT_TEST OR MAKE_TOOLS))
	include(pico_sdk_import.cmake)
	include(pico_extras_import.cmake)
endif()

project(PicoTerminal CXX C ASM)

set(CMAKE_CXX_STANDARD 20)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)
#set(PICO_CXX_ENABLE_RTTI 1)
 

MESSAGE (STATUS "System = ${CMAKE_SYSTEM_NAME}")   # Linux, PICO
MESSAGE (STATUS "CPU = ${CMAKE_SYSTEM_PROCESSOR}") # x86_64, cortex-m0plus

set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DRELEASE -DNDEBUG")


####################################################################
# OPTIONS:

option(MAKE_TOOLS "compile unit test and desktop tools (desktop, not pico!)" OFF)

option(ANSITERM_DEFAULT_UTF8_MODE        "Use UTF-8 on serial line" OFF)
option(ANSITERM_DEFAULT_APPLICATION_MODE "Keypad in application mode" OFF)
option(ANSITERM_DEFAULT_LOCAL_ECHO       "Local echo printable chars" OFF)
option(ANSITERM_DEFAULT_NEWLINE_MODE     "Newline mode: automatical cr after lf" OFF)
option(ANSITERM_DEFAULT_SGR_CUMULATIVE   "SGR (set graphics rendition) is cumulative" OFF)
option(ANSITERM_DEFAULT_C1_CODES_8BIT    "Send C1 codes as single 8-bit char" OFF)
option(ANSITERM_DEFAULT_AUTO_WRAP        "Automatically wrap cursor to next line" OFF)
option(ANSITERM_DEFAULT_LOG_UNHANDLED    "Print unhandled and broken control codes" OFF)

set(ANSITERM_DEFAULT_SCREENSIZE  "640x480" CACHE STRING "Default screen size")
set(VIDEO_SCANLINE_BUFFER_SIZE   "4"       CACHE STRING "Buffer for prepared scanlines")
set(PICO_DEFAULT_UART_BAUD_RATE  "115200"  CACHE STRING "Default uart baud rate")
	
set_property(CACHE ANSITERM_DEFAULT_SCREENSIZE PROPERTY STRINGS 320x240 400x300 512x384 640x480 800x600 1024x768)
set_property(CACHE VIDEO_SCANLINE_BUFFER_SIZE  PROPERTY STRINGS "2" "4" "8" "16")
set_property(CACHE PICO_DEFAULT_UART_BAUD_RATE PROPERTY STRINGS 2400 4800 9600 19200 38400 57600 115200)


##########################################################################

if(MAKE_TOOLS)
	add_subdirectory(kilipili/desktop_tools)
endif()
if(NOT MAKE_TOOLS)
	pico_sdk_init()
	add_subdirectory(kilipili)
	
##########################################################################
# Pico VGA Ansi Text Terminal                                                  

	add_compile_definitions( 
		ON=1 OFF=0
		PICO_STDIO_UART=${PICO_STDIO_UART}
		PICO_STDIO_USB=${PICO_STDIO_USB}
		DEFAULT_VGA_MODE=vga_mode_${ANSITERM_DEFAULT_SCREENSIZE}_60
		VIDEO_SCANLINE_BUFFER_SIZE=${VIDEO_SCANLINE_BUFFER_SIZE}
		PICO_DEFAULT_UART_BAUD_RATE=${PICO_DEFAULT_UART_BAUD_RATE}
		ANSITERM_DEFAULT_UTF8_MODE=${ANSITERM_DEFAULT_UTF8_MODE}
		ANSITERM_DEFAULT_APPLICATION_MODE=${ANSITERM_DEFAULT_APPLICATION_MODE}
		ANSITERM_DEFAULT_LOCAL_ECHO=${ANSITERM_DEFAULT_LOCAL_ECHO}
		ANSITERM_DEFAULT_NEWLINE_MODE=${ANSITERM_DEFAULT_NEWLINE_MODE}
		ANSITERM_DEFAULT_SGR_CUMULATIVE=${ANSITERM_DEFAULT_SGR_CUMULATIVE}
		ANSITERM_DEFAULT_C1_CODES_8BIT=${ANSITERM_DEFAULT_C1_CODES_8BIT}
		ANSITERM_DEFAULT_AUTO_WRAP=${ANSITERM_DEFAULT_AUTO_WRAP}
		ANSITERM_DEFAULT_LOG_UNHANDLED=${ANSITERM_DEFAULT_LOG_UNHANDLED}
		)
	
		add_executable(PicoTerm
		main.cpp
		kilipili/utilities/malloc.cpp
	)
 
	# Pull in libraries: (this also adds the header search paths)
	target_link_libraries(PicoTerm PUBLIC
		kilipili
		pico_stdlib
		pico_multicore
		#hardware_pwm
		#hardware_pio
		#hardware_dma
		#hardware_irq
		#hardware_spi
		#hardware_interp
		#hardware_rtc
		#hardware_i2c
		#hardware_adc
		#hardware_flash
	)
	
	if(CMAKE_SYSTEM_NAME STREQUAL "PICO")
		pico_enable_stdio_usb(PicoTerm ${PICO_STDIO_USB})
		pico_enable_stdio_uart(PicoTerm ${PICO_STDIO_UART})
	endif()
	
	# create map/bin/hex/uf2 file etc.
	pico_add_extra_outputs(PicoTerm)

endif()













