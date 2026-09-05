#include "core/gamemodes.hpp"

#include "core/gamemodeClassical.hpp"

// gameplaymodes -------------------------------------------------------------
// The selectable modes, in menu order. A mode's index (its position in this
// array) is the stable selector kept by game::mModeIndex and shown in the
// title menu's "GAME MODE" row, so keep this array ordered by how the modes
// should appear (index 0 is the default = Classical).
namespace gameplaymodes {
namespace {

struct Entry
{
    const char* name;
    std::unique_ptr<gameplay_mode> (*make)();
};

Entry kTable[] = {
    // The original Trigonometry Wars match rules; the default mode.
    { "Classical", []() -> std::unique_ptr<gameplay_mode> { return std::make_unique<classical_mode>(); } },
    // Future modes (e.g. the planned fast-pace "Endless" mode with new
    // weapon power-ups) are appended here, one row each.
};

} // namespace

int count()
{
    return static_cast<int>(sizeof(kTable) / sizeof(kTable[0]));
}

const char* name(int index)
{
    if (index < 0 || index >= count())
        return "?";
    return kTable[index].name;
}

std::unique_ptr<gameplay_mode> create(int index)
{
    if (index < 0 || index >= count())
        return nullptr;
    return kTable[index].make();
}

} // namespace gameplaymodes
