cmake_minimum_required(VERSION 2.6)

#
# Execute these commands in this directory:
#
# 1. mkdir build/; cd build/
#
# 2. Choose compiler:
#   Build with native toolchain:
#    cmake -DCMAKE_BUILD_TYPE=Debug ..
#
#   Build with mingw:
#    cmake -DCMAKE_TOOLCHAIN_FILE=../CMakeLists_mingw.txt -DCMAKE_BUILD_TYPE=Debug ..
#
# 3. make
#


# project title
set (PROJECT openrct2)
# OpenRCT2 resource directory
set (ORCT2_RESOURCE_DIR ${CMAKE_INSTALL_PREFIX}/share/${PROJECT}/)

project(${PROJECT})

if (CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
	message(FATAL_ERROR "Building in-source is not supported! Create a build dir and remove ${CMAKE_SOURCE_DIR}/CMakeCache.txt")
endif()

add_definitions(-DORCT2_RESOURCE_DIR="${ORCT2_RESOURCE_DIR}")
add_definitions(-DHAVE_CONFIG_H)
add_definitions(-DCURL_STATICLIB)

# Define current git branch.
execute_process(
	COMMAND git rev-parse --abbrev-ref HEAD
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE OPENRCT2_BRANCH
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)
add_definitions(-DOPENRCT2_BRANCH="${OPENRCT2_BRANCH}")

# Define commit hash.
execute_process(
	COMMAND git rev-parse HEAD
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE OPENRCT2_COMMIT_SHA1
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)
add_definitions(-DOPENRCT2_COMMIT_SHA1="${OPENRCT2_COMMIT_SHA1}")

# Define short commit hash.
execute_process(
	COMMAND git rev-parse --short HEAD
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE OPENRCT2_COMMIT_SHA1_SHORT
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)
add_definitions(-DOPENRCT2_COMMIT_SHA1_SHORT="${OPENRCT2_COMMIT_SHA1_SHORT}")

INCLUDE(FindPkgConfig)

if (APPLE)
	set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/opt/openssl/lib/pkgconfig")
endif (APPLE)

# Options

option(DISABLE_HTTP_TWITCH "Disable HTTP and Twitch support.")
if (DISABLE_HTTP_TWITCH)
	add_definitions(-DDISABLE_HTTP -DDISABLE_TWITCH)
endif (DISABLE_HTTP_TWITCH)

option(DISABLE_NETWORK "Disable multiplayer functionality. Mainly for testing.")
option(STATIC "Create a static build.")
option(FORCE64 "Force native (x86-64) build. Do not use, for experimental purposes only.")
option(DISABLE_OPENGL "Disable OpenGL support.")
option(DISABLE_RCT2 "WIP: Try building without using code and data segments from vanilla.")
option(USE_MMAP "Use mmap to try loading rct2's data segment into memory.")

if (FORCE64)
	set(TARGET_M "-m64")
	set(OBJ_FORMAT "elf64-x86-64")
	set(LINKER_SCRIPT "ld_script_x86_64.xc")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=pointer-to-int-cast -Wno-error=int-to-pointer-cast")
	if ((APPLE OR WIN32) AND NOT USE_MMAP)
		message(WARNING "Building such configuration won't work. Enabling USE_MMAP.")
		set(USE_MMAP ON)
	endif()
else ()
	set(TARGET_M "-m32")
	set(OBJ_FORMAT "elf32-i386")
	set(LINKER_SCRIPT "ld_script_i386.xc")
endif ()

if (DISABLE_OPENGL)
	add_definitions(-DDISABLE_OPENGL)
else (DISABLE_OPENGL)
	# Makes OpenGL function get queried in run-time rather than linked-in
	add_definitions(-DOPENGL_NO_LINK)
endif (DISABLE_OPENGL)

if (USE_MMAP)
	add_definitions(-DUSE_MMAP)
endif (USE_MMAP)

if (DISABLE_NETWORK)
	add_definitions(-DDISABLE_NETWORK)
else (DISABLE_NETWORK)
	if (WIN32)
		SET(NETWORKLIBS ${NETWORKLIBS} ws2_32)
	endif (WIN32)
	# If you are on OS X, CMake might try using system-provided OpenSSL.
	# This is too old and will not work.
	PKG_CHECK_MODULES(SSL REQUIRED openssl>=1.0.0)
endif (DISABLE_NETWORK)

if (DISABLE_RCT2)
	add_definitions(-DNO_RCT2)
endif (DISABLE_RCT2)

# Start of library checks

PKG_CHECK_MODULES(PNG libpng>=1.6)
if (NOT PNG_FOUND)
	PKG_CHECK_MODULES(PNG libpng16)
endif (NOT PNG_FOUND)
if (NOT PNG_FOUND)
	PKG_CHECK_MODULES(PNG libpng>=1.2)
endif (NOT PNG_FOUND)
if (NOT PNG_FOUND)
	PKG_CHECK_MODULES(PNG REQUIRED libpng12)
endif (NOT PNG_FOUND)

PKG_CHECK_MODULES(ZLIB REQUIRED zlib)
PKG_CHECK_MODULES(JANSSON REQUIRED jansson>=2.7)

# Handle creating the rct2 text and data files on OS X and Linux
# See details in src/openrct2.c:openrct2_setup_rct2_segment for how the values
# were derived.
if (UNIX)
  add_custom_command(
    OUTPUT openrct2_text
    COMMAND dd if=${CMAKE_CURRENT_SOURCE_DIR}/openrct2.exe of=${CMAKE_BINARY_DIR}/openrct2_text bs=4096 skip=1 count=1187
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/openrct2.exe
  )
  add_custom_command(
    OUTPUT openrct2_data
    COMMAND dd if=${CMAKE_CURRENT_SOURCE_DIR}/openrct2.exe of=${CMAKE_BINARY_DIR}/openrct2_data bs=4096 skip=1188 count=318
    COMMAND dd if=/dev/zero of=${CMAKE_BINARY_DIR}/openrct2_data bs=4096 seek=318 count=2630 conv=notrunc
    COMMAND dd if=${CMAKE_CURRENT_SOURCE_DIR}/openrct2.exe of=${CMAKE_BINARY_DIR}/openrct2_data bs=4096 skip=1506 seek=2948 count=1 conv=notrunc
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/openrct2.exe
  )
  add_custom_target(segfiles DEPENDS openrct2_text openrct2_data)
	if (NOT USE_MMAP)
		if (APPLE)
				set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -sectcreate rct2_text __text ${CMAKE_CURRENT_SOURCE_DIR}/build/openrct2_text -sectcreate rct2_data __data ${CMAKE_CURRENT_SOURCE_DIR}/build/openrct2_data -segaddr rct2_data 0x8a4000 -segprot rct2_data rwx rwx -segaddr rct2_text 0x401000 -segprot rct2_text rwx rwx -segaddr __TEXT 0x2000000 -fno-pie -read_only_relocs suppress")
		else (APPLE)
			# For Linux we have to use objcopy to wrap regular binaries into a linkable
			# format. We use specific section names which are then referenced in a
			# bespoke linker script so they can be placed at predefined VMAs.
			add_custom_command(
				OUTPUT openrct2_text_section.o
				COMMAND objcopy --input binary --output ${OBJ_FORMAT} --binary-architecture i386 openrct2_text openrct2_text_section.o --rename-section .data=.rct2_text,contents,alloc,load,readonly,code
				DEPENDS segfiles
				WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			)
			add_custom_command(
				OUTPUT openrct2_data_section.o
				COMMAND objcopy --input binary --output ${OBJ_FORMAT} --binary-architecture i386 openrct2_data openrct2_data_section.o --rename-section .data=.rct2_data,contents,alloc,load,readonly,data
				DEPENDS segfiles
				WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			)
			add_custom_target(linkable_sections DEPENDS openrct2_text_section.o openrct2_data_section.o)
			SET_SOURCE_FILES_PROPERTIES(
				openrct2_text_section.o openrct2_data_section.o
				PROPERTIES
				EXTERNAL_OBJECT true
				GENERATED true
			)
			# can't use GLOB here, as the files don't exist yet at cmake-time
			set(RCT2_SECTIONS "${CMAKE_BINARY_DIR}/openrct2_data_section.o" "${CMAKE_BINARY_DIR}/openrct2_text_section.o")
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-T,\"${CMAKE_CURRENT_SOURCE_DIR}/distribution/linux/${LINKER_SCRIPT}\"")
		endif (APPLE)
	endif (NOT USE_MMAP)
elseif (USE_MMAP)
	# No dd here, can't extract data segment
	message(WARNING "Sorry, your platform is not supported, you have to extract data segment manually")
endif (UNIX)
set(DEBUG_LEVEL 0 CACHE STRING "Select debug level for compilation. Use value in range 0–3.")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDEBUG=${DEBUG_LEVEL}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG=${DEBUG_LEVEL}")

# include lib
include_directories("lib/")
# add source files
file(GLOB_RECURSE ORCT2_SOURCES "src/*.c" "src/*.cpp" "src/*.h" "src/*.hpp")
if (APPLE)
	file(GLOB_RECURSE ORCT2_MM_SOURCES "src/*.m")
	set_source_files_properties(${ORCT2_MM_SOURCES} PROPERTIES COMPILE_FLAGS "-x objective-c -fmodules")
endif (APPLE)

# set necessary flags to compile code as is
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_M} -std=gnu99 -fno-pie -fstrict-aliasing -Werror -Wundef -Wmissing-declarations")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TARGET_M} -std=gnu++11 -fno-pie -fstrict-aliasing -Werror -Wundef -Wmissing-declarations")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${TARGET_M}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_SHARED_LINKER_FLAGS}")

if (MINGW)
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif ()

option(WITH_BREAKPAD "Enable breakpad")
if (WITH_BREAKPAD)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_BREAKPAD")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_BREAKPAD")
	set(BREAKPAD_DIR "/home/janisozaur/workspace/breakpad/src")
	set(BREAKPAD_INCLUDE_DIR "${BREAKPAD_DIR}/src")
	set(BREAKPAD_LIBRARY_DIR "${BREAKPAD_DIR}/src/client/linux")
	set(BREAKPAD_LIBS breakpad_client pthread)
endif (WITH_BREAKPAD)

# find and include SDL2
PKG_CHECK_MODULES(SDL2 REQUIRED sdl2 SDL2_ttf)
if (STATIC)
	# FreeType is required by SDL2_ttf, but not wired up properly in package
	PKG_CHECK_MODULES(FREETYPE REQUIRED freetype2)
	SET(SDL2LIBS ${SDL2_STATIC_LIBRARIES} ${FREETYPE_STATIC_LIBRARIES})
else (STATIC)
	SET(SDL2LIBS ${SDL2_LIBRARIES})
endif (STATIC)

if (STATIC)
	set(STATIC_START "-static")
	SET(REQUIREDLIBS ${PNG_STATIC_LIBRARIES} ${JANSSON_STATIC_LIBRARIES} ${ZLIB_STATIC_LIBRARIES} ${SSL_STATIC_LIBRARIES})
else (STATIC)
	SET(REQUIREDLIBS ${PNG_LIBRARIES} ${JANSSON_LIBRARIES} ${ZLIB_LIBRARIES} ${SSL_LIBRARIES})
endif (STATIC)

if (NOT DISABLE_OPENGL)
	if (WIN32)
		# Curl depends on openssl and ws2 in mingw builds, but is not wired up in pkg-config
		set(GLLIBS opengl32)
	elseif (APPLE)
		# GL doesn't work nicely with macOS, while find_package doesn't work with multiarch on Ubuntu.
		find_package(OpenGL REQUIRED)
		set(GLLIBS ${OPENGL_LIBRARY})
	else (WIN32)
		PKG_CHECK_MODULES(GL REQUIRED gl)
		set(GLLIBS ${GL_LIBRARIES})
	endif (WIN32)
endif (NOT DISABLE_OPENGL)

if (NOT DISABLE_HTTP_TWITCH)
	PKG_CHECK_MODULES(LIBCURL REQUIRED libcurl)
	if (WIN32)
		# Curl depends on openssl and ws2 in mingw builds, but is not wired up in pkg-config
		set(WSLIBS ws2_32)
	endif (WIN32)
	if (STATIC)
		SET(HTTPLIBS ${LIBCURL_STATIC_LIBRARIES} ${WSLIBS})
	else (STATIC)
		SET(HTTPLIBS ${LIBCURL_LIBRARIES} ${WSLIBS})
	endif (STATIC)
endif (NOT DISABLE_HTTP_TWITCH)

PKG_CHECK_MODULES(SPEEX REQUIRED speexdsp)

if (UNIX)
	# Include libdl for dlopen
	set(DLLIB dl)
endif (UNIX)

INCLUDE_DIRECTORIES(${SDL2_INCLUDE_DIRS} ${LIBCURL_INCLUDE_DIRS} ${JANSSON_INCLUDE_DIRS} ${SPEEX_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${BREAKPAD_INCLUDE_DIR} ${SSL_INCLUDE_DIRS})

LINK_DIRECTORIES(${SDL2_LIBRARY_DIRS} ${JANSSON_LIBRARY_DIRS} ${LIBCURL_LIBRARY_DIRS} ${PNG_LIBRARY_DIRS} ${ZLIB_LIBRARY_DIRS} ${BREAKPAD_LIBRARY_DIR} ${SSL_LIBRARY_DIRS})

if (NOT DISABLE_RCT2)
	# Disable optimizations for addresses.c for all compilers, to allow optimized
	# builds without need for -fno-omit-frame-pointer
	set_source_files_properties(src/addresses.c PROPERTIES COMPILE_FLAGS -O0)
endif (NOT DISABLE_RCT2)

if (WIN32)
	# build as library for now, replace with add_executable
	if (USE_MMAP)
		add_executable(${PROJECT} ${ORCT2_SOURCES} ${SPEEX_SOURCES})
	else ()
		add_library(${PROJECT} SHARED ${ORCT2_SOURCES} ${SPEEX_SOURCES})
	endif ()
else (WIN32)
	add_executable(${PROJECT} ${ORCT2_SOURCES} ${ORCT2_MM_SOURCES} ${RCT2_SECTIONS})
	add_dependencies(${PROJECT} segfiles)
	if (NOT APPLE AND NOT USE_MMAP)
		add_dependencies(${PROJECT} linkable_sections)
	endif ()
	add_custom_command(
		OUTPUT g2.dat
		COMMAND ./openrct2 sprite build ${CMAKE_BINARY_DIR}/g2.dat ${CMAKE_CURRENT_SOURCE_DIR}/resources/g2/
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		)
	add_custom_target(g2 DEPENDS ${PROJECT} g2.dat)
endif (WIN32)

if (UNIX AND NOT APPLE)
	# FontConfig for TrueType fonts.
	PKG_CHECK_MODULES(FONTCONFIG REQUIRED fontconfig)
	INCLUDE_DIRECTORIES(${FONTCONFIG_INCLUDE_DIRS})
	TARGET_LINK_LIBRARIES(${PROJECT} ${FONTCONFIG_LIBRARIES})
endif (UNIX AND NOT APPLE)


# install into ${CMAKE_INSTALL_PREFIX}/bin/
#install (TARGETS ${PROJECT} DESTINATION bin)

# libopenrct2.dll -> openrct2.dll
set_target_properties(${PROJECT} PROPERTIES PREFIX "")

# Link shared libs first
TARGET_LINK_LIBRARIES(${PROJECT} ${GLLIBS})
# if creating a static binary, precede libraries with -static, then name all the libs
TARGET_LINK_LIBRARIES(${PROJECT} ${STATIC_START} ${SDL2LIBS} ${HTTPLIBS} ${NETWORKLIBS} ${SPEEX_LIBRARIES} ${DLLIB} ${REQUIREDLIBS} ${BREAKPAD_LIBS})

if (APPLE OR STATIC)
	FIND_LIBRARY(ICONV_LIBRARIES NAMES iconv libiconv libiconv-2 c)
	TARGET_LINK_LIBRARIES(${PROJECT} ${ICONV_LIBRARIES})
endif (APPLE OR STATIC)

# Don't recurse, grab all *.txt and *.md files
file(GLOB DOC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/distribution/*.txt")
list(APPEND DOC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/contributors.md" "${CMAKE_CURRENT_SOURCE_DIR}/licence.txt")

# CMake does not allow specifying a dependency chain which includes built-in
# targets, like `install`, so we have to trick it and execute dependency ourselves.
install(CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\" --build \"${CMAKE_CURRENT_BINARY_DIR}\" --target g2)")
install(TARGETS ${PROJECT} RUNTIME DESTINATION bin)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/g2.dat" DESTINATION share/${PROJECT})
install(DIRECTORY data/ DESTINATION share/${PROJECT})
install(FILES ${DOC_FILES} DESTINATION share/doc/${PROJECT})

set(CPACK_PACKAGE_VERSION_MAJOR 0)
set(CPACK_PACKAGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 5)
INCLUDE(CPack)
