set(TARGET_NAME TownsQt)

find_package(Qt6 COMPONENTS Widgets OpenGLWidgets QUIET)
if(NOT Qt6_FOUND)
	message(STATUS "Qt6 not found — skipping TownsQt (install qt6-qtbase-devel)")
	return()
endif()
if(NOT Qt6OpenGLWidgets_FOUND)
	message(STATUS "Qt6 OpenGLWidgets not found — skipping TownsQt (install qt6-qtbase-devel-opengl or qt6-qtbase-devel)")
	return()
endif()

# Qt AutoMoc invokes the compiler as "launcher + flags + compiler"; ccache as launcher breaks this.
set(_TOWNSQT_SAVED_CXX_LAUNCHER "${CMAKE_CXX_COMPILER_LAUNCHER}")
set(CMAKE_CXX_COMPILER_LAUNCHER "")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC_COMPILER_PREDEFINES OFF)

add_executable(${TARGET_NAME}
	main_qt.cpp
	main_window.h
	main_window.cpp
	settings_dialog.h
	settings_dialog.cpp
	hdd_settings_dialog.h
	hdd_settings_dialog.cpp
	townsqt_model_profile.h
	townsqt_model_profile.cpp
	townsqt_rom_availability.h
	townsqt_rom_availability.cpp
	townsqt_app_profile.h
	townsqt_app_profile.cpp
	townsqt_argv_from_settings.h
	townsqt_argv_from_settings.cpp
	townsqt_gameport_options.h
	townsqt_gameport_options.cpp
	townsqt_miniaudio_devices.h
	townsqt_miniaudio_devices.cpp
	emulator_controller.h
	emulator_controller.cpp
	emu_view.h
	emu_view.cpp
	emu_gl_view.h
	emu_gl_view.cpp
	debug_text_window.h
	debug_text_window.cpp
	townsqt_drive_access_overlay.h
	townsqt_drive_access_overlay.cpp
	shared_rgba_framebuffer.cpp
	qt_input_queue.cpp
	qt_outside_world.cpp
	qt_command_thread.cpp
	qt_to_fskey.cpp
	townsqt_paths.cpp
	townsqt_settings.cpp
	townsqt_settings.h
	townsqt_version.h
	townsqt_i18n.h
	townsqt_i18n.cpp
	qt_sync_sound.cpp
	qt_miniaudio_impl.cpp
	townsqt_wayland_idle_inhibit.h
	townsqt_wayland_idle_inhibit.cpp
	townsqt_icons.qrc
	townsqt_i18n.qrc
)

# Binary name matches original convention: Tsugaru_CUI / Tsugaru_GUI → Tsugaru_QT
set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "Tsugaru_QT")

# m88-qt style: en/ja embedded; other locales from share/townsqt/translations/
message(STATUS "TownsQt: JSON i18n (TOWNSQT_LANG / share/townsqt/translations)")

if(WIN32)
	target_sources(${TARGET_NAME} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/../resources/tsugaru_icon.rc)
endif()

target_link_libraries(${TARGET_NAME}
	PRIVATE
		towns
		townscommand
		townsargv
		townsrender
		outside_world
		osinit
		midi_interface
		fssimplewindow_connection
		ysgamepad
		d77
		diskimg
		Qt6::Widgets
		Qt6::OpenGLWidgets
)

target_compile_definitions(${TARGET_NAME} PRIVATE QT_NO_KEYWORDS)

# Phase 1: 20 ms FM/PCM chunks (lower latency than default 40 ms on Linux).
target_compile_definitions(townssound PRIVATE TOWNSQT_AUDIO_20MS=1)
target_compile_definitions(towns PRIVATE TOWNSQT_SMOOTH_TIMING=1)

target_include_directories(${TARGET_NAME} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../externals/yssimplesound/src
	${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/miniaudio
	${CMAKE_CURRENT_SOURCE_DIR}/../osdependent/gamepad
)

if(UNIX)
	target_link_libraries(${TARGET_NAME} PRIVATE pthread dl m yssimplesound_nownd)

	find_package(PkgConfig QUIET)
	if(PkgConfig_FOUND)
		pkg_check_modules(TOWNSQT_WAYLAND_CLIENT QUIET wayland-client)
	endif()
	find_program(TOWNSQT_WAYLAND_SCANNER wayland-scanner)
	set(TOWNSQT_IDLE_INHIBIT_XML
	    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/wayland-protocols/idle-inhibit-unstable-v1.xml")
	if(TOWNSQT_WAYLAND_CLIENT_FOUND AND TOWNSQT_WAYLAND_SCANNER AND EXISTS "${TOWNSQT_IDLE_INHIBIT_XML}")
		set(TOWNSQT_IDLE_INHIBIT_CLIENT_H
		    "${CMAKE_CURRENT_BINARY_DIR}/idle-inhibit-unstable-v1-client-protocol.h")
		set(TOWNSQT_IDLE_INHIBIT_PRIVATE_C
		    "${CMAKE_CURRENT_BINARY_DIR}/idle-inhibit-unstable-v1-private-code.c")
		add_custom_command(
		    OUTPUT "${TOWNSQT_IDLE_INHIBIT_CLIENT_H}"
		    COMMAND "${TOWNSQT_WAYLAND_SCANNER}" client-header
		            "${TOWNSQT_IDLE_INHIBIT_XML}" "${TOWNSQT_IDLE_INHIBIT_CLIENT_H}"
		    DEPENDS "${TOWNSQT_IDLE_INHIBIT_XML}"
		    VERBATIM)
		add_custom_command(
		    OUTPUT "${TOWNSQT_IDLE_INHIBIT_PRIVATE_C}"
		    COMMAND "${TOWNSQT_WAYLAND_SCANNER}" private-code
		            "${TOWNSQT_IDLE_INHIBIT_XML}" "${TOWNSQT_IDLE_INHIBIT_PRIVATE_C}"
		    DEPENDS "${TOWNSQT_IDLE_INHIBIT_XML}"
		    VERBATIM)
		add_custom_target(townsqt_idle_inhibit_codegen
		    DEPENDS "${TOWNSQT_IDLE_INHIBIT_CLIENT_H}" "${TOWNSQT_IDLE_INHIBIT_PRIVATE_C}")
		add_dependencies(${TARGET_NAME} townsqt_idle_inhibit_codegen)
		target_sources(${TARGET_NAME} PRIVATE "${TOWNSQT_IDLE_INHIBIT_PRIVATE_C}")
		target_include_directories(${TARGET_NAME} PRIVATE
		    "${CMAKE_CURRENT_BINARY_DIR}"
		    ${TOWNSQT_WAYLAND_CLIENT_INCLUDE_DIRS})
		target_link_libraries(${TARGET_NAME} PRIVATE ${TOWNSQT_WAYLAND_CLIENT_LIBRARIES})
		target_compile_definitions(${TARGET_NAME} PRIVATE TOWNSQT_HAS_WAYLAND_IDLE_INHIBIT=1)
		message(STATUS "TownsQt: Wayland idle-inhibit enabled")
	else()
		message(STATUS "TownsQt: Wayland idle-inhibit disabled (wayland-client or wayland-scanner missing)")
	endif()
endif()

set(CMAKE_CXX_COMPILER_LAUNCHER "${_TOWNSQT_SAVED_CXX_LAUNCHER}")
