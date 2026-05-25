cmake_minimum_required(VERSION 3.15)
project(nanovg C CXX)

add_library(nanovg STATIC src/nanovg.c)
target_include_directories(nanovg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(NANOVG_FONTSTASH_STREAM)
    target_compile_definitions(nanovg PRIVATE FONTSTASH_STREAM_IMPLEMENTATION)
endif()

if(NANOVG_PLATFORM_PS4)
    target_sources(nanovg PRIVATE src/ps4/nanovg_ps4.c)
endif()

if(NANOVG_PLATFORM_DEKO3D)
    file(GLOB_RECURSE NANOVG_DEKO3D_SRC src/deko3d/*.cpp)
    target_sources(nanovg PRIVATE ${NANOVG_DEKO3D_SRC})
endif()
