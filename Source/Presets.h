#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <utility>

namespace aura
{
// Simple "front page" tags used by the genre / group buttons of the editor
enum Genre : unsigned { GenreHipHop = 1, GenreTrap = 2, GenreRnB = 4, GenreJazz = 8, GenreAmbient = 16, GenrePop = 32 };
constexpr int kNumGenres = 6;
constexpr unsigned kAllGenres = 63;
enum Group { GroupPluck = 0, GroupLead, GroupPad, GroupKeys, GroupBell, GroupTexture, GroupBass };
constexpr int kNumGroups = 7;
const juce::StringArray& genreNames();   // "HIP-HOP", "MELODIC TRAP", "R&B", "JAZZ", "AMBIENT", "POP"
const juce::StringArray& groupNames();   // "Plucks", "Leads", "Pads", "Keys", "Bells", "Textures", "Bass"
juce::String genreStarterPreset(int genreIndex);   // the sound that is loaded when a genre button is pressed

struct Preset
{
    juce::String name;
    juce::String category;
    unsigned genres = kAllGenres;                // bit mask of Genre
    int group = GroupLead;
    std::vector<std::pair<int, float>> values;  // parameter index -> real value (everything else = default)
    bool factory = true;
    bool testChord = false;                      // used by the calibration / QA tool
    int  testNote = 60;
};

constexpr int kNumCategories = 20;
const juce::StringArray& categoryNames();
const std::vector<Preset>& factoryPresets();
int categoryIndexOf(const juce::String& category);
const juce::StringArray& presetBuildErrors();   // unknown parameter ids etc. (empty when the library is clean)

// Full value table for a preset (defaults filled in) - used by the engine test and the manager.
std::vector<float> resolveValues(const Preset& p);
} // namespace aura
