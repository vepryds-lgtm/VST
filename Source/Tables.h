#pragma once
#include <vector>
#include <cmath>
#include <memory>

namespace aura
{
constexpr int   kTableSize = 2048;
constexpr int   kMipLevels = 11;      // level m serves fundamentals in [kMipBaseHz*2^m, kMipBaseHz*2^(m+1))
constexpr float kMipBaseHz = 10.0f;
constexpr int   kWTFrames  = 16;

// A bank holds `frames` single-cycle waveforms, each band-limited at kMipLevels different
// harmonic limits so that no oscillator ever aliases above Nyquist.
struct TableBank
{
    int frames = 1;
    std::vector<float> data; // frames * kMipLevels * (kTableSize + 1)   (+1 guard sample for interpolation)

    const float* table(int frame, int mip) const noexcept
    {
        return data.data() + (size_t) (frame * kMipLevels + mip) * (kTableSize + 1);
    }
};

struct WaveTables
{
    TableBank saw, square, tri, sine, wt;
    std::vector<float> sineLut; // kTableSize + 1

    // Shared, immutable, built once per sample rate (thread-safe).
    static std::shared_ptr<const WaveTables> get(double sampleRate);
};

inline int mipForFrequency(float hz) noexcept
{
    if (hz <= kMipBaseHz) return 0;
    const int m = (int) std::floor(std::log2(hz / kMipBaseHz));
    return m >= kMipLevels ? kMipLevels - 1 : m;
}

// Crash-proof table read: the index is masked, so even a phase of exactly 1.0 (or garbage) stays inside the table.
inline float readTable(const float* t, float phase01) noexcept
{
    const float pos = phase01 * (float) kTableSize;
    const int ip = (int) pos;
    const float f = pos - (float) ip;
    const int i = ip & (kTableSize - 1);
    return t[i] + (t[i + 1] - t[i]) * f;
}
} // namespace aura
