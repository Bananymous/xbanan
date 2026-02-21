set(SOURCES
	main.cpp
	Base.cpp
	Drawing.cpp
	Extensions.cpp
	ExtBigReg.cpp
	ExtGLX.cpp
	ExtRANDR.cpp
	Font.cpp
	Image.cpp
	Keymap.cpp
	SafeGetters.cpp
)

include(CheckSymbolExists)

check_symbol_exists(shmat "sys/shm.h" HAVE_SHMAT)
check_symbol_exists(shmdt "sys/shm.h" HAVE_SHMDT)
if(HAVE_SHMAT AND HAVE_SHMDT)
	set(SOURCES ${SOURCES} ExtSHM.cpp)
endif()

add_executable(xbanan ${SOURCES})
banan_link_library(xbanan ban)
banan_link_library(xbanan libgui)
banan_link_library(xbanan libdeflate)

target_compile_options(xbanan PRIVATE -Wall -Wextra -Wno-sign-compare -Wno-missing-field-initializers)

install(TARGETS xbanan OPTIONAL)
