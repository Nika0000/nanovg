cmake_minimum_required(VERSION 3.15)
project(nanovg_d3d11_starter LANGUAGES C)

if(NOT WIN32)
    message(FATAL_ERROR "The Direct3D 11 starter requires Windows and the Windows SDK.")
endif()

add_library(nanovg STATIC extern/nanovg/src/nanovg.c)
target_include_directories(nanovg PUBLIC extern/nanovg/include/nanovg extern/nanovg/third_party)

add_executable(nanovg_d3d11 src/main.c)
target_include_directories(nanovg_d3d11 PRIVATE src extern/nanovg/shaders/d3d11)
target_compile_definitions(nanovg_d3d11 PRIVATE NANOVG_D3D11)
target_link_libraries(nanovg_d3d11 PRIVATE nanovg d3d11 dxguid)
add_custom_command(TARGET nanovg_d3d11 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:nanovg_d3d11>/assets")
