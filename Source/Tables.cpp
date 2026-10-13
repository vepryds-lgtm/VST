#include "Tables.h"
#include <mutex>
#include <map>
#include <algorithm>
#include <cstdint>

namespace aura
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

struct Peak { float c, g, w; };
struct FrameDef { float slope, evenGain; Peak p1, p2; bool scramble; };

// 16 hand-designed spectra, morphing from pure sine through saw / square, nasal, buzzy and glassy
// formant-like shapes to a bright digital texture.
constexpr FrameDef kFrames[kWTFrames] = {
    { 4.0f, 1.0f, { 0, 0, 1 },        { 0, 0, 1 },        false }, //  0 near-sine
    { 2.2f, 0.4f, { 0, 0, 1 },        { 0, 0, 1 },        false }, //  1 soft, hollow-ish
    { 1.6f, 1.0f, { 0, 0, 1 },        { 0, 0, 1 },        false }, //  2 mellow saw
    { 1.2f, 1.0f, { 0, 0, 1 },        { 0, 0, 1 },        false }, //  3 saw-ish
    { 1.0f, 1.0f, { 0, 0, 1 },        { 0, 0, 1 },        false }, //  4 saw
    { 1.0f, 0.0f, { 0, 0, 1 },        { 0, 0, 1 },        false }, //  5 square-ish
    { 0.8f, 0.0f, { 3, 2.5f, 1.0f },  { 0, 0, 1 },        false }, //  6 reedy
    { 1.0f, 1.0f, { 4.5f, 5, 1.2f },  { 0, 0, 1 },        false }, //  7 nasal
    { 1.0f, 1.0f, { 7, 6, 1.5f },     { 0, 0, 1 },        false }, //  8 buzzy
    { 0.9f, 1.0f, { 10, 5, 2.0f },    { 0, 0, 1 },        false }, //  9 brassy
    { 1.3f, 0.2f, { 12, 6, 2.5f },    { 0, 0, 1 },        false }, // 10 glassy
    { 1.1f, 0.0f, { 6, 3, 1.0f },     { 0, 0, 1 },        false }, // 11 hollow
    { 0.5f, 1.0f, { 0, 0, 1 },        { 0, 0, 1 },        false }, // 12 bright
    { 1.4f, 1.0f, { 3, 5, 0.9f },     { 9, 4, 1.5f },     false }, // 13 "aah"
    { 1.2f, 1.0f, { 2, 6, 0.7f },     { 14, 3, 2.0f },    false }, // 14 "ooh/eee" blend
    { 0.3f, 1.0f, { 0, 0, 1 },        { 0, 0, 1 },        true  }, // 15 digital texture
};

float frameAmp(const FrameDef& f, int h)
{
    float a = std::pow((float) h, -f.slope);
    if (f.p1.g > 0) a *= 1.0f + f.p1.g * std::exp(-std::pow(h - f.p1.c, 2.0f) / (2 * f.p1.w * f.p1.w));
    if (f.p2.g > 0) a *= 1.0f + f.p2.g * std::exp(-std::pow(h - f.p2.c, 2.0f) / (2 * f.p2.w * f.p2.w));
    if ((h & 1) == 0) a *= f.evenGain;
    return a;
}

int maxHarmonics(double sr, int mip)
{
    const double topHz = (double) kMipBaseHz * std::pow(2.0, mip + 1);
    return std::clamp((int) std::floor(0.5 * sr / topHz), 1, kTableSize / 2 - 1);
}

template <typename AmpFn, typename PhaseFn>
void fillTable(float* dst, const std::vector<float>& sineLut, int nHarm, AmpFn amp, PhaseFn phase)
{
    std::vector<double> acc(kTableSize, 0.0);
    for (int h = 1; h <= nHarm; ++h)
    {
        const double a = amp(h);
        if (a == 0.0) continue;
        const int ph = phase(h); // integer phase offset in table samples
        for (int i = 0; i < kTableSize; ++i)
            acc[(size_t) i] += a * sineLut[(size_t) (((h * i) + ph) & (kTableSize - 1))];
    }
    for (int i = 0; i < kTableSize; ++i) dst[i] = (float) acc[(size_t) i];
    dst[kTableSize] = dst[0];
}

template <typename AmpFn>
TableBank buildBank(double sr, const std::vector<float>& lut, AmpFn amp)
{
    TableBank b; b.frames = 1;
    b.data.assign((size_t) kMipLevels * (kTableSize + 1), 0.f);
    for (int m = 0; m < kMipLevels; ++m)
        fillTable(b.data.data() + (size_t) m * (kTableSize + 1), lut, maxHarmonics(sr, m), amp, [](int) { return 0; });
    return b;
}

TableBank buildWavetable(double sr, const std::vector<float>& lut)
{
    TableBank b; b.frames = kWTFrames;
    b.data.assign((size_t) kWTFrames * kMipLevels * (kTableSize + 1), 0.f);
    for (int f = 0; f < kWTFrames; ++f)
    {
        const auto& fd = kFrames[f];
        std::uint32_t seed = 12345u + (std::uint32_t) f * 977u;
        std::vector<int> phases(kTableSize / 2 + 1, 0);
        if (fd.scramble)
            for (auto& p : phases) { seed = seed * 1664525u + 1013904223u; p = (int) ((seed >> 8) & (kTableSize - 1)); }

        // normalise every mip of a frame by the peak of the richest (mip 0) version
        float* t0 = b.data.data() + (size_t) (f * kMipLevels) * (kTableSize + 1);
        fillTable(t0, lut, maxHarmonics(sr, 0), [&](int h) { return (double) frameAmp(fd, h); },
                  [&](int h) { return phases[(size_t) h]; });
        float peak = 0.f;
        for (int i = 0; i < kTableSize; ++i) peak = std::max(peak, std::abs(t0[i]));
        const float norm = peak > 1e-6f ? 0.9f / peak : 1.f;

        for (int m = 0; m < kMipLevels; ++m)
        {
            float* t = b.data.data() + (size_t) (f * kMipLevels + m) * (kTableSize + 1);
            fillTable(t, lut, maxHarmonics(sr, m), [&](int h) { return (double) frameAmp(fd, h) * norm; },
                      [&](int h) { return phases[(size_t) h]; });
        }
    }
    return b;
}

std::shared_ptr<const WaveTables> build(double sr)
{
    auto wt = std::make_shared<WaveTables>();
    wt->sineLut.resize(kTableSize + 1);
    for (int i = 0; i <= kTableSize; ++i)
        wt->sineLut[(size_t) i] = (float) std::sin(2.0 * kPi * i / kTableSize);

    const auto& lut = wt->sineLut;
    wt->sine   = buildBank(sr, lut, [](int h) { return h == 1 ? 1.0 : 0.0; });
    // rising saw
    wt->saw    = buildBank(sr, lut, [](int h) { return -2.0 / kPi / h; });
    wt->square = buildBank(sr, lut, [](int h) { return (h & 1) ? 4.0 / kPi / h : 0.0; });
    wt->tri    = buildBank(sr, lut, [](int h) { return (h & 1) ? ((((h - 1) / 2) & 1) ? -1.0 : 1.0) * 8.0 / (kPi * kPi) / ((double) h * h) : 0.0; });
    wt->wt     = buildWavetable(sr, lut);
    return wt;
}
} // namespace

std::shared_ptr<const WaveTables> WaveTables::get(double sampleRate)
{
    static std::mutex m;
    static std::map<int, std::shared_ptr<const WaveTables>> cache;
    const int key = (int) std::lround(sampleRate);
    std::lock_guard<std::mutex> lock(m);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    auto t = build((double) key);
    cache[key] = t;
    return t;
}
} // namespace aura
