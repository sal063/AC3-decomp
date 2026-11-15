cmake_minimum_required(VERSION 3.25)
project(ac3engine VERSION 0.0.1 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "ac3engine builds for Windows x64 only; Linux runs the Windows build under Proton.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

option(AC3_WERROR "Treat warnings in engine code as errors" ON)

set(SDL3_ROOT "C:/SDL3" CACHE PATH "SDL3 development package (include/, lib/x64/)")
if(NOT EXISTS "${SDL3_ROOT}/include/SDL3/SDL.h")
    message(FATAL_ERROR "SDL3 not found at ${SDL3_ROOT}; pass -DSDL3_ROOT=<path>.")
endif()
add_library(SDL3::SDL3 SHARED IMPORTED)
set_target_properties(SDL3::SDL3 PROPERTIES
    IMPORTED_IMPLIB "${SDL3_ROOT}/lib/x64/SDL3.lib"
    IMPORTED_LOCATION "${SDL3_ROOT}/lib/x64/SDL3.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL3_ROOT}/include")

find_package(Vulkan 1.3 REQUIRED)

add_library(ac3_warnings INTERFACE)
target_compile_options(ac3_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wno-missing-field-initializers
    $<$<BOOL:${AC3_WERROR}>:-Werror>)

function(ac3_copy_runtime target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SDL3_ROOT}/lib/x64/SDL3.dll" $<TARGET_FILE_DIR:${target}>)
endfunction()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(AC3_SHADERS terrain.vert terrain.frag hd_prim.vert hd_prim.frag hd_present.vert hd_present.frag
    hd_down.vert hd_down.frag hd_present24.frag ocean_spectrum.comp ocean_fft.comp ocean_finalize.comp)
set(AC3_GENERATED ${CMAKE_BINARY_DIR}/generated)
set(AC3_SPV_ARGS)
set(AC3_SPV_FILES)
foreach(sh ${AC3_SHADERS})
    string(REPLACE "." "_" sym ${sh})
    set(spv ${AC3_GENERATED}/${sh}.spv)
    add_custom_command(OUTPUT ${spv}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${AC3_GENERATED}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} -O --target-env=vulkan1.3 ${CMAKE_SOURCE_DIR}/shaders/${sh} -o ${spv}
        DEPENDS ${CMAKE_SOURCE_DIR}/shaders/${sh}
        COMMENT "glslc ${sh}")
    list(APPEND AC3_SPV_FILES ${spv})
    list(APPEND AC3_SPV_ARGS spv_${sym}=${spv})
endforeach()
add_custom_command(OUTPUT ${AC3_GENERATED}/shaders.hpp
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/embed_spv.py ${AC3_GENERATED}/shaders.hpp ${AC3_SPV_ARGS}
    DEPENDS ${AC3_SPV_FILES} ${CMAKE_SOURCE_DIR}/tools/embed_spv.py
    COMMENT "embedding shaders")
add_custom_target(ac3_shaders DEPENDS ${AC3_GENERATED}/shaders.hpp)

add_subdirectory(engine)
add_subdirectory(app)
add_subdirectory(viewer)

enable_testing()
add_subdirectory(tests)

add_executable(ac3cat tools/ac3cat/main.cpp)
target_link_libraries(ac3cat PRIVATE ac3_warnings ac3_gamedata ac3_vfs ac3_assets ac3_core)

add_executable(simdiff tools/simdiff/main.cpp)
target_link_libraries(simdiff PRIVATE ac3_warnings ac3_sim ac3_machine)
