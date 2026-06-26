cmake_minimum_required(VERSION 3.22)

project(ZandersWave VERSION 0.1.0 LANGUAGES C CXX)

# ---- Global build settings -------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # compile_commands.json for tooling / Sonar fallback

# Ninja is single-config: default to Release when no config was chosen.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---- JUCE via FetchContent (pinned to an exact tag) ------------------------
# JUCE bundles the VST3 SDK, so no separate Steinberg SDK is required.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.9          # latest stable 8.0.x verified at scaffold time
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ---- Per-user plugin install dirs (no admin; avoids Program Files) ----------
# These INHERITED properties override JUCE's default system copy locations so
# COPY_PLUGIN_AFTER_BUILD installs into the current user's plugin folders.
if(WIN32)
    set_property(DIRECTORY PROPERTY JUCE_VST3_COPY_DIR "$ENV{LOCALAPPDATA}/Programs/Common/VST3")
endif()

# ---- The plugin target -----------------------------------------------------
juce_add_plugin(ZandersWave
    COMPANY_NAME                "Zanders"
    BUNDLE_ID                   com.zanders.zanderswave
    PLUGIN_MANUFACTURER_CODE    Znds          # 4 chars, exactly one uppercase
    PLUGIN_CODE                 Zwv1          # 4 chars, exactly one uppercase, unique
    FORMATS                     VST3 AU Standalone   # AU is silently skipped off macOS
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES             "Instrument" "Synth"
    AU_MAIN_TYPE                "kAudioUnitType_MusicDevice"
    COPY_PLUGIN_AFTER_BUILD     TRUE          # auto-install VST3/AU to the user plugin dir
    PRODUCT_NAME                "ZandersWave")

juce_generate_juce_header(ZandersWave)

# ---- Bundled assets (fonts) -> BinaryData ----------------------------------
juce_add_binary_data(ZandersWaveData
    SOURCES
        assets/fonts/SpaceGroteskVariable.ttf
        assets/fonts/JetBrainsMonoVariable.ttf)
set_target_properties(ZandersWaveData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# ---- Sources (globbed so new milestone files are picked up on reconfigure) --
file(GLOB_RECURSE ZW_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
target_sources(ZandersWave PRIVATE ${ZW_SOURCES})
target_include_directories(ZandersWave PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ---- JUCE feature flags ----------------------------------------------------
target_compile_definitions(ZandersWave PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)
    # (JUCE 8.0.9 removed the splash screen, so JUCE_DISPLAY_SPLASH_SCREEN is gone.)

# ---- Link JUCE modules -----------------------------------------------------
# juce_audio_utils transitively pulls audio_processors/basics/devices + gui modules
# (audio_devices + the keyboard component are needed by the Standalone target).
target_link_libraries(ZandersWave
    PRIVATE
        ZandersWaveData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# NOTE: juce_add_binary_data(...) for bundled fonts + wavetables is added in M6/M7
# once the asset files exist under assets/.

# ---- Headless render smoke test (M2 verification) --------------------------
# A console app that compiles the DSP sources directly and drives the voice
# engine; links the JUCE modules itself (not the plugin shared-code target) to
# avoid double-compiling JUCE modules.
juce_add_console_app(render_smoke PRODUCT_NAME "render_smoke")
juce_generate_juce_header(render_smoke)
target_sources(render_smoke PRIVATE
    tests/render_smoke.cpp
    src/Parameters.cpp
    src/PresetManager.cpp
    src/dsp/Wavetable.cpp
    src/dsp/WavetableLibrary.cpp
    src/dsp/ParamRefs.cpp
    src/dsp/ModMatrix.cpp
    src/dsp/Arpeggiator.cpp
    src/dsp/Voice.cpp)
target_include_directories(render_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(render_smoke PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(render_smoke PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# ---- Headless UI snapshot (dev tool; excluded from default/CI builds) -------
# Renders the editor to a PNG so UI layout can be inspected without a window.
# Build on demand: cmake --build build --target ui_snapshot
juce_add_console_app(ui_snapshot PRODUCT_NAME "ui_snapshot")
juce_generate_juce_header(ui_snapshot)
set_target_properties(ui_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(ui_snapshot PRIVATE tests/ui_snapshot.cpp ${ZW_SOURCES})
target_include_directories(ui_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(ui_snapshot PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JucePlugin_Name="ZandersWave")
target_link_libraries(ui_snapshot PRIVATE
    ZandersWaveData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# ---- DSP unit tests (juce::UnitTest) ---------------------------------------
# Console app that registers every tests/unit/*Tests.cpp and runs them. Compiles
# the DSP/logic sources directly (same pattern as render_smoke) so the engine is
# tested without the plugin/UI wrapper. Built with debug info (/Zi + /DEBUG) so
# OpenCppCoverage can map line coverage back to source in CI.
file(GLOB ZW_UNIT_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/*.cpp)
juce_add_console_app(unit_tests PRODUCT_NAME "unit_tests")
juce_generate_juce_header(unit_tests)
target_sources(unit_tests PRIVATE
    ${ZW_UNIT_TESTS}
    src/Parameters.cpp
    src/PresetManager.cpp
    src/dsp/Wavetable.cpp
    src/dsp/WavetableLibrary.cpp
    src/dsp/ParamRefs.cpp
    src/dsp/ModMatrix.cpp
    src/dsp/Arpeggiator.cpp
    src/dsp/Voice.cpp
    src/dsp/fx/FxChain.cpp)
target_include_directories(unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(unit_tests PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
if(MSVC)
    # Disable optimisation/inlining (/Od /Ob0) and emit debug info (/Zi) so
    # OpenCppCoverage maps line coverage accurately. With Release inlining the
    # header-only DSP otherwise reports near-zero coverage even when fully tested.
    target_compile_options(unit_tests PRIVATE /Od /Ob0 /Zi)
    target_link_options   (unit_tests PRIVATE /DEBUG)
endif()
target_link_libraries(unit_tests PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# ---- Performance monitoring: RTSan + benchmark harnesses -------------------
# Two opt-in dev/CI targets (see .github/workflows/quality.yml and perf.yml).
# Both are EXCLUDE_FROM_ALL so the default and SonarCloud builds stay lean.
option(ZW_RT_SANITIZE "Build rt_check with clang's RealtimeSanitizer (needs clang>=20)" OFF)
option(ZW_BUILD_BENCH "Build the perf_bench micro-benchmark (fetches nanobench)"        OFF)

# Real-time-safety check: drives the real plugin's processBlock under RTSan so
# any malloc/lock/syscall on the audio thread aborts the run.
if(ZW_RT_SANITIZE)
    juce_add_console_app(rt_check PRODUCT_NAME "rt_check")
    juce_generate_juce_header(rt_check)
    set_target_properties(rt_check PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_sources(rt_check PRIVATE tests/rt_check.cpp ${ZW_SOURCES})
    target_include_directories(rt_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(rt_check PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JucePlugin_Name="ZandersWave"
        "ZW_RT_NONBLOCKING=[[clang::nonblocking]]")
    target_compile_options(rt_check PRIVATE -fsanitize=realtime)
    target_link_options   (rt_check PRIVATE -fsanitize=realtime)
    target_link_libraries(rt_check PRIVATE
        ZandersWaveData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()

# Performance micro-benchmark: times the DSP graph (voices / FX / full) and emits
# github-action-benchmark JSON for trend tracking + PR regression alerts.
if(ZW_BUILD_BENCH)
    FetchContent_Declare(nanobench
        GIT_REPOSITORY https://github.com/martinus/nanobench.git
        GIT_TAG        v4.3.11
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(nanobench)

    juce_add_console_app(perf_bench PRODUCT_NAME "perf_bench")
    juce_generate_juce_header(perf_bench)
    set_target_properties(perf_bench PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_sources(perf_bench PRIVATE
        tests/perf_bench.cpp
        src/Parameters.cpp
        src/PresetManager.cpp
        src/dsp/Wavetable.cpp
        src/dsp/WavetableLibrary.cpp
        src/dsp/ParamRefs.cpp
        src/dsp/ModMatrix.cpp
        src/dsp/Voice.cpp
        src/dsp/fx/FxChain.cpp)   # render_smoke omits FxChain; perf_bench needs it
    target_include_directories(perf_bench PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${nanobench_SOURCE_DIR}/src/include)   # header-only use (we define ANKERL_NANOBENCH_IMPLEMENT)
    target_compile_definitions(perf_bench PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(perf_bench PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
