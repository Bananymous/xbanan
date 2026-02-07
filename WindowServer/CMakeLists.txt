set(SOURCES
	main.cpp
	Framebuffer.cpp
	Window.cpp
	WindowServer.cpp
)

add_executable(WindowServer ${SOURCES})
banan_include_headers(WindowServer libgui)
banan_link_library(WindowServer ban)
banan_link_library(WindowServer libfont)
banan_link_library(WindowServer libimage)
banan_link_library(WindowServer libinput)

target_link_libraries(WindowServer PRIVATE SDL2)

install(TARGETS WindowServer OPTIONAL)
