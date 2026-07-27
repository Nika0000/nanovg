# nanovg playground -- WebAssembly build, Emscripten only.
#
#   emcmake cmake -B build-web -DNANOVG_BUILD_PLAYGROUND=ON -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-web
#
# Output lands in build-web/playground/site/ as a ready-to-serve static site.

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "NANOVG_BUILD_PLAYGROUND requires Emscripten (use emcmake).")
endif()

set(NANOVG_SITE_DIR ${CMAKE_CURRENT_BINARY_DIR}/site)

add_executable(nanovg_web
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nanovg_web.c
    ${CMAKE_SOURCE_DIR}/src/nanovg.c
)

target_include_directories(nanovg_web PRIVATE
    ${CMAKE_SOURCE_DIR}/include/nanovg
    ${CMAKE_SOURCE_DIR}/third_party
)

# nanovg.c and the GLES3 backend are upstream code; their warnings aren't ours
# to fix from here, but keep the playground shim itself clean.
target_compile_options(nanovg_web PRIVATE -Wall -Wno-unused-parameter)

set(NANOVG_WEB_LINK_FLAGS
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    -sEXPORT_NAME=createNanoVG
    -sMIN_WEBGL_VERSION=2
    -sMAX_WEBGL_VERSION=2
    -sFULL_ES3=1
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=33554432
    -sEXPORTED_FUNCTIONS=_malloc,_free
    -sEXPORTED_RUNTIME_METHODS=HEAPF32,HEAPU8,HEAP32,stringToUTF8,lengthBytesUTF8
    -sFILESYSTEM=0
    -sENVIRONMENT=web
)
target_link_options(nanovg_web PRIVATE ${NANOVG_WEB_LINK_FLAGS})

set_target_properties(nanovg_web PROPERTIES
    OUTPUT_NAME nanovg
    SUFFIX .js
    RUNTIME_OUTPUT_DIRECTORY ${NANOVG_SITE_DIR}
)

# Static assets: the hand-written site, plus fonts/images borrowed from the
# native examples so they aren't duplicated in git.
#
# Each file is its own custom command so editing HTML/CSS/JS re-stages without
# relinking the wasm -- a POST_BUILD hook would only fire when the binary itself
# was rebuilt, silently serving stale files.

# CONFIGURE_DEPENDS so adding a file to www/ re-globs on the next build instead
# of being silently left out of the site until someone reconfigures.
file(GLOB NANOVG_WWW_FILES CONFIGURE_DEPENDS
     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/www
     ${CMAKE_CURRENT_SOURCE_DIR}/www/*)

set(NANOVG_STAGED "")
foreach(rel ${NANOVG_WWW_FILES})
    add_custom_command(
        OUTPUT ${NANOVG_SITE_DIR}/${rel}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/www/${rel} ${NANOVG_SITE_DIR}/${rel}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/www/${rel}
        COMMENT "Staging ${rel}"
        VERBATIM
    )
    list(APPEND NANOVG_STAGED ${NANOVG_SITE_DIR}/${rel})
endforeach()

set(NANOVG_WEB_ASSETS
    examples/Roboto-Regular.ttf
    examples/Roboto-Bold.ttf
    examples/Roboto-Light.ttf
    examples/entypo.ttf
    examples/images/image1.jpg
)

foreach(asset ${NANOVG_WEB_ASSETS})
    get_filename_component(name ${asset} NAME)
    add_custom_command(
        OUTPUT ${NANOVG_SITE_DIR}/assets/${name}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/${asset} ${NANOVG_SITE_DIR}/assets/${name}
        DEPENDS ${CMAKE_SOURCE_DIR}/${asset}
        VERBATIM
    )
    list(APPEND NANOVG_STAGED ${NANOVG_SITE_DIR}/assets/${name})
endforeach()

add_custom_target(nanovg_playground ALL DEPENDS ${NANOVG_STAGED} nanovg_web)
