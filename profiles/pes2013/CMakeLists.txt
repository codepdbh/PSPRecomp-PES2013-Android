# The PES generator always disables GTA address-specific code lowering.
add_executable(pes2013_recomp ../vcs/tools/vcs_codegen_main.cpp)
target_link_libraries(pes2013_recomp PRIVATE psprecomp_core)
target_compile_definitions(pes2013_recomp PRIVATE PSPRECOMP_GENERIC_CODEGEN=1)

# A clean checkout can build the tools before local game inputs are supplied.
file(GLOB PES2013_GENERATED CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generated/*.cpp")
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/generated_registry.cpp")
    message(STATUS "PES corpus missing: building tools only. Run profiles/pes2013/tools/prepare.ps1, then reconfigure.")
    return()
endif()

add_library(pes2013_aot STATIC ${PES2013_GENERATED})
target_include_directories(pes2013_aot PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/generated")
target_link_libraries(pes2013_aot PUBLIC psprecomp_core)
set_target_properties(pes2013_aot PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(pes2013_aot PRIVATE /bigobj /W0 ${PSPRECOMP_MSVC_MP_FLAG})
    if(PSPRECOMP_GENERATED_OPT_LEVEL STREQUAL "0")
        target_compile_options(pes2013_aot PRIVATE /Od /Ob0)
    else()
        target_compile_options(pes2013_aot PRIVATE /O2 /Ob0)
    endif()
else()
    target_compile_options(pes2013_aot PRIVATE "-O${PSPRECOMP_GENERATED_OPT_LEVEL}" -g0 -w)
endif()

add_executable(pes2013_probe host/probe_main.cpp)
target_link_libraries(pes2013_probe PRIVATE pes2013_aot)
if(ANDROID)
    target_link_libraries(pes2013_probe PRIVATE log)
endif()
target_compile_definitions(pes2013_probe PRIVATE
    PES2013_EXPECTED_SHA256="5ab84864354b3ee32cf4dbb1a7e0bb0fc914d63b0f25cddfa387af35e09a14db")
