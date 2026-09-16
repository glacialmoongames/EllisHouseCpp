cmake_minimum_required(VERSION 3.24)
project(EllisHouseCpp VERSION 0.1.0 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
if(EMSCRIPTEN)
    set(PLATFORM Web CACHE STRING "" FORCE)
endif()
FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(ellis_house
    src/main.cpp
    src/game.cpp
    src/manifest.cpp
    src/platform_keyboard.cpp
)
set_target_properties(ellis_house PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_include_directories(ellis_house PRIVATE src)
target_link_libraries(ellis_house PRIVATE raylib)

if(EMSCRIPTEN)
    set_target_properties(ellis_house PROPERTIES SUFFIX ".html")
    target_link_options(ellis_house PRIVATE
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sINITIAL_MEMORY=268435456"
        "-sFORCE_FILESYSTEM=1"
    )
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT ELLIS_IPO_SUPPORTED OUTPUT ELLIS_IPO_ERROR)
if(ELLIS_IPO_SUPPORTED)
    set_property(TARGET ellis_house PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

if(MSVC)
    target_compile_options(ellis_house PRIVATE /W4 /permissive-)
else()
    target_compile_options(ellis_house PRIVATE -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections)
    target_link_options(ellis_house PRIVATE -Wl,--gc-sections $<$<CONFIG:Release>:-Wl,-s>)
endif()

if(NOT EMSCRIPTEN)
    add_custom_command(TARGET ellis_house POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                "$<TARGET_FILE_DIR:ellis_house>/assets"
    )
endif()
