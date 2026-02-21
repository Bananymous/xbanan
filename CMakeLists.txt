cmake_minimum_required(VERSION 3.26)

project(banan-os CXX C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(banan_include_headers target library)
	target_include_directories(${target} PUBLIC $<TARGET_PROPERTY:${library},SOURCE_DIR>/include)
endfunction()

function(banan_link_library target library)
	target_link_libraries(${target} PUBLIC ${library})
	banan_include_headers(${target} ${library})
endfunction()

function(banan_install_headers target)
	file(GLOB_RECURSE headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/include *.h)
	foreach(header ${headers})
		get_filename_component(subdirectory ${header} DIRECTORY)
		install(FILES include/${header} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${subdirectory})
	endforeach()
	target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endfunction()

set(CMAKE_CXX_STANDARD 20)
add_compile_definitions(-Dstddbg=stdout)
add_compile_options(-g)

if(BANAN_OS)
	find_library(BAN ban REQUIRED)
	add_library(ban SHARED IMPORTED)
	set_target_properties(ban PROPERTIES IMPORTED_LOCATION "${BAN}")

	find_library(LIBGUI gui REQUIRED)
	add_library(libgui SHARED IMPORTED)
	set_target_properties(libgui PROPERTIES IMPORTED_LOCATION "${LIBGUI}")

	find_library(LIBINPUT input REQUIRED)
	add_library(libinput SHARED IMPORTED)
	set_target_properties(libinput PROPERTIES IMPORTED_LOCATION "${LIBINPUT}")

	find_library(LIBDEFLATE deflate REQUIRED)
	add_library(libdeflate SHARED IMPORTED)
	set_target_properties(libdeflate PROPERTIES IMPORTED_LOCATION "${LIBDEFLATE}")
else()
	add_subdirectory(BAN)
	add_subdirectory(LibClipboard)
	add_subdirectory(LibDEFLATE)
	add_subdirectory(LibFont)
	add_subdirectory(LibGUI)
	add_subdirectory(LibImage)
	add_subdirectory(LibInput)
	add_subdirectory(Terminal)
	add_subdirectory(ProgramLauncher)
	add_subdirectory(WindowServer)
endif()

add_subdirectory(xbanan)
