#include "PresetManager.h"
#include "Params.h"

namespace aura
{
PresetManager::PresetManager(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    all = factoryPresets();
    numFactoryPresets = (int) all.size();
    loadFavorites();
    rescanUserPresets();
}

juce::File PresetManager::userPresetDirectory() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("AuraSynth").getChildFile("Presets");
    dir.createDirectory();
    return dir;
}

juce::File PresetManager::favoritesFile() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("AuraSynth");
    dir.createDirectory();
    return dir.getChildFile("favorites.txt");
}

void PresetManager::loadFavorites()
{
    favorites.clear();
    const auto f = favoritesFile();
    if (f.existsAsFile()) { favorites.addLines(f.loadFileAsString()); favorites.removeEmptyStrings(); }
}

void PresetManager::saveFavorites() const { favoritesFile().replaceWithText(favorites.joinIntoString("\n")); }

bool PresetManager::isFavorite(int index) const
{
    return index >= 0 && index < numPresets() && favorites.contains(keyOf(all[(size_t) index]));
}

void PresetManager::toggleFavorite(int index)
{
    if (index < 0 || index >= numPresets()) return;
    const auto key = keyOf(all[(size_t) index]);
    if (favorites.contains(key)) favorites.removeString(key); else favorites.add(key);
    saveFavorites();
    sendChangeMessage();
}

int PresetManager::indexOfKey(const juce::String& name, const juce::String& category) const
{
    for (int i = 0; i < numPresets(); ++i)
        if (all[(size_t) i].name == name && (category.isEmpty() || all[(size_t) i].category == category)) return i;
    return -1;
}

void PresetManager::applyValues(const std::vector<float>& values)
{
    for (int i = 0; i < kNumParams; ++i)
    {
        if (i == volume) continue;      // the user's Volume knob belongs to the session, not to the sound
        if (auto* p = apvts.getParameter(paramDef(i).id))
            p->setValueNotifyingHost(p->convertTo0to1(values[(size_t) i]));
    }
}

void PresetManager::load(int index)
{
    if (index < 0 || index >= numPresets()) return;
    const auto& p = all[(size_t) index];
    applyValues(resolveValues(p));
    current = index;
    currentNameStr = p.name;
    currentCategoryStr = p.category;
    curGenres = p.genres; curGroup = p.group;
    sendChangeMessage();
}

void PresetManager::stepWithin(const std::vector<int>& list, int delta)
{
    if (list.empty()) return;
    int pos = -1;
    for (size_t i = 0; i < list.size(); ++i) if (list[i] == current) { pos = (int) i; break; }
    const int n = (int) list.size();
    pos = pos < 0 ? (delta > 0 ? 0 : n - 1) : ((pos + delta) % n + n) % n;
    load(list[(size_t) pos]);
}

std::vector<int> PresetManager::filter(const juce::String& search, const juce::String& category) const
{
    return filter(search, category, -1, -1);
}

std::vector<int> PresetManager::filter(const juce::String& search, const juce::String& category, int genre, int group) const
{
    std::vector<int> out;
    const auto tokens = juce::StringArray::fromTokens(search.trim().toLowerCase(), " ", "");
    for (int i = 0; i < numPresets(); ++i)
    {
        const auto& p = all[(size_t) i];
        if (category == "Favorites") { if (!isFavorite(i)) continue; }
        else if (category == "User") { if (p.factory) continue; }
        else if (category.isNotEmpty() && p.category != category) continue;
        if (genre >= 0 && (p.genres & (1u << genre)) == 0) continue;
        if (group >= 0 && p.group != group) continue;

        const auto hay = (p.name + " " + p.category + " " + groupNames()[juce::jlimit(0, kNumGroups - 1, p.group)]).toLowerCase();
        bool ok = true;
        for (const auto& t : tokens) if (!hay.contains(t)) { ok = false; break; }
        if (ok) out.push_back(i);
    }
    return out;
}

int PresetManager::indexOfName(const juce::String& name) const { return indexOfKey(name, {}); }

bool PresetManager::isModified() const
{
    if (current < 0 || current >= numPresets()) return false;
    const auto vals = resolveValues(all[(size_t) current]);
    for (int i = 0; i < kNumParams; ++i)
    {
        if (i == volume) continue;
        if (auto* p = apvts.getParameter(paramDef(i).id))
            if (std::abs(p->getValue() - p->convertTo0to1(vals[(size_t) i])) > 0.01f) return true;
    }
    return false;
}

bool PresetManager::resetToPreset()
{
    if (current < 0 || current >= numPresets()) return false;
    load(current);
    return true;
}

bool PresetManager::saveUserPreset(const juce::String& rawName, const juce::String& category)
{
    const auto name = rawName.trim();
    if (name.isEmpty()) return false;

    juce::XmlElement xml("AuraPreset");
    xml.setAttribute("name", name);
    xml.setAttribute("category", category);
    xml.setAttribute("version", 2);
    xml.setAttribute("genres", (int) curGenres);
    xml.setAttribute("group", curGroup);
    for (int i = 0; i < kNumParams; ++i)
        if (i != volume)
            if (auto* raw = apvts.getRawParameterValue(paramDef(i).id))
            {
            auto* e = xml.createNewChildElement("P");
            e->setAttribute("id", paramDef(i).id);
            e->setAttribute("v", (double) raw->load());
        }
    const auto file = userPresetDirectory().getChildFile(juce::File::createLegalFileName(name) + ".aurapreset");
    if (!xml.writeTo(file)) return false;

    rescanUserPresets();
    const int idx = indexOfKey(name, category);
    if (idx >= 0) { current = idx; currentNameStr = name; currentCategoryStr = category; }
    sendChangeMessage();
    return true;
}

bool PresetManager::loadPresetFile(const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName("AuraPreset")) return false;

    Preset p;
    p.factory = false;
    p.name = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
    p.category = xml->getStringAttribute("category", "User");
    p.genres = (unsigned) xml->getIntAttribute("genres", (int) kAllGenres);
    p.group = xml->getIntAttribute("group", GroupLead);
    for (auto* e : xml->getChildWithTagNameIterator("P"))
    {
        const int idx = indexOfParam(e->getStringAttribute("id"));
        if (idx >= 0) p.values.emplace_back(idx, (float) e->getDoubleAttribute("v"));
    }
    applyValues(resolveValues(p));
    curGenres = p.genres; curGroup = p.group;
    currentNameStr = p.name; currentCategoryStr = p.category;
    const int idx = indexOfKey(p.name, p.category);
    current = idx;
    if (idx < 0) { all.push_back(p); current = numPresets() - 1; }
    sendChangeMessage();
    return true;
}

void PresetManager::rescanUserPresets()
{
    all.resize((size_t) numFactoryPresets);
    for (const auto& f : userPresetDirectory().findChildFiles(juce::File::findFiles, true, "*.aurapreset"))
    {
        const auto xml = juce::XmlDocument::parse(f);
        if (xml == nullptr || !xml->hasTagName("AuraPreset")) continue;
        Preset p;
        p.factory = false;
        p.name = xml->getStringAttribute("name", f.getFileNameWithoutExtension());
        p.category = xml->getStringAttribute("category", "User");
        p.genres = (unsigned) xml->getIntAttribute("genres", (int) kAllGenres);
        p.group = xml->getIntAttribute("group", GroupLead);
        for (auto* e : xml->getChildWithTagNameIterator("P"))
        {
            const int idx = indexOfParam(e->getStringAttribute("id"));
            if (idx >= 0) p.values.emplace_back(idx, (float) e->getDoubleAttribute("v"));
        }
        all.push_back(std::move(p));
    }
    // keep `current` pointing at the same preset after a rescan
    current = indexOfKey(currentNameStr, currentCategoryStr);
}

void PresetManager::setCurrentFromState(const juce::String& name, const juce::String& category)
{
    currentNameStr = name.isEmpty() ? juce::String("Init") : name;
    currentCategoryStr = category;
    current = indexOfKey(name, category);
    if (current >= 0) { curGenres = all[(size_t) current].genres; curGroup = all[(size_t) current].group; }
    sendChangeMessage();
}
} // namespace aura
