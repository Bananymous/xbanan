set(XBANAN_SOURCES
	main.cpp
	Base.cpp
	Drawing.cpp
	Events.cpp
	Extensions.cpp
	ExtBigReg.cpp
	ExtRANDR.cpp
	ExtXRes.cpp
	Font.cpp
	Image.cpp
	Keymap.cpp
	SafeGetters.cpp
)

set(FONT_PATH ${CMAKE_SOURCE_DIR}/fonts CACHE STRING "path to X11 fonts")

option(ENABLE_GLX "enable glx extension" ON)
if(ENABLE_GLX)
	list(APPEND XBANAN_SOURCES ExtGLX.cpp)
endif()

option(ENABLE_SHM "enable shm extension" ON)
if(ENABLE_SHM)
	list(APPEND XBANAN_SOURCES ExtSHM.cpp)
endif()

if(PLATFORM STREQUAL "banan-os")
	list(APPEND XBANAN_SOURCES banan-os/banan-os.cpp)
elseif(PLATFORM STREQUAL "SDL3")
	list(APPEND XBANAN_SOURCES SDL3/sdl3.cpp)
endif()

add_executable(xbanan ${XBANAN_SOURCES})
banan_link_library(xbanan ban)
banan_link_library(xbanan libdeflate)

target_compile_definitions(xbanan PRIVATE FONT_PATH="${FONT_PATH}")

if(PLATFORM STREQUAL "banan-os")
	banan_link_library(xbanan libgui)
	banan_link_library(xbanan libinput)
elseif(PLATFORM STREQUAL "SDL3")
	find_package(SDL3 REQUIRED)
	banan_link_library(xbanan SDL3::SDL3)
endif()

target_compile_options(xbanan PRIVATE -Wall -Wextra)
target_compile_options(xbanan PRIVATE
	-Wno-sign-compare
	-Wno-missing-field-initializers
	-Wno-unused-variable
	-Wno-unused-but-set-variable
)

install(TARGETS xbanan OPTIONAL)
