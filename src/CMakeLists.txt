cmake_minimum_required(VERSION 3.20)

# openSUSE suse-devel-config may set CXX="ccache g++", which breaks Qt AutoMoc.
foreach(_TSUGARU_COMPILER_VAR CC CXX)
	if(DEFINED ENV{${_TSUGARU_COMPILER_VAR}})
		string(STRIP "$ENV{${_TSUGARU_COMPILER_VAR}}" _TSUGARU_COMPILER_VALUE)
		if(_TSUGARU_COMPILER_VALUE MATCHES "^ccache[ ]+(.+)$")
			set(ENV{${_TSUGARU_COMPILER_VAR}} "${CMAKE_MATCH_1}")
		endif()
		unset(_TSUGARU_COMPILER_VALUE)
	endif()
endforeach()
unset(_TSUGARU_COMPILER_VAR)

project(TSUGARU)

# Uncomment the following line to compile CPU core in high-fidelity mode.
# add_compile_definitions(TSUGARU_I486_HIGH_FIDELITY)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX)
	set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} -Wno-unused-variable)
	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -Wno-unused-variable)
endif()


if(APPLE)
    set(DISC_IMAGE_DIR "$ENV{HOME}/TOWNSISO")
elseif(MSVC)
    set(DISC_IMAGE_DIR "C:/D/TownsISO")
else()
    set(DISC_IMAGE_DIR "$ENV{HOME}/TownsISO")
endif()

add_compile_definitions(YS_LITTLE_ENDIAN)
add_compile_definitions(YS_TWOS_COMPLEMENT)
# Believe or not, C/C++ does not assume 2's complement.
# C/C++ does not guarantee that 0xFFFF is interpreted as 16-bit signed negative one.
# In practice, pretty much all CPUs uses 2's complement, and safe to assume it is.
# This flag will skip a little bit of computation.
#
# Speaking of C/C++, you may think:
#    int x;
#    short y;
#    y=x;
# will simply drop high 16 bits of x and copy low 16 bits to y.
# It is what you get, in practice, but it is not guaranteed either.
# If someone comes up with a crazy CPU in the future, the code may break.

enable_testing()

add_subdirectory(resources)

add_subdirectory(osdependent)

add_subdirectory(vgmrecorder)

add_subdirectory(cpputil)
add_subdirectory(lineparser)
add_subdirectory(miscutil)
add_subdirectory(cheapmath)

add_subdirectory(vmbase)
add_subdirectory(device)
add_subdirectory(cpu)
add_subdirectory(inout)
add_subdirectory(ramrom)
add_subdirectory(ym2612)
add_subdirectory(ym2612tests)
add_subdirectory(rf5c68)
add_subdirectory(i8251)
add_subdirectory(i8251_to_socket)
add_subdirectory(i8253)
add_subdirectory(diskdrive)

add_subdirectory(customstick)

add_subdirectory(towns)

add_subdirectory(tests)

add_subdirectory(discimg)
add_subdirectory(diskimg)

add_subdirectory(main_cui)
add_subdirectory(main_headless)
add_subdirectory(townsqt)


#externals
add_subdirectory(externals)


#command-line utilities
add_subdirectory(cmdutil)


#test386
add_subdirectory(test386vm)


#Serial to TCP Server Sample
add_subdirectory(serversample)

if(UNIX)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/TownsQtInstall.cmake")
endif()
