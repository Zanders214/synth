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
    src/dsp/Wavetable.cpp
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
