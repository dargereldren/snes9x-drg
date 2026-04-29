if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	find_package(SDL2 REQUIRED)
	list(APPEND LIBS SDL2::SDL2 SDL2::SDL2main)
	list(APPEND LIBS ${ZLIB_LIBRARIES} ${PNG_LIBRARIES} opengl32 gdi32 winmm imm32 ole32 oleaut32 version uuid advapi32 setupapi shell32)
else()
	pkg_check_modules(SDL2 REQUIRED sdl2)
	list(APPEND INCLUDES ${SDL2_INCLUDE_DIRS})
	list(APPEND LIBS ${SDL2_LIBRARIES} ${ZLIB_LIBRARIES} ${PNG_LIBRARIES})
	list(APPEND FLAGS ${SDL2_CFLAGS})
endif()

# SDL2 build
set(SOURCES ${SOURCES}
	ports/sdl2/sdlmain.cpp
	ports/sdl2/sdlinput.cpp
	ports/sdl2/sdlvideo.cpp
	ports/sdl2/sdlaudio.cpp
)