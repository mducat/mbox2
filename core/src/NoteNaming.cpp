#include "mbox/theory/NoteNaming.h"

#include <array>
#include <atomic>

namespace mbox::theory
{

namespace
{
    std::atomic<NoteNaming> currentNaming { NoteNaming::English };

    constexpr std::array<std::string_view, 7> kEnglish { "C", "D", "E", "F", "G", "A", "B" };
    // "R\xC3\xA9" = "Ré" in UTF-8 (escaped so the source encoding does not matter).
    constexpr std::array<std::string_view, 7> kSolfege { "Do", "R\xC3\xA9", "Mi", "Fa", "Sol", "La", "Si" };

    constexpr int wrap (int i) noexcept
    {
        const int r = i % 7;
        return r < 0 ? r + 7 : r;
    }

    const std::array<std::string_view, 7>& table (NoteNaming n) noexcept
    {
        return n == NoteNaming::Solfege ? kSolfege : kEnglish;
    }

    /** Letter index and length of the note name at the start of `s` in naming `n`, or {-1, 0}. */
    std::pair<int, std::size_t> matchAt (std::string_view s, NoteNaming n) noexcept
    {
        const auto& names = table (n);
        // Longest match first ("Sol" before "Si" is not ambiguous, but keep it general).
        int best = -1;
        std::size_t bestLen = 0;
        for (int i = 0; i < 7; ++i)
        {
            const auto name = names[static_cast<std::size_t> (i)];
            if (name.size() > bestLen && s.substr (0, name.size()) == name)
            {
                best = i;
                bestLen = name.size();
            }
        }
        if (best < 0 && n == NoteNaming::Solfege && s.substr (0, 2) == "Re") // unaccented spelling
            return { 1, 2 };
        return { best, bestLen };
    }
} // namespace

void setNoteNaming (NoteNaming naming) noexcept { currentNaming.store (naming, std::memory_order_relaxed); }
NoteNaming noteNaming() noexcept                { return currentNaming.load (std::memory_order_relaxed); }

std::string letterName (int letterIndex, NoteNaming naming)
{
    return std::string (table (naming)[static_cast<std::size_t> (wrap (letterIndex))]);
}

std::string letterName (int letterIndex)
{
    return letterName (letterIndex, noteNaming());
}

int letterIndexOf (std::string_view name) noexcept
{
    const auto solfege = matchAt (name, NoteNaming::Solfege);
    if (solfege.first >= 0)
        return solfege.first;
    return matchAt (name, NoteNaming::English).first;
}

std::string convertNoteNames (std::string_view text, NoteNaming from, NoteNaming to)
{
    if (from == to)
        return std::string (text);

    std::string out;
    out.reserve (text.size() + 8);

    std::size_t i = 0;
    bool atNameStart = true;
    while (i < text.size())
    {
        if (atNameStart)
        {
            atNameStart = false;
            if (const auto [letter, len] = matchAt (text.substr (i), from); letter >= 0)
            {
                out += letterName (letter, to);
                i += len;
                continue;
            }
        }
        const char c = text[i++];
        out += c;
        atNameStart = (c == '/');
    }
    return out;
}

} // namespace mbox::theory
