cmake_minimum_required(VERSION 3.22)
project(Uku C)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
set(KRYON_DIR "${REPO_ROOT}/vendor/kryon")
set(RAYLIB_DIR "${KRYON_DIR}/vendor/raylib/src")
set(SQLITE_DIR "${REPO_ROOT}/vendor-builds/sqlite")
set(NATIVE_APP_GLUE_DIR "${ANDROID_NDK}/sources/android/native_app_glue")
set(KRY_GENERATED_DIR "${CMAKE_BINARY_DIR}/kryon/generated")
set(KRYON_RAYLIB_GENERATED_DIR "${CMAKE_BINARY_DIR}/kryon/raylib_generated")
set(KRYON_COMPAT_GENERATED_HEADER "${KRYON_RAYLIB_GENERATED_DIR}/kryon_compat.generated.h")
set(KRYON_RAYLIB_BACKEND_RENAME_HEADER "${KRYON_RAYLIB_GENERATED_DIR}/raylib_backend_rename.h")
set(KRYON_RAYLIB_WRAPPERS_C "${KRYON_RAYLIB_GENERATED_DIR}/kryon_raylib_wrappers.c")
set(KRYON_NULL_BACKEND_C "${KRYON_RAYLIB_GENERATED_DIR}/kryon_null_backend.c")
include("${KRYON_DIR}/cmake/KryonKrySources.cmake")

set(OQS_BUILD_ONLY_LIB ON CACHE BOOL "" FORCE)
set(OQS_USE_OPENSSL OFF CACHE BOOL "" FORCE)
set(OQS_MINIMAL_BUILD "SIG_ml_dsa_44" CACHE STRING "" FORCE)
set(OQS_DIST_BUILD OFF CACHE BOOL "" FORCE)
set(OQS_OPT_TARGET generic CACHE STRING "" FORCE)
foreach(oqs_x86_feature
        ADX AES AVX AVX2 AVX512 AVX512BW AVX512DQ AVX512F
        BMI1 BMI2 FMA PCLMULQDQ POPCNT SSE SSE2 SSE3 VPCLMULQDQ)
    set(OQS_USE_${oqs_x86_feature}_INSTRUCTIONS OFF CACHE BOOL "" FORCE)
endforeach()
add_subdirectory("${KRYON_DIR}/vendor/liboqs" "${CMAKE_BINARY_DIR}/liboqs")
if(NOT TARGET oqs)
    message(FATAL_ERROR "Uku Android builds require liboqs for Ksync account crypto")
endif()

file(GLOB_RECURSE KRYON_SOURCES CONFIGURE_DEPENDS "${KRYON_DIR}/src/*.c")
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/backend/.*")
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/runtime_assets/.*")
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/notification/.*")
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/platform/desktop/.*")
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/platform/desktop_tray/.*")
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/platform/plan9/.*")
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/platform/web/.*")
list(REMOVE_ITEM KRYON_SOURCES
    "${KRYON_DIR}/src/scene/physics_world.c"
    "${KRYON_DIR}/src/scene/node_body2d.c"
    "${KRYON_DIR}/src/scene/node_area2d.c"
    "${KRYON_DIR}/src/scene/node_collision_shape2d.c")
list(APPEND KRYON_SOURCES
    "${KRYON_DIR}/src/backend/kry_event_wait.c"
    "${KRYON_DIR}/src/backend/kry_input.c"
    "${KRYON_DIR}/src/backend/kry_instance.c"
    "${KRYON_DIR}/src/backend/kry_screenshot.c"
    "${KRYON_DIR}/src/backend/kry_sw_png.c"
    "${KRYON_DIR}/src/backend/kry_surface_math.c")

file(GLOB_RECURSE UKU_KRY_SOURCES CONFIGURE_DEPENDS "${REPO_ROOT}/src/*.kry")
list(SORT UKU_KRY_SOURCES)
kryon_generate_kry_sources(UKU_KRY_GENERATED_SOURCES UKU_KRY_INCLUDE_DIR
    TARGET uku_kry_generated
    KRYON_DIR "${KRYON_DIR}"
    ROOT "${REPO_ROOT}"
    OUT_DIR "${KRY_GENERATED_DIR}"
    NO_MAIN
    SOURCES ${UKU_KRY_SOURCES}
)

add_custom_command(
    OUTPUT
        "${KRYON_COMPAT_GENERATED_HEADER}"
        "${KRYON_RAYLIB_BACKEND_RENAME_HEADER}"
        "${KRYON_RAYLIB_WRAPPERS_C}"
        "${KRYON_NULL_BACKEND_C}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${KRYON_RAYLIB_GENERATED_DIR}"
    COMMAND ${CMAKE_COMMAND} -E env KRYON_COMPAT_AUDIO=0
        sh "${KRYON_DIR}/tools/generate-kryon-compat.sh"
        "${RAYLIB_DIR}/raylib.h"
        "${KRYON_COMPAT_GENERATED_HEADER}"
        "${KRYON_RAYLIB_BACKEND_RENAME_HEADER}"
        "${KRYON_RAYLIB_WRAPPERS_C}"
        "${KRYON_NULL_BACKEND_C}"
    DEPENDS "${KRYON_DIR}/tools/generate-kryon-compat.sh" "${RAYLIB_DIR}/raylib.h"
    VERBATIM
)
add_custom_target(kryon_raylib_generated DEPENDS
    "${KRYON_COMPAT_GENERATED_HEADER}"
    "${KRYON_RAYLIB_BACKEND_RENAME_HEADER}"
    "${KRYON_RAYLIB_WRAPPERS_C}"
    "${KRYON_NULL_BACKEND_C}"
)

file(GLOB UKU_LOCALE_ASSETS RELATIVE "${REPO_ROOT}" CONFIGURE_DEPENDS "${REPO_ROOT}/locales/*.txt")
set(UKU_EMBEDDED_INPUTS
    vendor/kryon/fonts/noto/NotoSans-Regular.ttf
    ${UKU_LOCALE_ASSETS}
)
set(UKU_EMBEDDED_DEPS)
foreach(asset ${UKU_EMBEDDED_INPUTS})
    list(APPEND UKU_EMBEDDED_DEPS "${REPO_ROOT}/${asset}")
endforeach()
set(UKU_EMBEDDED_ASSETS_C "${CMAKE_BINARY_DIR}/uku_embedded_assets.c")
add_custom_command(
    OUTPUT "${UKU_EMBEDDED_ASSETS_C}"
    COMMAND sh "${KRYON_DIR}/scripts/embed-assets.sh" "${UKU_EMBEDDED_ASSETS_C}" ${UKU_EMBEDDED_INPUTS}
    DEPENDS "${KRYON_DIR}/scripts/embed-assets.sh" ${UKU_EMBEDDED_DEPS}
    WORKING_DIRECTORY "${REPO_ROOT}"
    VERBATIM
)

add_library(raylib STATIC
    "${RAYLIB_DIR}/rcore.c"
    "${RAYLIB_DIR}/rshapes.c"
    "${RAYLIB_DIR}/rtextures.c"
    "${RAYLIB_DIR}/rtext.c"
    "${RAYLIB_DIR}/raudio.c"
)
add_dependencies(raylib kryon_raylib_generated)
target_compile_definitions(raylib PRIVATE
    PLATFORM_ANDROID
    GRAPHICS_API_OPENGL_ES2
    SUPPORT_SCREEN_CAPTURE=0
    SUPPORT_COMPRESSION_API=0
    SUPPORT_AUTOMATION_EVENTS=0
    SUPPORT_CLIPBOARD_IMAGE=0
    SUPPORT_FILEFORMAT_BMP=0
    SUPPORT_FILEFORMAT_GIF=0
    SUPPORT_FILEFORMAT_QOI=0
    SUPPORT_FILEFORMAT_DDS=0
    SUPPORT_FILEFORMAT_TTF=1
    SUPPORT_FILEFORMAT_JPG=1
    SUPPORT_FILEFORMAT_PNG=1
    SUPPORT_MODULE_RAUDIO=0
)
target_compile_options(raylib PRIVATE
    -include "${KRYON_RAYLIB_BACKEND_RENAME_HEADER}"
    -Os
    -ffunction-sections
    -fdata-sections
)
target_include_directories(raylib PUBLIC
    "${RAYLIB_DIR}"
    "${NATIVE_APP_GLUE_DIR}"
)

add_library(main SHARED
    "${REPO_ROOT}/src/main.c"
    "${REPO_ROOT}/src/qrcodegen.c"
    "${SQLITE_DIR}/sqlite3.c"
    android_bridge.c
    ${UKU_KRY_GENERATED_SOURCES}
    "${UKU_EMBEDDED_ASSETS_C}"
    ${KRYON_SOURCES}
    "${KRYON_RAYLIB_WRAPPERS_C}"
    "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c"
)
add_dependencies(main uku_kry_generated kryon_raylib_generated)
target_include_directories(main PRIVATE
    "${KRYON_DIR}/include"
    "${KRYON_DIR}/src"
    "${REPO_ROOT}"
    "${REPO_ROOT}/src"
    "${UKU_KRY_INCLUDE_DIR}"
    "${SQLITE_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}"
    "${NATIVE_APP_GLUE_DIR}"
)
target_compile_definitions(main PRIVATE
    PLATFORM_ANDROID
    ANDROID_BUILD=1
    _DEFAULT_SOURCE
    _GNU_SOURCE
    _FILE_OFFSET_BITS=64
    UI_EMBEDDED_ONLY=1
    HAS_LIBOQS=1
    KRYON_HAS_LIBOQS=1
    KRYON_WITH_PHYSICS=0
    SUPPORT_MODULE_RAUDIO=0
)
target_compile_options(main PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fvisibility=hidden
)
target_link_options(main PRIVATE
    -Wl,--gc-sections
    -Wl,--strip-all
    -Wl,-z,max-page-size=16384
    -Wl,-z,common-page-size=16384
    -Wl,--wrap=fopen
)
target_link_libraries(main
    raylib
    oqs
    android
    log
    EGL
    GLESv2
    m
)
