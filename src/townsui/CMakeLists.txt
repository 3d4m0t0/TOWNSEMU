set(TARGET_NAME TownsUI)

add_executable(${TARGET_NAME} main.cpp menubar_connection.cpp towns_menu_ui_thread.cpp)

target_link_libraries(${TARGET_NAME}
	towns
	townscommand
	townsargv
	townsrender
	outside_world
	osinit
	fssimplewindow_connection
)

if(UNIX)
	target_link_libraries(${TARGET_NAME} pthread yssimplesound yssimplesound_platform)
endif()
