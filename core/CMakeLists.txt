add_library(mbox_theory STATIC
    src/PitchClass.cpp
    src/Scale.cpp
    src/Chord.cpp
    src/Tuning.cpp
    src/analysis/Harmonics.cpp
    src/Spelling.cpp
    src/Voicing.cpp
    src/instrument/Keyboard.cpp
    src/instrument/Fretboard.cpp
    src/instrument/GuitarVoicing.cpp
    src/TrichordSpace.cpp
)
add_library(mbox::theory ALIAS mbox_theory)

target_include_directories(mbox_theory PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(mbox_theory PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(mbox_theory PRIVATE /W4 /permissive-)
else()
    target_compile_options(mbox_theory PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Needed so the static lib can be linked into the plugin's shared module.
set_target_properties(mbox_theory PROPERTIES POSITION_INDEPENDENT_CODE ON)
