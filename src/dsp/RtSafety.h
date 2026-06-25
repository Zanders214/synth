#pragma once

// ZW_RT_NONBLOCKING marks a function as a real-time (audio-thread) context for
// clang's RealtimeSanitizer. The `rt_check` CMake target (ZW_RT_SANITIZE=ON)
// defines this on the command line to [[clang::nonblocking]] so RTSan aborts on
// any allocation, lock or syscall reached from the marked function; every other
// build (MSVC/SonarCloud/plugin/bench) compiles it away to nothing.
//
// We annotate the DSP hot paths we own (ZWVoice::renderNextBlock, FxChain::process)
// rather than the whole processBlock: juce::Synthesiser::renderNextBlock takes an
// (uncontended) framework lock around the voice loop that we can't remove without
// replacing the voice engine, and it sits *outside* these inner contexts, so this
// scope checks our code without flagging that lock.
#ifndef ZW_RT_NONBLOCKING
#define ZW_RT_NONBLOCKING
#endif
