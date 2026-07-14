set(TARGET_NAME midi_interface)
if(MSVC)
	add_library(${TARGET_NAME} midi_interface.h midi_interface.cpp windows/midi_implement.cpp)
	target_link_libraries(${TARGET_NAME} winmm.lib)
else()
	find_library(MIDI_ALSA_LIB NAMES asound)
	if(MIDI_ALSA_LIB)
		add_library(${TARGET_NAME}
		    midi_interface.h
		    midi_interface.cpp
		    linux/midi_linux_implement.cpp
		    linux/midi_fluidsynth_host.cpp
		    linux/midi_fluidsynth_host.h
		    linux/midi_alsa_seq_host.cpp
		    linux/midi_alsa_seq_host.h
		    linux/midi_backend_probe.cpp
		    linux/midi_backend_probe.h
		    linux/midi_output_ports.cpp
		    linux/midi_output_ports.h)
		target_link_libraries(${TARGET_NAME} PRIVATE ${MIDI_ALSA_LIB} dl pthread)
	else()
		add_library(${TARGET_NAME} midi_interface.h midi_interface.cpp null/midi_implement.cpp)
	endif()
endif()
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
