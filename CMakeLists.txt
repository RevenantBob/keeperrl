cmake_minimum_required (VERSION 3.16)

# vcpkg toolchain must be set before project(). Only used on Windows for now;
# the Linux/Mac path below still uses system find_package() as before.
if(WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "vcpkg toolchain file")
endif()

project(keeper)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# clang/gcc-style flags the codebase relies on (also accepted by clang-cl's GNU passthrough).
# Left off entirely for a plain MSVC (cl.exe) build.
if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-sign-compare -Wno-unused-variable -Wno-shift-count-overflow -ftemplate-depth=512 -Wno-tautological-constant-out-of-range-compare -Wno-mismatched-tags")
endif()

set(STDAFX_H "${CMAKE_CURRENT_SOURCE_DIR}/stdafx.h")
set(STDAFX_H_GCH "${CMAKE_CURRENT_BINARY_DIR}/stdagx.h.gch")

# set debug cxxflags
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wimplicit-fallthrough -Wno-unused-function")

if(NOT WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld -Wl,-rpath=. -Wl,--gdb-index")
endif()

# additional cmake modules directory (used by the Linux/Mac MODULE-mode find_package calls below)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

if(WIN32)
    find_program(BASH_EXECUTABLE bash PATHS "C:/Program Files/Git/bin" NO_DEFAULT_PATH)
else()
    find_program(BASH_EXECUTABLE bash)
endif()

# find required libraries
find_package(OpenGL REQUIRED)
if(APPLE)
    # assume built-in pthreads on MacOS
    set(CMAKE_THREAD_LIBS_INIT "-lpthread")
    set(CMAKE_HAVE_THREADS_LIBRARY 1)
    set(CMAKE_USE_WIN32_THREADS_INIT 0)
    set(CMAKE_USE_PTHREADS_INIT 1)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
else()
    find_package(Threads REQUIRED)
endif()

if(WIN32)
    # SDL3 is vendored from source and built as part of this project.
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/SDL-release-3.4.4" EXCLUDE_FROM_ALL)

    # Everything else comes from vcpkg (via CMAKE_TOOLCHAIN_FILE set above).
    find_package(CURL CONFIG REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(OpenAL CONFIG REQUIRED)
    find_package(Ogg CONFIG REQUIRED)
    find_package(Vorbis CONFIG REQUIRED)
    find_package(unofficial-theora CONFIG REQUIRED)
else()
    find_package(SDL2 REQUIRED)
    find_package(SDL2_image REQUIRED)
    find_package(CURL REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(OpenAL REQUIRED)
    find_package(Vorbis REQUIRED)
    find_package(Ogg REQUIRED)
    find_package(VorbisFile REQUIRED)
    find_package(THEORA REQUIRED)
endif()

# generate version.h
file(GLOB GENERATE_VERSION_SH "gen_version.sh")
set(KEEPER_VERSION_H "${CMAKE_CURRENT_SOURCE_DIR}/version.h")
add_custom_command(
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/
    COMMAND ${BASH_EXECUTABLE} ${GENERATE_VERSION_SH}
    DEPENDS ${GENERATE_VERSION_SH}
    OUTPUT ${KEEPER_VERSION_H}
    COMMENT "Generating version.h"
)

# check member serialization
# the stamp file is not actually generated, but it doesn't seem to matter
# Skipped on Windows for now: check_serial.sh relies on the `rev` coreutil,
# which Git-for-Windows' minimal bash doesn't ship.
set(CHECK_SERIAL_STAMP "")
if(NOT WIN32)
    file(GLOB CHECK_SERIAL_SH "check_serial.sh")
    set(CHECK_SERIAL_STAMP "${CMAKE_CURRENT_SOURCE_DIR}/check_serial.stamp")
    add_custom_command(
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/
        COMMAND ${BASH_EXECUTABLE} ${CHECK_SERIAL_SH}
        DEPENDS ${CHECK_SERIAL_SH}
        OUTPUT ${CHECK_SERIAL_STAMP}
        COMMENT "Checking serialization"
    )
endif()

# rules to create `keeper` binary
# Steamworks SDK isn't vendored in this checkout, so steam_*.cpp is excluded for now.
file(GLOB SOURCES "*.cpp" "extern/*.cpp")
list(FILTER SOURCES EXCLUDE REGEX ".*/steam_[^/]*\\.cpp$")
# steam_input.cpp/steam_achievements.cpp compile fine without the SDK: their Steamworks-dependent
# code is guarded by #ifdef USE_STEAMWORKS, falling back to no-op stubs otherwise. The other
# steam_*.cpp files include real Steamworks SDK headers unconditionally and stay excluded.
list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/steam_input.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/steam_achievements.cpp")
add_executable(keeper ${SOURCES} ${KEEPER_VERSION_H} ${CHECK_SERIAL_STAMP})
target_include_directories(keeper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(keeper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/extern)

if(WIN32)
    target_compile_definitions(keeper PRIVATE WINDOWS)
    # The codebase's own CHECK/ASSERT/PASSERT macros (fx_base.h) gate on !defined(NDEBUG)
    # rather than using <cassert>, and some call sites (e.g. fx_particle_system.cpp) chain
    # `<<` onto them assuming they always expand to something -- so NDEBUG must stay undefined,
    # matching how the project's own Makefiles build it (they never define NDEBUG either).
    target_compile_options(keeper PRIVATE -UNDEBUG)
    target_link_options(keeper PRIVATE /ERRORLIMIT:0)
    target_link_libraries(keeper PRIVATE
        SDL3::SDL3
        OpenGL::GL
        Threads::Threads
        CURL::libcurl
        ZLIB::ZLIB
        OpenAL::OpenAL
        Ogg::ogg
        Vorbis::vorbis
        Vorbis::vorbisfile
        unofficial::theora::theoradec
        Dbghelp.lib
        )
else()
    target_include_directories(keeper PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(keeper PRIVATE
        ${SDL2_LIBRARIES}
        ${SDL2_IMAGE_LIBRARY}
        ${OPENGL_gl_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT}
        ${CURL_LIBRARIES}
        ${ZLIB_LIBRARIES}
        ${OPENAL_LIBRARY}
        ${VORBIS_LIBRARIES}
        ${OGG_LIBRARIES}
        ${THEORA_dec_LIBRARY}
        ${VorbisFile_LIBRARIES}
        )
endif()

# set up definitions
if ((${CMAKE_BUILD_TYPE} MATCHES "Release"))
    target_compile_definitions(keeper PRIVATE RELEASE=1)
elseif(NOT WIN32)
# generate stdafx.h.gch for debug build (Linux/Mac only for now)
# workaround to unquote the cxxflags
set(CXX_FLAGS_LIST ${CMAKE_CXX_FLAGS})
separate_arguments(CXX_FLAGS_LIST)
add_custom_command(
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/
    COMMAND ${CMAKE_CXX_COMPILER} ${CXX_FLAGS_LIST} -x c++-header ${STDAFX_H} -MMD -o ${STDAFX_H_GCH}
    DEPENDS ${STDAFX_H}
    OUTPUT ${STDAFX_H_GCH}
    COMMENT "Generating ${STDAFX_H_GCH}"
)
if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -include-pch ${STDAFX_H_GCH}")
endif()
target_sources(keeper PRIVATE ${STDAFX_H_GCH})
endif()

if(ENABLE_LOCAL_USER_DIR)
    target_compile_definitions(keeper PRIVATE ENABLE_LOCAL_USER_DIR=1)
endif()
if(SANITIZE)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()
if(PROF)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
endif()
if(EASY_PROFILER)
    target_compile_definitions(keeper PRIVATE EASY_PROFILER=1)
    target_link_libraries(keeper PRIVATE libeasy_profiler)
endif()
