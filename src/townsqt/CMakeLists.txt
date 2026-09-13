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
	townsqt_hdd_townsos.h
	townsqt_hdd_townsos.cpp
	townsqt_hdd_templates.qrc
	drive_config_page.h
	drive_config_page.cpp
	audio_mixer_dialog.h
	audio_mixer_dialog.cpp
	townsqt_model_profile.h
	townsqt_model_profile.cpp
	townsqt_cpu_profile.h
	townsqt_cpu_profile.cpp
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
	cpu_debug_window.h
	cpu_debug_window.cpp
	cdrom_monitor_window.h
	cdrom_monitor_window.cpp
	mouse_coord_scan_window.h
	mouse_coord_scan_window.cpp
	mouse_coord_profile_page.h
	mouse_coord_profile_page.cpp
	townsqt_mouse_preset.h
	townsqt_mouse_preset.cpp
	townsqt_mouse_presets.qrc
	townsqt_drive_access_overlay.h
	townsqt_drive_access_overlay.cpp
	shared_rgba_framebuffer.cpp
	qt_input_queue.cpp
	qt_outside_world.cpp
	qt_command_thread.cpp
	qt_to_fskey.cpp
	townsqt_paths.cpp
	townsqt_disc_statesave.cpp
	townsqt_settings.cpp
	townsqt_settings.h
	townsqt_version.h
	townsqt_i18n.h
	townsqt_i18n.cpp
	qt_sync_sound.cpp
	qt_miniaudio_impl.cpp
	townsqt_wayland_idle_inhibit.h
	townsqt_wayland_idle_inhibit.cpp
	townsqt_wayland_relative_pointer.h
	townsqt_wayland_relative_pointer.cpp
	townsqt_icons.qrc
	townsqt_i18n.qrc
)

# Binary name matches original convention: Tsugaru_CUI / Tsugaru_GUI → Tsugaru_QT
set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "Tsugaru_QT")

# Ship mouse_XXXXXXXX.ini drop-ins without editing the static qrc.
file(GLOB TOWNSQT_MOUSE_PRESET_FP
	CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/mouse_presets/mouse_*.ini")
if(TOWNSQT_MOUSE_PRESET_FP)
	set(_TOWNSQT_MOUSE_PRESET_FP_QRC
	    "${CMAKE_CURRENT_BINARY_DIR}/townsqt_mouse_presets_fp.qrc")
	file(WRITE "${_TOWNSQT_MOUSE_PRESET_FP_QRC}"
	    "<!DOCTYPE RCC>\n<RCC version=\"1.0\">\n  <qresource prefix=\"/mouse_presets\">\n")
	foreach(_f IN LISTS TOWNSQT_MOUSE_PRESET_FP)
		get_filename_component(_name "${_f}" NAME)
		file(APPEND "${_TOWNSQT_MOUSE_PRESET_FP_QRC}"
		    "    <file alias=\"${_name}\">${_f}</file>\n")
	endforeach()
	file(APPEND "${_TOWNSQT_MOUSE_PRESET_FP_QRC}" "  </qresource>\n</RCC>\n")
	target_sources(${TARGET_NAME} PRIVATE "${_TOWNSQT_MOUSE_PRESET_FP_QRC}")
endif()

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

# TownsQt: FM/PCM synthesis chunk matches DAC period (~12 ms; see sound.h TOWNSQT_AUDIO_20MS).
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
	set(TOWNSQT_RELATIVE_POINTER_XML
	    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/wayland-protocols/relative-pointer-unstable-v1.xml")
	set(TOWNSQT_POINTER_CONSTRAINTS_XML
	    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/wayland-protocols/pointer-constraints-unstable-v1.xml")
	if(TOWNSQT_WAYLAND_CLIENT_FOUND AND TOWNSQT_WAYLAND_SCANNER)
		set(TOWNSQT_WAYLAND_PROTO_SRCS "")
		set(TOWNSQT_WAYLAND_DEFS "")

		if(EXISTS "${TOWNSQT_IDLE_INHIBIT_XML}")
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
			list(APPEND TOWNSQT_WAYLAND_PROTO_SRCS
			    "${TOWNSQT_IDLE_INHIBIT_CLIENT_H}"
			    "${TOWNSQT_IDLE_INHIBIT_PRIVATE_C}")
			list(APPEND TOWNSQT_WAYLAND_DEFS TOWNSQT_HAS_WAYLAND_IDLE_INHIBIT=1)
			message(STATUS "TownsQt: Wayland idle-inhibit enabled")
		else()
			message(STATUS "TownsQt: Wayland idle-inhibit disabled (XML missing)")
		endif()

		if(EXISTS "${TOWNSQT_RELATIVE_POINTER_XML}" AND EXISTS "${TOWNSQT_POINTER_CONSTRAINTS_XML}")
			set(TOWNSQT_RELATIVE_POINTER_CLIENT_H
			    "${CMAKE_CURRENT_BINARY_DIR}/relative-pointer-unstable-v1-client-protocol.h")
			set(TOWNSQT_RELATIVE_POINTER_PRIVATE_C
			    "${CMAKE_CURRENT_BINARY_DIR}/relative-pointer-unstable-v1-private-code.c")
			set(TOWNSQT_POINTER_CONSTRAINTS_CLIENT_H
			    "${CMAKE_CURRENT_BINARY_DIR}/pointer-constraints-unstable-v1-client-protocol.h")
			set(TOWNSQT_POINTER_CONSTRAINTS_PRIVATE_C
			    "${CMAKE_CURRENT_BINARY_DIR}/pointer-constraints-unstable-v1-private-code.c")
			add_custom_command(
			    OUTPUT "${TOWNSQT_RELATIVE_POINTER_CLIENT_H}"
			    COMMAND "${TOWNSQT_WAYLAND_SCANNER}" client-header
			            "${TOWNSQT_RELATIVE_POINTER_XML}" "${TOWNSQT_RELATIVE_POINTER_CLIENT_H}"
			    DEPENDS "${TOWNSQT_RELATIVE_POINTER_XML}"
			    VERBATIM)
			add_custom_command(
			    OUTPUT "${TOWNSQT_RELATIVE_POINTER_PRIVATE_C}"
			    COMMAND "${TOWNSQT_WAYLAND_SCANNER}" private-code
			            "${TOWNSQT_RELATIVE_POINTER_XML}" "${TOWNSQT_RELATIVE_POINTER_PRIVATE_C}"
			    DEPENDS "${TOWNSQT_RELATIVE_POINTER_XML}"
			    VERBATIM)
			add_custom_command(
			    OUTPUT "${TOWNSQT_POINTER_CONSTRAINTS_CLIENT_H}"
			    COMMAND "${TOWNSQT_WAYLAND_SCANNER}" client-header
			            "${TOWNSQT_POINTER_CONSTRAINTS_XML}" "${TOWNSQT_POINTER_CONSTRAINTS_CLIENT_H}"
			    DEPENDS "${TOWNSQT_POINTER_CONSTRAINTS_XML}"
			    VERBATIM)
			add_custom_command(
			    OUTPUT "${TOWNSQT_POINTER_CONSTRAINTS_PRIVATE_C}"
			    COMMAND "${TOWNSQT_WAYLAND_SCANNER}" private-code
			            "${TOWNSQT_POINTER_CONSTRAINTS_XML}" "${TOWNSQT_POINTER_CONSTRAINTS_PRIVATE_C}"
			    DEPENDS "${TOWNSQT_POINTER_CONSTRAINTS_XML}"
			    VERBATIM)
			list(APPEND TOWNSQT_WAYLAND_PROTO_SRCS
			    "${TOWNSQT_RELATIVE_POINTER_CLIENT_H}"
			    "${TOWNSQT_RELATIVE_POINTER_PRIVATE_C}"
			    "${TOWNSQT_POINTER_CONSTRAINTS_CLIENT_H}"
			    "${TOWNSQT_POINTER_CONSTRAINTS_PRIVATE_C}")
			list(APPEND TOWNSQT_WAYLAND_DEFS TOWNSQT_HAS_WAYLAND_RELATIVE_POINTER=1)
			message(STATUS "TownsQt: Wayland relative-pointer enabled")
		else()
			message(STATUS "TownsQt: Wayland relative-pointer disabled (XML missing)")
		endif()

		if(TOWNSQT_WAYLAND_PROTO_SRCS)
			add_custom_target(townsqt_wayland_codegen DEPENDS ${TOWNSQT_WAYLAND_PROTO_SRCS})
			add_dependencies(${TARGET_NAME} townsqt_wayland_codegen)
			foreach(_src IN LISTS TOWNSQT_WAYLAND_PROTO_SRCS)
				if(_src MATCHES "\\.c$")
					target_sources(${TARGET_NAME} PRIVATE "${_src}")
				endif()
			endforeach()
			target_include_directories(${TARGET_NAME} PRIVATE
			    "${CMAKE_CURRENT_BINARY_DIR}"
			    ${TOWNSQT_WAYLAND_CLIENT_INCLUDE_DIRS})
			target_link_libraries(${TARGET_NAME} PRIVATE ${TOWNSQT_WAYLAND_CLIENT_LIBRARIES})
			target_compile_definitions(${TARGET_NAME} PRIVATE ${TOWNSQT_WAYLAND_DEFS})
		endif()
	else()
		message(STATUS "TownsQt: Wayland protocols disabled (wayland-client or wayland-scanner missing)")
	endif()
endif()

set(CMAKE_CXX_COMPILER_LAUNCHER "${_TOWNSQT_SAVED_CXX_LAUNCHER}")
