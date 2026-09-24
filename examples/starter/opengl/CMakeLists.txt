cmake_minimum_required(VERSION 3.15)
project(nanovg_opengl_starter LANGUAGES C)

include(FetchContent)

add_library(nanovg STATIC extern/nanovg/src/nanovg.c)
target_include_directories(nanovg PUBLIC
    extern/nanovg/include
    extern/nanovg/include/nanovg
    extern/nanovg/third_party
)

find_package(OpenGL REQUIRED)
find_package(glfw3 QUIET)
if(NOT TARGET glfw)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glfw)
endif()

if(NOT APPLE)
    find_package(GLEW QUIET)
    if(NOT TARGET GLEW::GLEW)
        set(glew-cmake_BUILD_SHARED OFF CACHE BOOL "" FORCE)
        # GLEW 2.2.0 predates CMake's removal of compatibility below 3.5.
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
        FetchContent_Declare(glew
            GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
            GIT_TAG glew-cmake-2.2.0
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(glew)
        if(TARGET libglew_static)
            add_library(GLEW::GLEW ALIAS libglew_static)
        endif()
    endif()
endif()

add_executable(nanovg_opengl src/main.c)
target_link_libraries(nanovg_opengl PRIVATE nanovg glfw OpenGL::GL)
if(NOT APPLE)
    target_compile_definitions(nanovg_opengl PRIVATE NANOVG_STARTER_GLEW)
    target_link_libraries(nanovg_opengl PRIVATE GLEW::GLEW)
endif()
if(UNIX)
    target_link_libraries(nanovg_opengl PRIVATE m ${CMAKE_DL_LIBS})
endif()

add_custom_command(TARGET nanovg_opengl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:nanovg_opengl>/assets"
    VERBATIM
)
