#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Presets.h"

namespace aura
{
// Owns the factory + user preset lists, favourites, and applies presets to the parameter tree.
// All methods are meant to be called from the message thread.
class PresetManager : public juce::ChangeBroadcaster
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts);

    int numPresets() const { return (int) all.size(); }
    int numFactory() const { return numFactoryPresets; }
    const Preset& preset(int index) const { return all[(size_t) juce::jlimit(0, numPresets() - 1, index)]; }

    int currentIndex() const { return current; }
    juce::String currentName() const     { return currentNameStr; }
    juce::String currentCategory() const { return currentCategoryStr; }

    void load(int index);
    void stepWithin(const std::vector<int>& list, int delta);   // previous / next inside a (filtered) list

    // browser support. category: "" = all, "Favorites", "User", or a category name
    std::vector<int> filter(const juce::String& search, const juce::String& category) const;
    // front-page browser: genre = -1 (any) or 0..5 (index into genreNames()), group = -1 (any) or Group
    std::vector<int> filter(const juce::String& search, const juce::String& category, int genre, int group) const;
    bool isModified() const;                    // true when any knob differs from the loaded preset (Volume ignored)
    bool resetToPreset();                       // re-applies the selected preset (factory or user) = "back to original"
    int  indexOfName(const juce::String& name) const;
    bool isFavorite(int index) const;
    void toggleFavorite(int index);
    int  numFavorites() const { return favorites.size(); }

    // files
    juce::File userPresetDirectory() const;
    bool saveUserPreset(const juce::String& name, const juce::String& category);
    bool loadPresetFile(const juce::File& file);
    void rescanUserPresets();

    // state restore (host session load)
    void setCurrentFromState(const juce::String& name, const juce::String& category);

private:
    static juce::String keyOf(const Preset& p) { return p.category + "|" + p.name; }
    void applyValues(const std::vector<float>& values);
    void loadFavorites();
    void saveFavorites() const;
    juce::File favoritesFile() const;
    int indexOfKey(const juce::String& name, const juce::String& category) const;

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Preset> all;
    int numFactoryPresets = 0;
    int current = -1;
    juce::String currentNameStr = "Init", currentCategoryStr = "";
    juce::StringArray favorites;
    unsigned curGenres = kAllGenres;   // tags of the sound that is currently loaded (used when saving a user preset)
    int curGroup = GroupLead;
};
} // namespace aura
