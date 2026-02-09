set(SOURCES
	main.cpp
)

add_executable(xbanan ${SOURCES})
banan_link_library(xbanan ban)
banan_link_library(xbanan libgui)

target_compile_options(xbanan PRIVATE -Wall -Wno-sign-compare)

install(TARGETS xbanan OPTIONAL)
