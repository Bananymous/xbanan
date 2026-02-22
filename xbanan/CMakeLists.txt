set(SOURCES
	main.cpp
	Base.cpp
	Drawing.cpp
	Extensions.cpp
	ExtBigReg.cpp
	ExtRANDR.cpp
	Font.cpp
	Image.cpp
	Keymap.cpp
	SafeGetters.cpp
)

option(ENABLE_GLX "enable glx extension" ON)

include(CheckSymbolExists)
include(CMakeDependentOption)
check_symbol_exists(shmat "sys/shm.h" HAVE_SHMAT)
check_symbol_exists(shmdt "sys/shm.h" HAVE_SHMDT)
cmake_dependent_option(ENABLE_SHM "enable shm extension" ON "HAVE_SHMAT;HAVE_SHMDT" OFF)

if(ENABLE_GLX)
	set(SOURCES ${SOURCES} ExtGLX.cpp)
endif()

if(ENABLE_SHM)
	set(SOURCES ${SOURCES} ExtSHM.cpp)
endif()

add_executable(xbanan ${SOURCES})
banan_link_library(xbanan ban)
banan_link_library(xbanan libdeflate)
banan_link_library(xbanan libgui)
banan_link_library(xbanan libinput)

target_compile_options(xbanan PRIVATE -Wall -Wextra)
target_compile_options(xbanan PRIVATE
	-Wno-sign-compare
	-Wno-missing-field-initializers
	-Wno-unused-variable
	-Wno-unused-but-set-variable
)

install(TARGETS xbanan OPTIONAL)
