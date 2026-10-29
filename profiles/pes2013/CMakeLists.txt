# The PES generator always disables GTA address-specific code lowering.
add_executable(pes2013_recomp ../vcs/tools/vcs_codegen_main.cpp)
target_link_libraries(pes2013_recomp PRIVATE psprecomp_core)
target_compile_definitions(pes2013_recomp PRIVATE PSPRECOMP_GENERIC_CODEGEN=1)

add_library(pes2013_kernel_library STATIC ../vcs/host/psp_kernel_library.cpp ../vcs/host/psp_power_services.cpp ../vcs/host/psp_umd_services.cpp ../vcs/host/psp_impose_services.cpp)
target_include_directories(pes2013_kernel_library PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../vcs/host")
target_link_libraries(pes2013_kernel_library PUBLIC psprecomp_core)
set_target_properties(pes2013_kernel_library PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(PSPRECOMP_BUILD_TESTS AND PSPRECOMP_BUILD_PROFILE_TESTS)
    add_executable(pes2013_kernel_tests tests/kernel_library_tests.cpp)
    target_link_libraries(pes2013_kernel_tests PRIVATE pes2013_kernel_library)
    add_test(NAME pes2013_kernel_tests COMMAND pes2013_kernel_tests)
    add_executable(pes2013_umd_tests tests/umd_services_tests.cpp)
    target_link_libraries(pes2013_umd_tests PRIVATE pes2013_kernel_library)
    add_test(NAME pes2013_umd_tests COMMAND pes2013_umd_tests)
endif()

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

option(PSPRECOMP_PES2013_WINDOWS_HOST_PROBE "Build the headless shared-service probe on Windows" OFF)
if(ANDROID OR (WIN32 AND PSPRECOMP_PES2013_WINDOWS_HOST_PROBE))
    set(SHARED_HOST "${CMAKE_CURRENT_SOURCE_DIR}/../vcs/host")
    set(AT3 "${CMAKE_CURRENT_SOURCE_DIR}/../vcs/third_party/at3_standalone")
    if(ANDROID)
        add_library(pes2013_atrac STATIC
            "${AT3}/atrac.cpp" "${AT3}/atrac3.cpp" "${AT3}/atrac3plus.cpp"
            "${AT3}/atrac3plusdec.cpp" "${AT3}/atrac3plusdsp.cpp" "${AT3}/get_bits.cpp"
            "${AT3}/compat.cpp" "${AT3}/fft.cpp" "${AT3}/mem.cpp")
        target_include_directories(pes2013_atrac PRIVATE "${AT3}")
        target_compile_options(pes2013_atrac PRIVATE -O2 -w)
        target_link_libraries(pes2013_atrac PRIVATE log)
        set_target_properties(pes2013_atrac PROPERTIES POSITION_INDEPENDENT_CODE ON)
        set(PES_PLATFORM_SOURCES
            "${SHARED_HOST}/ge_gpu_backend_vulkan.cpp" "${SHARED_HOST}/display_window_android.cpp"
            "${SHARED_HOST}/audio_output_android.cpp" "${SHARED_HOST}/vcs_media_decoder_android.cpp")
        set(PES_PLATFORM_LIBRARIES pes2013_atrac android log aaudio vulkan ${CMAKE_DL_LIBS})
    else()
        set(PSPRECOMP_PES2013_FFMPEG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../vcs/third_party/ffmpeg"
            CACHE PATH "Local FFmpeg 7 SDK root with lib/ and bin/ for Windows host diagnostics")
        set(PES_PLATFORM_SOURCES
            "${SHARED_HOST}/ge_gpu_backend_dx12.cpp" "${SHARED_HOST}/display_window.cpp"
            "${SHARED_HOST}/audio_output.cpp" "${SHARED_HOST}/vcs_media_decoder.cpp")
        set(PES_PLATFORM_LIBRARIES winmm)
        foreach(component avcodec avformat avutil swresample swscale)
            if(NOT EXISTS "${PSPRECOMP_PES2013_FFMPEG_ROOT}/lib/${component}.lib")
                message(FATAL_ERROR "Provide PSPRECOMP_PES2013_FFMPEG_ROOT with FFmpeg 7 import libraries")
            endif()
            list(APPEND PES_PLATFORM_LIBRARIES "${PSPRECOMP_PES2013_FFMPEG_ROOT}/lib/${component}.lib")
        endforeach()
    endif()
    add_library(pes2013_shared_host STATIC
        "${SHARED_HOST}/vcs_profile.cpp" "${SHARED_HOST}/vcs_native_fast_paths.cpp"
        "${SHARED_HOST}/framebuffer_capture.cpp" "${SHARED_HOST}/vcs_config.cpp"
        "${SHARED_HOST}/vcs_bootstrap_paths.cpp" "${SHARED_HOST}/vcs_camera_input.cpp"
        "${SHARED_HOST}/vcs_vehicle_input.cpp" "${SHARED_HOST}/vcs_fps_overlay.cpp"
        "${SHARED_HOST}/vcs_project2dfx.cpp" "${SHARED_HOST}/vcs_project2dfx_lights.cpp"
        "${SHARED_HOST}/vcs_draw_distance_patch.cpp" "${SHARED_HOST}/vcs_runtime_log.cpp"
        "${SHARED_HOST}/vcs_hdr_post_dx12_stub.cpp" "${SHARED_HOST}/ge_renderer.cpp"
        ${PES_PLATFORM_SOURCES})
    target_include_directories(pes2013_shared_host PUBLIC "${SHARED_HOST}"
        PRIVATE "${AT3}" "${CMAKE_CURRENT_SOURCE_DIR}/../vcs/third_party/ffmpeg/include")
    target_compile_definitions(pes2013_shared_host PUBLIC PSPRECOMP_TITLE_PES2013=1 PSPRECOMP_HEADLESS=1)
    target_link_libraries(pes2013_shared_host PUBLIC psprecomp_core pes2013_kernel_library
        PRIVATE ${PES_PLATFORM_LIBRARIES})
    set_target_properties(pes2013_shared_host PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_executable(pes2013_host_probe host/probe_main.cpp)
    target_link_libraries(pes2013_host_probe PRIVATE pes2013_aot pes2013_shared_host)
    target_compile_definitions(pes2013_host_probe PRIVATE PSPRECOMP_PES2013_SHARED_HOST=1
        PES2013_EXPECTED_SHA256="5ab84864354b3ee32cf4dbb1a7e0bb0fc914d63b0f25cddfa387af35e09a14db")
    if(WIN32)
        file(GLOB PES_FFMPEG_DLLS "${PSPRECOMP_PES2013_FFMPEG_ROOT}/bin/*.dll")
        foreach(dll IN LISTS PES_FFMPEG_DLLS)
            add_custom_command(TARGET pes2013_host_probe POST_BUILD COMMAND ${CMAKE_COMMAND} -E
                copy_if_different "${dll}" "$<TARGET_FILE_DIR:pes2013_host_probe>")
        endforeach()
    endif()
endif()
