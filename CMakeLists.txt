cmake_minimum_required( VERSION 3.8 )

include(tools/arch.cmake)
include(tools/os.cmake)
include(tools/subdirs.cmake)

set(appname dmc)
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_EXTENSIONS ON )
set( CXX_STANDARD_REQUIRED ON)

project(${appname})

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
	set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
	set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

file(GLOB_RECURSE src_list "src/*.c*")
add_executable(${appname} ${src_list})

set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin/${OS}_${ARCH}_${BUILD_TYPE})
set(FIBJS_DIR ${PROJECT_SOURCE_DIR}/fibjs/bin/${OS}_${ARCH}_${BUILD_TYPE})

set(EOS_DIR ${PROJECT_SOURCE_DIR}/eos)
set(EOS_BUILD_DIR ${EOS_DIR}/build/${OS}_${BUILD_TYPE})

set(OPENSSL_DIR ${PROJECT_SOURCE_DIR}/openssl)
set(OPENSSL_BUILD_DIR ${OPENSSL_DIR}/out.build/${OS}_${BUILD_TYPE})

set(EXECUTABLE_OUTPUT_PATH ${BIN_DIR})

configure_file(${PROJECT_SOURCE_DIR}/tools/gitinfo.h.in ${CMAKE_CURRENT_BINARY_DIR}/gitinfo.h)

list(APPEND CMAKE_MODULE_PATH "${EOS_DIR}/libraries/fc/CMakeModules")

include(CheckIncludeFiles)
include(CheckCSourceCompiles)

set(CMAKE_C_FLAGS "${BUILD_OPTION}")

set(flags "-fsigned-char -fmessage-length=0 -fdata-sections -ffunction-sections -D_FILE_OFFSET_BITS=64")
set(ccflags "-std=c++17")
set(link_flags " ")

if(${OS} STREQUAL "Darwin")
 	set( CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -Wno-deprecated-declarations" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-deprecated-declarations" )

	set(flags "${flags} -Wno-nullability-completeness -mmacosx-version-min=10.15 -DOBJC_OLD_DISPATCH_PROTOTYPES=1")
	set(link_flags "${link_flags} -framework WebKit -framework Cocoa -framework Carbon -framework IOKit -mmacosx-version-min=10.15")

	target_link_libraries(${appname} dl iconv stdc++)

	find_library(security_framework security)
	find_library(localauthentication_framework localauthentication)
	find_library(corefoundation_framework corefoundation)
	find_library(cocoa_framework cocoa)
	target_link_libraries(${appname} ${security_framework} ${corefoundation_framework} ${localauthentication_framework} ${cocoa_framework})
endif()

if(${OS} STREQUAL "Linux")
	target_link_libraries(${appname} dl rt)
endif()

if(${OS} STREQUAL "FreeBSD")
	find_library(execinfo execinfo "/usr/local/lib" "/usr/lib")
	target_link_libraries(${appname} ${execinfo})
endif()

if(${BUILD_TYPE} STREQUAL "release")
	set(flags "${flags} -O3 -s ${BUILD_OPTION} -w -fvisibility=hidden")

	if(${OS} STREQUAL "FreeBSD")
		set(flags "${flags} -fno-omit-frame-pointer")
	else()
		set(flags "${flags} -fomit-frame-pointer")
	endif()

	set(link_flags "${link_flags} ${BUILD_OPTION}")
	add_definitions(-DNDEBUG=1)

	if(${OS} STREQUAL "Linux")
		set(link_flags "${link_flags} -static-libstdc++ -static-libgcc -Wl,--no-as-needed")
	endif()
endif()

if(${BUILD_TYPE} STREQUAL "debug")
	set(flags "${flags} -g -O0 ${BUILD_OPTION} -Wall -Wno-overloaded-virtual")
	set(link_flags "${link_flags} ${BUILD_OPTION}")
	add_definitions(-DDEBUG=1)
endif()

set(CMAKE_C_FLAGS "${flags}")
set(CMAKE_CXX_FLAGS "${flags} ${ccflags}")

add_definitions(-DEOSIO_EOS_VM_RUNTIME_ENABLED)

include_directories("${PROJECT_SOURCE_DIR}/include" 
	"${PROJECT_SOURCE_DIR}/fibjs/fibjs/include" 
	"${PROJECT_SOURCE_DIR}/fibjs/vender" 
	"${PROJECT_SOURCE_DIR}/fibjs/vender/v8" 
	"${PROJECT_SOURCE_DIR}/fibjs/vender/v8/include" 
	"${PROJECT_SOURCE_DIR}/fibjs/vender/uv/include" 
	"${OPENSSL_BUILD_DIR}/include"
	"${OPENSSL_DIR}/include" 
	"${CMAKE_CURRENT_BINARY_DIR}")

MACRO(EOSLIBS dir)
	file(GLOB eos_libs "${dir}/*")
	foreach(eos_lib ${eos_libs})
		if(IS_DIRECTORY "${eos_lib}/include")
			include_directories("${eos_lib}/include")
		endif()
	endforeach()
ENDMACRO()

EOSLIBS("${EOS_DIR}/plugins")
EOSLIBS("${EOS_DIR}/libraries")

include_directories("${EOS_DIR}/libraries")
include_directories("${EOS_DIR}/plugins")
include_directories("${EOS_DIR}/libraries/builtins")
include_directories("${EOS_DIR}/libraries/softfloat/source/include")
include_directories("${EOS_DIR}/libraries/wasm-jit/Include")
include_directories("${EOS_BUILD_DIR}/programs")
include_directories("${EOS_BUILD_DIR}/libraries/chain/include")

MACRO(link_libs dir)
	file(GLOB_RECURSE _libs "${dir}/*.a")
	target_link_libraries(${appname} ${_libs})
ENDMACRO()

link_libs("${EOS_BUILD_DIR}/plugins")
link_libs("${EOS_BUILD_DIR}/libraries")
link_libs("${EOS_BUILD_DIR}/libraries/builtins")

SET( Boost_USE_STATIC_LIBS ON CACHE STRING "ON or OFF" )
FIND_PACKAGE(Boost REQUIRED COMPONENTS
    date_time
    filesystem
    system
    program_options
    chrono
    unit_test_framework
    iostreams)
	
include_directories(${Boost_INCLUDE_DIR})
target_link_libraries(${appname} ${Boost_LIBRARIES})

set(fibjs_libs fibjs jemalloc expat gumbo gd tiff jpeg png webp zlib leveldb snappy msgpack pcre sqlite mongo umysql uuid exif mbedtls curve25519 blst blake2 v8 unzip uv exlib ev editline)

foreach(lib ${fibjs_libs})
	target_link_libraries(${appname} "${FIBJS_DIR}/lib${lib}.a")
endforeach()

target_link_libraries(${appname} 
	${OPENSSL_BUILD_DIR}/libssl.a
	${OPENSSL_BUILD_DIR}/libcrypto.a
)

target_link_libraries(${appname} pthread)

if(${OS} STREQUAL "Linux")
	target_link_libraries(${appname} util gmp)
else()
	target_link_libraries(${appname} /usr/local/opt/gmp/lib/libgmp.a)
endif()

set_target_properties(${appname} PROPERTIES LINK_FLAGS ${link_flags})
