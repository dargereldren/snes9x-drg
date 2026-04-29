string(APPEND DATADIR ${CMAKE_INSTALL_PREFIX} "/" ${CMAKE_INSTALL_DATAROOTDIR} "/snes9x")
string(APPEND LOCALEDIR ${CMAKE_INSTALL_PREFIX} "/" ${CMAKE_INSTALL_DATAROOTDIR} "/locale")
add_compile_definitions(SNES9X_GTK HAVE_MKSTEMP "GETTEXT_PACKAGE=\"${CMAKE_PROJECT_NAME}\"" DATADIR=\"${DATADIR}\" SNES9XLOCALEDIR=\"${LOCALEDIR}\")
set(ARGS -Wall -Wno-unused-parameter -Wno-unused-variable)

include(CheckIncludeFile)
include(FindGettext)

# Add any new language to this foreach loop
foreach(lang de es fr_FR ja pl pt_BR ru sr@latin uk zh_CN)
	# GETTEXT_PROCESS_PO_FILES(${lang} ALL PO_FILES po/${lang}.po)
	# install(FILES ${CMAKE_BINARY_DIR}/${lang}.gmo
	#        DESTINATION ${LOCALEDIR}/${lang}/LC_MESSAGES
	#        RENAME ${CMAKE_PROJECT_NAME}.mo
	#        COMPONENT translations)
endforeach()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(GTK REQUIRED gtkmm-3.0 gthread-2.0 libpng)
pkg_check_modules(XRANDR REQUIRED xrandr)

find_library(X11 X11 REQUIRED)
find_library(XEXT Xext REQUIRED)
list(APPEND INCLUDES ${SDL2_INCLUDE_DIRS} ${GTK_INCLUDE_DIRS})
list(APPEND ARGS ${SDL2_CFLAGS} ${GTK_CFLAGS} ${XRANDR_CFLAGS})
list(APPEND LIBS ${X11} ${XEXT} ${CMAKE_DL_LIBS} ${SDL2_LIBRARIES} ${GTK_LIBRARIES} ${XRANDR_LIBRARIES})

if(USE_XV)
	pkg_check_modules(XV REQUIRED xv)
	list(APPEND DEFINES "USE_XV")
	list(APPEND SOURCES ports/gtk/gtk_display_driver_xv.cpp ports/gtk/gtk_display_driver_xv.h)
	list(APPEND ARGS ${XV_CFLAGS})
	list(APPEND LIBS ${XV_LIBRARIES})
endif()

add_executable(sourcify ports/gtk/sourcify.c)
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gtk_snes9x_ui.cpp
	COMMAND sourcify ${CMAKE_CURRENT_SOURCE_DIR}/ports/gtk/snes9x.ui ${CMAKE_CURRENT_BINARY_DIR}/gtk_snes9x_ui.cpp snes9x_ui
	DEPENDS sourcify)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mini_icon.cpp
	COMMAND sourcify ${CMAKE_CURRENT_SOURCE_DIR}/ports/gtk/data/mini_icon.png ${CMAKE_CURRENT_BINARY_DIR}/mini_icon.cpp mini_icon
	DEPENDS sourcify)

list(APPEND SOURCES
    lib/imgui/imgui.cpp
	lib/imgui/imgui_demo.cpp
	lib/imgui/imgui_draw.cpp
	lib/imgui/imgui_impl_opengl3.cpp
	lib/imgui/imgui_tables.cpp
	lib/imgui/imgui_widgets.cpp
	lib/imgui/snes9x_imgui.cpp
    lib/fmt/src/format.cc
    ports/common/video/opengl/glx_context.cpp
    ports/common/video/opengl/shaders/glsl.cpp
    ports/common/video/opengl/shaders/shader_helpers.cpp
    ports/common/video/vulkan/slang_helpers.cpp
    ports/common/video/vulkan/slang_helpers.hpp
    ports/common/video/vulkan/slang_preset_ini.cpp
    ports/common/video/vulkan/slang_preset_ini.hpp
    ports/common/video/std_chrono_throttle.cpp
    ports/common/video/std_chrono_throttle.hpp
    lib/glad/src/glx.c
    lib/glad/src/egl.c
    lib/glad/src/gl.c
    ports/gtk/gtk_display_driver_opengl.cpp
    ports/gtk/gtk_shader_parameters.cpp
	ports/gtk/gtk_binding.cpp
	ports/gtk/gtk_binding.h
	ports/gtk/gtk_cheat.cpp
	ports/gtk/gtk_cheat.h
	ports/gtk/gtk_config.cpp
	ports/gtk/gtk_config.h
	ports/gtk/gtk_control.cpp
	ports/gtk/gtk_control.h
	ports/gtk/gtk_display.cpp
	ports/gtk/gtk_display_driver_gtk.cpp
	ports/gtk/gtk_display_driver_gtk.h
	ports/gtk/gtk_display_driver.h
	ports/gtk/gtk_display.h
	ports/gtk/threadpool.cpp
	ports/gtk/threadpool.h
	ports/gtk/gtk_file.cpp
	ports/gtk/gtk_file.h
	ports/gtk/gtk_builder_window.cpp
	ports/gtk/gtk_builder_window.h
	ports/gtk/gtk_preferences.cpp
	ports/gtk/gtk_preferences.h
	ports/gtk/gtk_s9xcore.h
	ports/gtk/gtk_s9x.cpp
	ports/gtk/gtk_s9x.h
	ports/gtk/gtk_s9xwindow.cpp
	ports/gtk/gtk_s9xwindow.h
	ports/gtk/gtk_sound.cpp
	ports/gtk/gtk_sound.h
	ports/gtk/gtk_splash.cpp
	ports/gtk/gtk_compat.h
	ports/gtk/gtk_netplay_dialog.cpp
	ports/gtk/gtk_netplay_dialog.h
	ports/gtk/gtk_netplay.cpp
	ports/gtk/gtk_netplay.h
	ports/gtk/background_particles.cpp
	ports/gtk/background_particles.h
    ${CMAKE_CURRENT_BINARY_DIR}/gtk_snes9x_ui.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/mini_icon.cpp)