cmake_minimum_required( VERSION 3.20 )

if( NOT DEFINED ENV{PSPDEV} )
	message( FATAL_ERROR "PSPDEV env var not set. Source the PSPSDK environment first." )
endif()
if( NOT CMAKE_C_COMPILER MATCHES "psp-gcc" )
	message( FATAL_ERROR "This CMakeLists is PSP-only. Use psp-cmake (from PSPSDK) to configure." )
endif()

project( box3d_psp C )
set( CMAKE_C_STANDARD 17 )
set( CMAKE_C_STANDARD_REQUIRED ON )

list( APPEND CMAKE_MODULE_PATH "${PSPDEV}/psp/share" )
include( CreatePBP )

set( BOX3D_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../box3d" )
set( PSP_PORT_DIR   "${CMAKE_CURRENT_SOURCE_DIR}" )

file( GLOB BOX3D_SOURCES "${BOX3D_SRC_DIR}/src/*.c" )

add_library( box3d_psp_lib STATIC ${BOX3D_SOURCES} )
target_include_directories( box3d_psp_lib PUBLIC
	${BOX3D_SRC_DIR}/include
	${BOX3D_SRC_DIR}/src
	${PSP_PORT_DIR}/src
)
target_compile_definitions( box3d_psp_lib PUBLIC
	BOX3D_USER_CONFIG="${PSP_PORT_DIR}/src/psp_box3d_config.h"
)

target_compile_options( box3d_psp_lib PRIVATE
	-std=gnu17
	-O2
	-G0
	-march=allegrex
	-mtune=allegrex
	-mabi=eabi
	-Wall
	-Wno-unused-parameter
	-Wno-unused-function
	-Wno-unused-variable
	-Wno-unused-but-set-variable
	-Wno-sign-compare
	-Wno-format
	-Wno-strict-aliasing
	-Wno-incompatible-pointer-types
)

set( PSP_PORT_SOURCES
	${PSP_PORT_DIR}/src/psp_main.c
	${PSP_PORT_DIR}/src/psp_host.c
	${PSP_PORT_DIR}/src/vfpu_math.c
)

add_executable( box3d_psp ${PSP_PORT_SOURCES} )
target_link_libraries( box3d_psp PRIVATE box3d_psp_lib )

target_compile_options( box3d_psp PRIVATE
	-std=gnu17
	-O2
	-G0
	-march=allegrex
	-mtune=allegrex
	-mabi=eabi
	-Wall
	-Wno-unused-parameter
	-Wno-unused-function
	-Wno-unused-variable
	-Wno-format
)

target_link_options( box3d_psp PRIVATE
	-std=gnu17
	-O2
	-G0
	-march=allegrex
	-mtune=allegrex
	-mabi=eabi
	-Wl,-zmax-page-size=128
)

target_link_libraries( box3d_psp PRIVATE
	-lpspgu
	-lpspgum_vfpu
	-lpspvfpu
	-lpspdisplay
	-lpspctrl
	-lpspge
	-lpspdebug
	-lpsphprm
	-lpspsdk
	-lc
	-lpspuser
	-lpspkernel
)

create_pbp_file(
	TARGET  box3d_psp
	TITLE   "Box3D_PSP"
	VERSION "v1.0"
	MEMSIZE 1
)
