#pragma once
// AuraDrums - the sound library. Every sound is synthesised in code (no samples, no copyright issues),
// deterministic (fixed random seeds) and loudness-matched per category so the sounds mix together without fader work.
#include <memory>
#include <string>
#include <vector>

namespace drums
{
enum class Cat { Kick = 0, Bass808, Clap, Snare, Hat, Perc, Fx };
constexpr int kNumCats = 7;
const char* catName(Cat c);          // "KICKS", "808s", ...
const char* catSingular(Cat c);

struct Sample
{
    std::vector<float> l, r;         // r is empty for mono sounds
    double sr = 44100.0;
    bool isMono() const { return r.empty(); }
    int length() const { return (int) l.size(); }
};

struct SoundInfo
{
    std::string name;
    Cat cat;
    std::string tag;                 // one-line character description shown in the browser
    int rootNote = -1;               // MIDI note at which the sample plays at its original pitch; -1 = not pitched
    bool gated = false;              // note-off fades the sound out (808s), otherwise it is a one-shot
    int choke = 0;                   // sounds of the same non-zero choke group cut each other (closed / open hat)
    float levelDb = 0.f;             // per-sound mix trim inside its category
};

int numSounds();
const SoundInfo& soundInfo(int index);
int indexOfSound(const std::string& name);                              // -1 if unknown
std::shared_ptr<const Sample> renderSound(int index, double sampleRate); // deterministic, ~ms each; call from a non-audio thread

// objective numbers used by the QA tool and by the mixer
struct Measure { double peakDb = -120, loudnessDb = -120, lengthSec = 0, centroidHz = 0, dcOffset = 0; };
Measure measure(const Sample& s);

// ---- kits: GM-style key maps so a normal drum MIDI file plays straight away
struct KitEntry { int note; const char* sound; };
struct Kit { const char* name; std::vector<KitEntry> map; };
const std::vector<Kit>& kits();
int kitSoundForNote(int kit, int note);   // sound index or -1
} // namespace drums
