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

set(XBANAN_LIBS
	ban
	libdeflate
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
	list(APPEND XBANAN_LIBS libgui libinput)
elseif(PLATFORM STREQUAL "SDL3")
	find_package(SDL3 REQUIRED)
	list(APPEND XBANAN_SOURCES SDL3/sdl3.cpp)
	list(APPEND XBANAN_LIBS SDL3::SDL3)
endif()

add_executable(xbanan ${XBANAN_SOURCES})

target_compile_definitions(xbanan PRIVATE FONT_PATH="${FONT_PATH}")

target_compile_options(xbanan PRIVATE -Wall -Wextra)
target_compile_options(xbanan PRIVATE
	-Wno-sign-compare
	-Wno-missing-field-initializers
	-Wno-unused-variable
	-Wno-unused-but-set-variable
)

foreach(lib ${XBANAN_LIBS})
	banan_link_library(xbanan ${lib})
endforeach()

install(TARGETS xbanan OPTIONAL)
