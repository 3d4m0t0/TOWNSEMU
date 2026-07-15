# FHS-style install rules for TownsQt (Linux packagers / local install).

include(GNUInstallDirs)

if(NOT TARGET TownsQt)
	return()
endif()

set(TOWNSQT_DOCDIR "${CMAKE_INSTALL_DATADIR}/doc/townsqt")

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/data/townsqt.desktop.in"
	"${CMAKE_CURRENT_BINARY_DIR}/townsqt.desktop"
	@ONLY)

install(TARGETS TownsQt
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/townsqt.desktop"
	DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

foreach(_TOWNSQT_ICON_SIZE 16 32 48 64 128 256 512)
	install(FILES
		"${CMAKE_CURRENT_SOURCE_DIR}/resources/tsugaru_icon_${_TOWNSQT_ICON_SIZE}.png"
		DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_TOWNSQT_ICON_SIZE}x${_TOWNSQT_ICON_SIZE}/apps"
		RENAME "townsqt.png")
endforeach()

file(GLOB TOWNSQT_EXTERNAL_TRANSLATIONS
	"${CMAKE_CURRENT_SOURCE_DIR}/townsqt/translations/townsqt_*.json")
list(FILTER TOWNSQT_EXTERNAL_TRANSLATIONS EXCLUDE REGEX ".*/townsqt_(en|ja)\\.json$")
if(TOWNSQT_EXTERNAL_TRANSLATIONS)
	install(FILES ${TOWNSQT_EXTERNAL_TRANSLATIONS}
		DESTINATION ${CMAKE_INSTALL_DATADIR}/townsqt/translations)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../BUILD_LINUX.md")
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../BUILD_LINUX.md"
		DESTINATION ${TOWNSQT_DOCDIR})
endif()

message(STATUS "Install rules: ${CMAKE_INSTALL_BINDIR}/Tsugaru_QT, "
               "${CMAKE_INSTALL_DATADIR}/applications/townsqt.desktop, "
               "${CMAKE_INSTALL_DATADIR}/townsqt/translations/")
