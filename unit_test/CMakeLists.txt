###########################################
# unit_tests/CMakeLists.txt

cmake_minimum_required(VERSION 3.13)

project(AnsiTermUnitTest CXX C ASM)

set(CMAKE_CXX_STANDARD 20)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)
#set(PICO_CXX_ENABLE_RTTI 1)
 
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DRELEASE -DNDEBUG")

set(PICO_BOARD "vgaboard" CACHE STRING "the target board, e.g. \"vgaboard\"")

add_executable(AnsiTermUnitTest
	main_unit_test.cpp
	AnsiTerm_unit_test.cpp
	MockTextVDU.cpp
	MockTextVDU.h
	MockHidHandler.h
	MockHidHandler.cpp
	../AnsiTerm.cpp
	../AnsiTerm.h
	../kilipili/desktop_tools/unit_test/Mock/MockPixmap.h
	../kilipili/desktop_tools/unit_test/Mock/MockPixmap.cpp
	)

target_compile_definitions(AnsiTermUnitTest PUBLIC
	MAKE_TOOLS
	UNIT_TEST
	PICO_BOARD_H="${PICO_BOARD_HEADER_DIRS}/${PICO_BOARD}.h"
	)

# add current dir to 'include search path':
target_include_directories(AnsiTermUnitTest PUBLIC
	${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_LIST_DIR}/..
	${CMAKE_CURRENT_LIST_DIR}/../kilipili
	)

# dependencies. this also adds the include paths:
target_link_libraries(AnsiTermUnitTest PUBLIC
	kilipili_common
	kilipili_graphics
	kilipili_usb_host
	)

 













