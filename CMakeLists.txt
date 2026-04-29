cmake_minimum_required(VERSION 3.20)
project(snes9x VERSION 1.63)
include(GNUInstallDirs)

# Port
set(SNES9X_PORT "GTK" CACHE STRING "Port to build snes9x with")

# System
option(DEBUGGER "Enable Snes9x Debugger" ON)
option(HAVE_NETPLAY "Build support for Netplay" ON)

# Sound
option(USE_SOUNDDRIVER "Build support for sound drivers" ON)
option(USE_PORTAUDIO "Build PortAudio sound driver" OFF)
option(USE_ALSA "Build ALSA sound driver" ON)
option(USE_PULSEAUDIO "Use PulseAudio sound driver" OFF)
option(USE_OSS "Build support for OSS sound driver" OFF)

# Zip
option(HAVE_UNZIP "Build support for unzip" ON)
option(USE_SYSTEMZIP "Force use of system minizip" ON)

# Other libraries
option(USE_XV "Build support for XVideo output" ON)
option(USE_WAYLAND "Build support for Wayland" ON)
option(USE_LIBPNG "Build support for PNG" ON)
option(USE_JMA "Build support for JMA" ON)

# Options
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Type
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "One of: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# Building
set(LIBS)
set(FLAGS)
set(INCLUDES "core/" "core/apu/" "core/apu/bapu/" "lib/" "lib/unzip/" "lib/jma/" "lib/fmt/" "lib/fmt/include/" "lib/glad/include/" "lib/imgui/" "ports/common/")
set(DEFINES HAVE_STDINT_H ALLOW_CPU_OVERCLOCK HAVE_STRINGS_H HAVE_STDINT_H RIGHTSHIFT_IS_SAR)
set(SOURCES
	core/fxinst.cpp
	core/fxemu.cpp
	core/fxdbg.cpp
	core/c4.cpp
	core/c4emu.cpp
	core/apu/apu.cpp
	core/apu/bapu/dsp/sdsp.cpp
	core/apu/bapu/smp/smp.cpp
	core/apu/bapu/smp/smp_state.cpp
	core/msu1.cpp
	core/dsp.cpp
	core/dsp1.cpp
	core/dsp2.cpp
	core/dsp3.cpp
	core/dsp4.cpp
	core/spc7110.cpp
	core/obc1.cpp
	core/seta.cpp
	core/seta010.cpp
	core/seta011.cpp
	core/seta018.cpp
	core/controls.cpp
	core/crosshairs.cpp
	core/cpu.cpp
	core/sa1.cpp
	core/debug.cpp
	core/sdd1.cpp
	core/tile.cpp
	core/tileimpl-n1x1.cpp
	core/tileimpl-n2x1.cpp
	core/tileimpl-h2x1.cpp
	core/srtc.cpp
	core/gfx.cpp
	core/memmap.cpp
	core/clip.cpp
	core/ppu.cpp
	core/dma.cpp
	core/snes9x.cpp
	core/globals.cpp
	core/stream.cpp
	core/conffile.cpp
	core/bsx.cpp
	core/snapshot.cpp
	core/screenshot.cpp
	core/movie.cpp
	core/statemanager.cpp
	core/sha256.cpp
	core/bml.cpp
	core/cpuops.cpp
	core/cpuexec.cpp
	core/sa1cpu.cpp
	core/cheats.cpp
	core/cheats2.cpp
	core/sdd1emu.cpp
	core/netplay.cpp
	core/server.cpp
	core/loadzip.cpp
	core/fscompat.cpp)

# Packages
# ZLIB
find_package(PkgConfig REQUIRED)
if(USE_ZLIB)
	pkg_check_modules(ZLIB REQUIRED zlib)
	list(APPEND INCLUDES ${ZLIB_INCLUDE_DIRS})
	list(APPEND FLAGS ${ZLIB_COMPILE_FLAGS})
	list(APPEND DEFINES ZLIB)
endif()

# PNG
if(USE_PNG)
	pkg_check_modules(PNG REQUIRED libpng)
	list(APPEND INCLUDES ${PNG_INCLUDE_DIRS})
	list(APPEND FLAGS ${PNG_COMPILE_FLAGS})
	list(APPEND DEFINES HAVE_LIBPNG)
endif()

# Netplay
if(HAVE_NETPLAY)
	list(APPEND DEFINES "NETPLAY_SUPPORT")
endif()

# Unzip
if(HAVE_UNZIP)
	list(APPEND DEFINES "UNZIP_SUPPORT")
	pkg_check_modules(MINIZIP "minizip")
	if(USE_SYSTEMZIP AND MINIZIP_FOUND)
		list(APPEND DEFINES "SYSTEM_ZIP")
		list(APPEND ARGS ${MINIZIP_CFLAGS})
		list(APPEND LIBS ${MINIZIP_LIBRARIES})
	else()
		list(APPEND SOURCES lib/unzip/unzip.c lib/unzip/ioapi.c lib/unzip/zip.c)
	endif()
endif()

# Wayland
if(USE_WAYLAND)
	pkg_check_modules(WAYLAND REQUIRED wayland-client wayland-egl)
	list(APPEND DEFINES "USE_WAYLAND")
	list(APPEND SOURCES ports/common/video/opengl/wayland_egl_context.cpp
						ports/common/video/opengl/wayland_egl_context.hpp
						ports/common/video/wayland/wayland_surface.cpp
						ports/common/video/wayland/wayland_surface.hpp
						ports/common/video/wayland/wayland-idle-inhibit-unstable-v1.c
						ports/common/video/wayland/viewporter-client-protocol.c
						ports/common/video/wayland/fractional-scale-v1.c)
	list(APPEND ARGS ${WAYLAND_CFLAGS})
	list(APPEND LIBS ${WAYLAND_LIBRARIES})
endif()

# Sound driver
if(USE_SOUNDDRIVER)
	list(APPEND SOURCES
		ports/common/audio/s9x_sound_driver.hpp
		ports/common/audio/s9x_sound_driver_sdl.hpp
		ports/common/audio/s9x_sound_driver_sdl.cpp)
	if(USE_PULSEAUDIO)
		pkg_check_modules(PULSEAUDIO REQUIRED libpulse)
		list(APPEND DEFINES "USE_PULSEAUDIO")
		list(APPEND SOURCES ports/common/audio/s9x_sound_driver_pulse.cpp)
		list(APPEND ARGS ${PULSEAUDIO_CFLAGS})
		list(APPEND LIBS ${PULSEAUDIO_LIBRARIES})
	endif()
	if(USE_PORTAUDIO)
		pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
		list(APPEND DEFINES "USE_PORTAUDIO")
		list(APPEND SOURCES ports/common/audio/s9x_sound_driver_portaudio.cpp)
		list(APPEND ARGS ${PORTAUDIO_CFLAGS})
		list(APPEND LIBS ${PORTAUDIO_LIBRARIES})
	endif()
	if(USE_ALSA)
		pkg_check_modules(ALSA REQUIRED alsa)
		list(APPEND DEFINES "USE_ALSA")
		list(APPEND SOURCES ports/common/audio/s9x_sound_driver_alsa.cpp)
		list(APPEND ARGS ${ALSA_CFLAGS})
		list(APPEND LIBS ${ALSA_LIBRARIES})
	endif()
	if(USE_OSS)
		CHECK_INCLUDE_FILE("sys/soundcard.h" OSS)
		if(OSS)
			list(APPEND SOURCES ports/common/audio/s9x_sound_driver_oss.cpp)
			list(APPEND DEFINES "USE_OSS")
		endif()
	endif()
endif()

# JMA
if(USE_JMA)
	list(APPEND DEFINES "JMA_SUPPORT")
	set(LIBJMA_SOURCES
		lib/jma/s9x-jma.cpp
		lib/jma/7zlzma.cpp
		lib/jma/crc32.cpp
		lib/jma/iiostrm.cpp
		lib/jma/inbyte.cpp
		lib/jma/jma.cpp
		lib/jma/lzma.cpp
		lib/jma/lzmadec.cpp
		lib/jma/winout.cpp)
	add_library(jma ${LIBJMA_SOURCES})
	target_include_directories(jma PRIVATE ${INCLUDES})
	target_compile_options(jma PUBLIC ${ARGS})
	target_compile_definitions(jma PRIVATE ${DEFINES})
	list(APPEND LIBS jma)
endif()

# Port building
if(SNES9X_PORT MATCHES "SDL2")
	include(ports/sdl2/snes9x.cmake)
endif()
if(SNES9X_PORT MATCHES "GTK")
	include(ports/gtk/snes9x.cmake)
endif()

# Build the final executable.
add_executable(snes9x-lin ${SOURCES})
target_link_libraries(snes9x-lin PRIVATE ${LIBS})
target_compile_definitions(snes9x-lin PRIVATE ${DEFINES})
target_compile_options(snes9x-lin PRIVATE ${FLAGS})
target_include_directories(snes9x-lin PRIVATE ${INCLUDES})
set_target_properties(snes9x-lin PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "../"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "../"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "../"
	RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "../"
	RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "../"
)