cmake_minimum_required(VERSION 3.15)
project(nanovg C CXX)

option(NANOVG_BUILD_EXAMPLES "Build example programs" OFF)
option(NANOVG_PLATFORM_PS4 "Enable PS4 backend" OFF)
option(NANOVG_PLATFORM_DEKO3D "Enable Nintendo Switch/deko3d backend" OFF)
option(NANOVG_PLATFORM_METAL "Enable Metal backend" OFF)
option(NANOVG_FONTSTASH_STREAM "Enable font streaming" OFF)

add_library(nanovg STATIC src/nanovg.c)
target_include_directories(nanovg PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nanovg
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_include_directories(nanovg PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/d3d11
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/metal
)

if(NANOVG_FONTSTASH_STREAM)
    target_compile_definitions(nanovg PRIVATE FONTSTASH_STREAM_IMPLEMENTATION)
endif()

if(NANOVG_PLATFORM_PS4)
    target_sources(nanovg PRIVATE src/ps4/nanovg_ps4.c)
endif()

if(NANOVG_PLATFORM_DEKO3D)
    file(GLOB_RECURSE NANOVG_DEKO3D_SRC src/deko3d/*.cpp)
    target_sources(nanovg PRIVATE ${NANOVG_DEKO3D_SRC})
    target_include_directories(nanovg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/deko3d)
endif()

if(NANOVG_PLATFORM_METAL)
    enable_language(OBJC)
    target_sources(nanovg PRIVATE src/metal/nanovg_mtl.m)
endif()

if(NANOVG_BUILD_EXAMPLES)
    set(NANOVG_EXAMPLE_COMMON
        examples/demo.c
        examples/perf.c
    )

    if(WIN32)
        add_executable(example_d3d11 examples/example_d3d11.c ${NANOVG_EXAMPLE_COMMON})
        target_include_directories(example_d3d11 PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include/nanovg
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party
            ${CMAKE_CURRENT_SOURCE_DIR}/examples
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/d3d11
        )
        target_compile_definitions(example_d3d11 PRIVATE NANOVG_D3D11)
        target_link_libraries(example_d3d11 nanovg d3d11 dxguid)
    endif()

    find_package(glfw3 QUIET)
    if(glfw3_FOUND)
        find_package(OpenGL REQUIRED)

        add_executable(example_gl3 examples/example_gl3.c ${NANOVG_EXAMPLE_COMMON})
        target_include_directories(example_gl3 PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include/nanovg
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party
            ${CMAKE_CURRENT_SOURCE_DIR}/examples
        )
        target_link_libraries(example_gl3 nanovg glfw OpenGL::GL ${CMAKE_DL_LIBS})
        if(UNIX)
            target_link_libraries(example_gl3 m)
        endif()

        if(NOT APPLE)
            find_package(GLEW REQUIRED)
            target_link_libraries(example_gl3 GLEW::GLEW)
            target_compile_definitions(example_gl3 PRIVATE NANOVG_GLEW)
        endif()
    endif()
endif()
