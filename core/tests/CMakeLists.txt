include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.9.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(mbox_theory_tests
    PitchClassTests.cpp
    ScaleTests.cpp
    ChordTests.cpp
    TuningTests.cpp
    MicrotonalTests.cpp
    HarmonicsTests.cpp
    WaveformTests.cpp
    HarmonyTests.cpp
    SpellingTests.cpp
    VoicingTests.cpp
    InstrumentTests.cpp
    TrichordSpaceTests.cpp
    SoundTests.cpp
)
target_link_libraries(mbox_theory_tests PRIVATE mbox::theory Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(mbox_theory_tests)
