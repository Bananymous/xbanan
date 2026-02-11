set(SOURCES
	main.cpp
	Base.cpp
	Extensions.cpp
	ExtRANDR.cpp
	Keymap.cpp
)

add_executable(xbanan ${SOURCES})
banan_link_library(xbanan ban)
banan_link_library(xbanan libgui)

target_compile_options(xbanan PRIVATE -Wall -Wextra -Wno-sign-compare -Wno-missing-field-initializers)

install(TARGETS xbanan OPTIONAL)
