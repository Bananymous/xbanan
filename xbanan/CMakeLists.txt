set(SOURCES
	main.cpp
)

add_executable(xbanan ${SOURCES})
banan_link_library(xbanan ban)
banan_link_library(xbanan libgui)

install(TARGETS xbanan OPTIONAL)
