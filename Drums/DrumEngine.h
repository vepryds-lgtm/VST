#pragma once
// AuraDrums - the playback engine (plain C++, no JUCE, so it can be unit-tested on its own).
#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include "DrumSounds.h"

namespace drums
{
constexpr double kBankRate = 48000.0;      // every sound is synthesised once at this rate and resampled on playback
constexpr int kThumbPoints = 72;

// The whole library, rendered once per process and shared by every plugin instance (immutable after construction).
struct Bank
{
    std::vector<std::shared_ptr<const Sample>> samples;
    std::vector<std::array<float, kThumbPoints>> thumbs;   // 0..1 envelope for the sound tiles
    static std::shared_ptr<const Bank> get();
};

enum class KeyMode { Auto = 0, Pitched, Fixed };

struct Settings
{
    bool  kitMode = false;
    int   sound   = 0;          // Single mode: the selected sound
    int   kit     = 0;          // Kit mode: index into kits()
    KeyMode keyMode = KeyMode::Auto;
    float tuneSemis = 0.f;      // -12 .. +12
    float length    = 1.f;      // 0.05 .. 1  (shorten only)
};

class Engine
{
public:
    static constexpr int kVoices = 32;

    explicit Engine(std::shared_ptr<const Bank> b = Bank::get());
    void prepare(double hostSampleRate);
    void reset();                                   // silence everything at once
    void setSettings(const Settings& s) { st = s; }
    const Settings& settings() const { return st; }

    void noteOn(int note, float velocity01);         // follows the Single / Kit mode
    void noteOff(int note);
    void allNotesOff();                              // releases gated sounds, lets one-shots ring out
    void preview(int sound, float velocity01);       // editor audition: root pitch, ignores the mode
    void render(float* left, float* right, int numSamples);   // ADDS into the buffers

    int  activeVoices() const;
    const Bank& bank() const { return *bankPtr; }

private:
    struct Voice
    {
        const Sample* s = nullptr;
        double pos = 0.0, ratio = 1.0;
        float gain = 1.f, env = 1.f, envStep = 0.f;     // env: 1 = full level, ramps to 0 on a fade-out
        int note = -1, sound = -1, choke = 0;
        bool active = false, gated = false, held = false, releasing = false, preview = false, bass = false;
        double endPos = 0.0, fadeLen = 0.0;
        uint64_t age = 0;
    };
    void start(int sound, double semis, float vel, int note, bool isPreview);
    void fade(Voice& v, double seconds);
    static float hermite(const float* d, int n, double pos);

    std::shared_ptr<const Bank> bankPtr;
    Settings st;
    double hostRate = 48000.0;
    std::array<Voice, kVoices> voices;
    uint64_t counter = 0;
};
} // namespace drums
