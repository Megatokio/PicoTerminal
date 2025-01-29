
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

set(VIDEO_DEFAULT_SCREENSIZE    "640x480" CACHE STRING "Default screen size")
set(VIDEO_SCANLINE_BUFFER_SIZE  "4"       CACHE STRING "Buffer for prepared scanlines")
set(PICO_DEFAULT_UART_BAUD_RATE "115200"  CACHE STRING "Default uart baud rate")
	
set_property(CACHE VIDEO_DEFAULT_SCREENSIZE PROPERTY STRINGS 320x240 400x300 512x384 640x480 800x600 1024x768)
set_property(CACHE VIDEO_SCANLINE_BUFFER_SIZE  PROPERTY STRINGS "2" "4" "8" "16")
set_property(CACHE PICO_DEFAULT_UART_BAUD_RATE PROPERTY STRINGS 2400 4800 9600 19200 38400 57600 115200)


##########################################################################

if(MAKE_TOOLS)
	add_subdirectory(kilipili/desktop_tools)
endif()

if(NOT MAKE_TOOLS)
	pico_sdk_init()
	add_subdirectory(kilipili)
	
	add_compile_definitions( 
		ON=1 OFF=0
		PICO_STDIO_UART=${PICO_STDIO_UART}
		PICO_STDIO_USB=${PICO_STDIO_USB}
		DEFAULT_VGA_MODE=vga_mode_${VIDEO_DEFAULT_SCREENSIZE}_60
		VIDEO_SCANLINE_BUFFER_SIZE=${VIDEO_SCANLINE_BUFFER_SIZE}
		PICO_DEFAULT_UART_BAUD_RATE=${PICO_DEFAULT_UART_BAUD_RATE}
		)
	
		add_executable(PicoTerm
		main.cpp
		kilipili/utilities/malloc.cpp
		)
 
	target_link_libraries(PicoTerm PUBLIC
		kilipili
		)
	
	pico_enable_stdio_usb(PicoTerm ${PICO_STDIO_USB})
	pico_enable_stdio_uart(PicoTerm ${PICO_STDIO_UART})
	
	pico_add_extra_outputs(PicoTerm)

endif()













