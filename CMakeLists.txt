# Current dependencies:
#   Git
#   SDL2
#   Boost

set(NativePrecompiledHeaders 1)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type")
endif()
set(CMAKE_BUILD_TYPE_VALUES "Debug;Release" CACHE INTERNAL "List of supported build")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${CMAKE_BUILD_TYPE_VALUES})

cmake_minimum_required(VERSION 3.0)
project(cathook VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
add_library(cathook SHARED "")

set(GameSpecific 1 CACHE BOOL "Build for specific target game (As opposed to universal, but slower, lib)")
set(Game "tf2" CACHE STRING "Target game")
set(GameValues "tf2;hl2dm;dab;tf2c;css;dynamic" CACHE INTERNAL "List of supported game types")
set_property(CACHE Game PROPERTY STRINGS ${GameValues})

set(EnableProfiler 0 CACHE BOOL "Enable Profiler")
set(EnableVisuals 1 CACHE BOOL "Enable Visuals")
set(ExternalDrawing 0 CACHE BOOL "External Visuals")
set(EnableEngineDrawing 0 CACHE BOOL "Draw using engine")
set(EnableImGuiDrawing 1 CACHE BOOL "Draw using ImGui's drawlists")
set(EnableGUI 1 CACHE BOOL "Enable GUI")
set(EnableIPC 1 CACHE BOOL "Enable IPC")
set(DataPath "/opt/cathook/data" CACHE FILEPATH "Data location")
set(VACBypass 0 CACHE BOOL "Textmode VAC bypass")
set(Textmode 0 CACHE BOOL "Various textmode-only features for bots")
set(EnableTextmodeStdin 0 CACHE BOOL "Textmode Stdin -> Console bridge (EXPERIMENTAL)")
set(EnableWarnings 0 CACHE BOOL "Enable compile warnings")
set(EnableNullGraphics 0 CACHE BOOL "Enable experimental textmode hooks (CRASHES)")

if (NOT DEFINED Internal_Binarymode)
    set(Internal_Binarymode 0 CACHE INTERNAL "Build cathook to be used in 'binary mode'")
endif()
if (NOT DEFINED Internal_Symbolized)
    set(Internal_Symbolized 0 CACHE INTERNAL "Build cathook with symbols so crashes can be sent")
endif()

if(NOT EnableVisuals)
    set(EnableGUI 0)
endif()

if (EnableVisuals)
    if (EnableImGuiDrawing)
        set(EnableEngineDrawing 0)
        find_package(Freetype REQUIRED)
        target_include_directories(cathook PRIVATE "${FREETYPE_INCLUDE_DIRS}")
        target_link_libraries(cathook ${FREETYPE_LIBRARIES})
    endif()
endif()

find_package(Git REQUIRED)
find_package(SDL2 REQUIRED)
find_package(Boost REQUIRED)

# cat packages

execute_process(COMMAND
        "${GIT_EXECUTABLE}" log -1 --pretty=\"%h\"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND
        "${GIT_EXECUTABLE}" log -1 --pretty=\"%ai\"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_COMMITTER_DATE
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

find_library(ValveLibrarySDL2    NAMES libSDL2-2.0.so.0 PATHS "${PROJECT_SOURCE_DIR}/lib" NO_DEFAULT_PATH)
find_library(ValveLibraryTier0   NAMES tier0 PATHS "${PROJECT_SOURCE_DIR}/lib" NO_DEFAULT_PATH)
find_library(ValveLibraryVStdLib NAMES vstdlib PATHS "${PROJECT_SOURCE_DIR}/lib" NO_DEFAULT_PATH)

if(EnableIPC)
    add_subdirectory(external/simple-ipc)
    target_link_libraries(cathook SimpleIPC)
endif()

if(EnableOnlineFeatures)
    add_subdirectory(external/co-library)
    target_link_libraries(cathook co-library)
endif()

if(EnableVisuals)
    target_include_directories(cathook PRIVATE include/visual)
    find_package(SDL2 REQUIRED)
    find_package(Freetype REQUIRED)
    if(ExternalDrawing)
        find_package(GLEW REQUIRED)
        add_subdirectory(external/libxoverlay)
        target_link_libraries(cathook xoverlay ${GLEW_LIBRARIES})
        target_include_directories(cathook PRIVATE external/libxoverlay/include)
    endif()
    target_include_directories(cathook PRIVATE "${SDL2_INCLUDE_DIRS}")
    target_link_libraries(cathook ${ValveLibrarySDL2} ${FREETYPE_LIBRARIES})
endif()

configure_file(include/config.h.in ${CMAKE_SOURCE_DIR}/include/config.h @ONLY)
configure_file(include/version.h.in ${CMAKE_SOURCE_DIR}/include/version.h @ONLY)

set(CMAKE_CXX_FLAGS "-m32 -march=native -fexceptions -fno-gnu-unique -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-rdynamic -ggdb -Og")
if (Internal_Symbolized)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ggdb -fvisibility=hidden -fvisibility-inlines-hidden")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -s -fvisibility=hidden -fvisibility-inlines-hidden")
endif()

target_compile_definitions(cathook PRIVATE
        _GLIBCXX_USE_CXX11_ABI=0
        _POSIX=1
        FREETYPE_GL_USE_VAO=1
        RAD_TELEMETRY_DISABLED=1
        LINUX=1
        USE_SDL=1
        _LINUX=1
        POSIX=1
        GNUC=1
        NO_MALLOC_OVERRIDE=1)

target_include_directories(cathook SYSTEM PRIVATE
        "external/source-sdk-2013-headers/mp/src/mathlib"
        "external/source-sdk-2013-headers/mp/src/public"
        "external/source-sdk-2013-headers/mp/src/common"
        "external/source-sdk-2013-headers/mp/src/public/tier0"
        "external/source-sdk-2013-headers/mp/src/public/tier1"
        "external/source-sdk-2013-headers/mp/src/tier1"
        "external/source-sdk-2013-headers/mp/src")

if(EnableWarnings)
    target_compile_options(cathook PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-class-memaccess -Wno-strict-aliasing -Wno-sign-compare)
else()
    target_compile_options(cathook PRIVATE -w)
endif()
if(Internal_Binarymode)
    target_compile_options(cathook PRIVATE -march=x86-64)
endif()


target_include_directories(cathook PRIVATE include include/hooks)

add_custom_target(data
        bash ./install-data "${DataPath}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        VERBATIM)

target_link_libraries(cathook ${ValveLibraryTier0} ${ValveLibraryVStdLib})
set_target_properties(cathook PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(src)
add_subdirectory(include)
add_subdirectory(external)
add_subdirectory(modules)

if (EnablePrecompiledHeaders AND NativePrecompiledHeaders)
    target_precompile_headers(cathook PRIVATE "${CMAKE_SOURCE_DIR}/include/common.hpp")
endif()

add_custom_command(TARGET cathook POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cathook> "${CMAKE_SOURCE_DIR}/bin/$<TARGET_FILE_NAME:cathook>")

# Strip
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message("Stripping info from library")
    add_custom_command(TARGET cathook POST_BUILD
    COMMAND strip --strip-all "${CMAKE_SOURCE_DIR}/bin/$<TARGET_FILE_NAME:cathook>")
endif()
