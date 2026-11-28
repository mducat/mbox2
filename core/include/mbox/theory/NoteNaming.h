#pragma once

#include <string>
#include <string_view>

namespace mbox::theory
{

/**
    How note letters are written in every name the core produces
    (PitchClass::name, SpelledPitchClass::name, chord symbols, Tuning::noteName...).

      English:  C D E F G A B        ("F#m7", "Bb", "C7(b9)/E")
      Solfege:  Do Re Mi Fa Sol La Si ("Fa#m7", "Sib", "Do7(b9)/Mi"), with the
                accent on Re (UTF-8 "Ré").

    Accidentals, qualities and octave numbers are unchanged (C4 = Do4 = MIDI 60).
    Parsing (PitchClass::fromName) always takes English names.

    This is a process-wide display setting (English by default); it is only
    read when a name is built, never on the audio thread.
*/
enum class NoteNaming : int { English = 0, Solfege = 1 };

void setNoteNaming (NoteNaming naming) noexcept;
NoteNaming noteNaming() noexcept;

/** Name of a natural letter, 0 = C .. 6 = B (any int, taken mod 7), in the given / current naming. */
std::string letterName (int letterIndex, NoteNaming naming);
std::string letterName (int letterIndex);

/** Letter index (0 = C .. 6 = B) of the natural note name in `name` (any naming), or -1. */
int letterIndexOf (std::string_view name) noexcept;

/**
    Rewrites the note names of a label built in `from` naming into `to` naming:
    the name at the start and every name after a '/' (slash bass), so chord
    symbols such as "Bbm7/Ab" or "Do7(b9)/Mi" convert both ways. Text that does
    not start with a note name in those places is left as is.
*/
std::string convertNoteNames (std::string_view text, NoteNaming from, NoteNaming to);

/** RAII helper (mainly for tests): switches the naming for a scope. */
class ScopedNoteNaming
{
public:
    explicit ScopedNoteNaming (NoteNaming naming) noexcept : previous (noteNaming()) { setNoteNaming (naming); }
    ~ScopedNoteNaming() { setNoteNaming (previous); }

    ScopedNoteNaming (const ScopedNoteNaming&) = delete;
    ScopedNoteNaming& operator= (const ScopedNoteNaming&) = delete;

private:
    NoteNaming previous;
};

} // namespace mbox::theory
