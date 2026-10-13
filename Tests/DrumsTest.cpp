// AuraDrums - objective QA of the sound library (no JUCE needed).
// Usage: AuraDrumsTest [--report] [--wav <dir>]
#include "DrumSounds.h"
#include "DrumEngine.h"
#include <atomic>
#include <new>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif
// count heap allocations so the test can prove that rendering never allocates
static std::atomic<long> gAllocs { 0 };
static std::atomic<bool> gCount { false };
void* operator new(std::size_t n) { if (gCount) ++gAllocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

using namespace drums;
static const double kPiT = 3.14159265358979323846;
static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; std::printf("  FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void writeWav(const std::string& path, const Sample& s)
{
    FILE* f = std::fopen(path.c_str(), "wb"); if (!f) return;
    const int ch = s.isMono() ? 1 : 2, n = s.length(), sr = (int) s.sr;
    auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); }; auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); w32((uint32_t) (36 + n * ch * 3)); std::fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(1); w16((uint16_t) ch);
    w32((uint32_t) sr); w32((uint32_t) (sr * ch * 3)); w16((uint16_t) (ch * 3)); w16(24); std::fwrite("data", 1, 4, f); w32((uint32_t) (n * ch * 3));
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < ch; ++c)
        {
            const float v = std::max(-1.f, std::min(1.f, (c == 0 ? s.l : s.r)[(size_t) i]));
            int32_t q = (int32_t) std::lrint(v * 8388607.0); uint8_t b[3] = { (uint8_t) (q & 255), (uint8_t) ((q >> 8) & 255), (uint8_t) ((q >> 16) & 255) };
            std::fwrite(b, 1, 3, f);
        }
    std::fclose(f);
}

// strongest partial (Goertzel scan) in [lo,hi] Hz inside the time window [t0,t1]
static double dominantHz(const Sample& s, double t0, double t1, double lo, double hi, double step = 0.05)
{
    const int a = (int) (t0 * s.sr), b = std::min(s.length(), (int) (t1 * s.sr));
    double bestF = 0, bestP = -1;
    for (double f = lo; f <= hi; f += step)
    {
        const double w = 2.0 * kPiT * f / s.sr, cw = 2.0 * std::cos(w);
        double s1 = 0, s2 = 0;
        for (int i = a; i < b; ++i)
        {
            const double win = 0.5 - 0.5 * std::cos(2.0 * kPiT * (i - a) / (double) (b - a));
            const double s0 = s.l[(size_t) i] * win + cw * s1 - s2; s2 = s1; s1 = s0;
        }
        const double p = s1 * s1 + s2 * s2 - cw * s1 * s2;
        if (p > bestP) { bestP = p; bestF = f; }
    }
    return bestF;
}

static double corr(const Sample& a, const Sample& b)
{
    const int n = std::min(a.length(), b.length());
    double ab = 0, aa = 0, bb = 0;
    for (int i = 0; i < n; ++i) { ab += (double) a.l[(size_t) i] * b.l[(size_t) i]; aa += (double) a.l[(size_t) i] * a.l[(size_t) i]; bb += (double) b.l[(size_t) i] * b.l[(size_t) i]; }
    return ab / std::sqrt(aa * bb + 1e-30);
}

int main(int argc, char** argv)
{
    bool report = false; std::string wavDir;
    for (int i = 1; i < argc; ++i) { if (!std::strcmp(argv[i], "--report")) report = true; if (!std::strcmp(argv[i], "--wav") && i + 1 < argc) wavDir = argv[++i]; }
    const int N = numSounds();
    std::printf("AuraDrums QA: %d sounds\n", N);

    // ---- catalogue
    std::map<int, int> perCat; std::map<std::string, int> names;
    for (int i = 0; i < N; ++i) { perCat[(int) soundInfo(i).cat]++; names[soundInfo(i).name]++; }
    std::printf("[catalogue]\n");
    for (int c = 0; c < kNumCats; ++c)
    {
        std::printf("  %-14s %d\n", catName((Cat) c), perCat[c]);
        if (c <= (int) Cat::Hat) CHECK(perCat[c] >= 5 && perCat[c] <= 10, "%s has %d sounds (want 5-10)", catName((Cat) c), perCat[c]);
        else CHECK(perCat[c] >= 10, "%s has only %d sounds", catName((Cat) c), perCat[c]);
    }
    for (auto& kv : names) CHECK(kv.second == 1, "duplicate name %s", kv.first.c_str());
    for (int i = 0; i < N; ++i) CHECK(!soundInfo(i).tag.empty(), "%s has no description", soundInfo(i).name.c_str());

    // ---- render everything at 3 sample rates
    const double rates[3] = { 44100.0, 48000.0, 96000.0 };
    std::vector<std::shared_ptr<const Sample>> bank[3];
    for (int r = 0; r < 3; ++r)
    {
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < N; ++i) bank[r].push_back(renderSound(i, rates[r]));
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        std::printf("[render] %.0f Hz: %.0f ms for all sounds\n", rates[r], ms);
        CHECK(ms < 6000.0, "rendering the bank at %.0f Hz took %.0f ms", rates[r], ms);
    }

    // ---- per sound sanity
    std::printf("[per-sound]\n");
    struct Cs { float peakCeil; };
    const float ceilDb[kNumCats] = { -1.5f, -1.5f, -1.5f, -1.5f, -3.f, -3.f, -3.f };
    std::vector<Measure> M[3];
    for (int r = 0; r < 3; ++r) for (int i = 0; i < N; ++i) M[r].push_back(measure(*bank[r][(size_t) i]));
    for (int r = 0; r < 3; ++r)
        for (int i = 0; i < N; ++i)
        {
            const Sample& s = *bank[r][(size_t) i]; const auto& si = soundInfo(i); const auto& m = M[r][(size_t) i];
            bool finite = true; for (float v : s.l) finite &= std::isfinite(v); for (float v : s.r) finite &= std::isfinite(v);
            CHECK(finite, "%s: non-finite sample", si.name.c_str());
            CHECK(m.peakDb <= ceilDb[(int) si.cat] + 0.05, "%s @%.0f: peak %.2f dB above ceiling", si.name.c_str(), rates[r], m.peakDb);
            CHECK(m.peakDb > -30.0, "%s: far too quiet (peak %.1f dB)", si.name.c_str(), m.peakDb);
            CHECK(std::abs(m.dcOffset) < 0.002, "%s: DC offset %.4f", si.name.c_str(), m.dcOffset);
            CHECK(m.lengthSec > 0.02 && m.lengthSec < 6.0, "%s: odd length %.2f s", si.name.c_str(), m.lengthSec);
            CHECK(std::abs(s.l.front()) < 0.01f && std::abs(s.l.back()) < 0.001f, "%s: click at start/end (%.4f / %.5f)", si.name.c_str(), s.l.front(), s.l.back());
            CHECK(std::abs(s.sr - rates[r]) < 1e-6, "%s: wrong stored sample rate", si.name.c_str());
            if (si.cat == Cat::Kick || si.cat == Cat::Bass808) CHECK(s.isMono(), "%s: low-end must be mono", si.name.c_str());
        }

    // ---- sample-rate independence (loudness and brightness must not depend on the host rate)
    std::printf("[sample-rate independence]\n");
    double worstL = 0, worstC = 0;
    for (int i = 0; i < N; ++i)
        for (int r = 1; r < 3; ++r)
        {
            const double dl = std::abs(M[r][(size_t) i].loudnessDb - M[0][(size_t) i].loudnessDb);
            const double dc = std::abs(std::log2(M[r][(size_t) i].centroidHz / M[0][(size_t) i].centroidHz));
            worstL = std::max(worstL, dl); worstC = std::max(worstC, dc);
            CHECK(dl < 1.0, "%s: loudness differs %.2f dB between %.0f and %.0f Hz", soundInfo(i).name.c_str(), dl, rates[0], rates[r]);
            CHECK(dc < 0.35, "%s: centroid moves %.2f oct between sample rates", soundInfo(i).name.c_str(), dc);
        }
    std::printf("  worst loudness difference %.2f dB, worst centroid shift %.2f oct\n", worstL, worstC);

    // ---- determinism
    {
        auto again = renderSound(indexOfSound("Plan A Kick"), 44100.0);
        const auto& a = *bank[0][(size_t) indexOfSound("Plan A Kick")];
        CHECK(again->l == a.l, "rendering is not deterministic");
    }

    // ---- mix balance
    std::printf("[balance @44.1k]  loudness (K-weighted, loudest 80 ms) per category\n");
    std::map<int, std::vector<double>> ld, pk;
    for (int i = 0; i < N; ++i) { ld[(int) soundInfo(i).cat].push_back(M[0][(size_t) i].loudnessDb); pk[(int) soundInfo(i).cat].push_back(M[0][(size_t) i].peakDb); }
    double catMean[kNumCats];
    for (int c = 0; c < kNumCats; ++c)
    {
        auto& v = ld[c]; const double mn = *std::min_element(v.begin(), v.end()), mx = *std::max_element(v.begin(), v.end());
        double s = 0; for (double x : v) s += x; catMean[c] = s / v.size();
        std::printf("  %-14s loudness %.1f .. %.1f (spread %.1f dB)  peak %.1f .. %.1f\n", catName((Cat) c), mn, mx, mx - mn,
                    *std::min_element(pk[c].begin(), pk[c].end()), *std::max_element(pk[c].begin(), pk[c].end()));
        if (c != (int) Cat::Clap) CHECK(mx - mn < 2.0, "%s: loudness spread %.1f dB inside the category", catName((Cat) c), mx - mn);
        else CHECK(mx - mn < 6.0, "%s: loudness spread too wide", catName((Cat) c));   // contains the intentional -5 dB ghost clap
    }
    CHECK(catMean[(int) Cat::Kick] > catMean[(int) Cat::Bass808] - 0.1, "kicks should be at least as loud as 808s");
    CHECK(catMean[(int) Cat::Snare] > catMean[(int) Cat::Hat] + 6, "hats must sit clearly below snares");
    CHECK(catMean[(int) Cat::Clap] > catMean[(int) Cat::Hat] + 6, "hats must sit clearly below claps");

    // ---- spectral character
    std::printf("[character]\n");
    auto centMean = [&](Cat c) { double s = 0; int k = 0; for (int i = 0; i < N; ++i) if (soundInfo(i).cat == c) { s += M[0][(size_t) i].centroidHz; ++k; } return s / k; };
    std::printf("  centroids: kick %.0f  808 %.0f  clap %.0f  snare %.0f  hat %.0f\n", centMean(Cat::Kick), centMean(Cat::Bass808), centMean(Cat::Clap), centMean(Cat::Snare), centMean(Cat::Hat));
    CHECK(centMean(Cat::Kick) < 200 && centMean(Cat::Bass808) < 250, "low end is not low");
    CHECK(centMean(Cat::Kick) < centMean(Cat::Clap) && centMean(Cat::Clap) < centMean(Cat::Hat), "centroid order kick < clap < hat broken");
    CHECK(centMean(Cat::Hat) > 6000, "hats are not bright (centroid %.0f)", centMean(Cat::Hat));
    for (int i = 0; i < N; ++i)
        if (soundInfo(i).cat == Cat::Hat) CHECK(M[0][(size_t) i].centroidHz > 6000, "%s: centroid only %.0f Hz", soundInfo(i).name.c_str(), M[0][(size_t) i].centroidHz);

    // ---- pitched sounds
    std::printf("[808 pitch]\n");
    const double f0 = 440.0 * std::pow(2.0, (37 - 69) / 12.0) / 2.0;   // the 808 sample plays one octave below the key (C#1, 34.65 Hz on key 37), same as the AuraSynth 808 presets
    for (int i = 0; i < N; ++i)
    {
        const auto& si = soundInfo(i); if (si.cat != Cat::Bass808) continue;
        const double f = dominantHz(*bank[0][(size_t) i], 0.35, 0.9, 25, 120);
        std::printf("  %-16s root %d  fundamental %.2f Hz (target %.2f)\n", si.name.c_str(), si.rootNote, f, f0);
        CHECK(si.rootNote == 37 && si.gated, "%s: 808 must be pitched at 37 and gated", si.name.c_str());
        CHECK(std::abs(f - f0) < 1.5, "%s: fundamental %.2f Hz, expected %.2f Hz", si.name.c_str(), f, f0);
    }

    // ---- uniqueness inside a category
    std::printf("[uniqueness]\n");
    int near = 0;
    for (int i = 0; i < N; ++i)
        for (int j = i + 1; j < N; ++j)
        {
            if (soundInfo(i).cat != soundInfo(j).cat) continue;
            const double c = std::abs(corr(*bank[0][(size_t) i], *bank[0][(size_t) j]));
            const double dc = std::abs(std::log2(M[0][(size_t) i].centroidHz / M[0][(size_t) j].centroidHz));
            const double dl = std::abs(std::log2(M[0][(size_t) i].lengthSec / M[0][(size_t) j].lengthSec));
            if (c > 0.95 && dc < 0.08 && dl < 0.15) { ++near; std::printf("  near-duplicate: %s / %s (corr %.3f)\n", soundInfo(i).name.c_str(), soundInfo(j).name.c_str(), c); }
        }
    CHECK(near == 0, "%d near-duplicate pairs", near);

    // ---- kits
    std::printf("[kits]\n");
    const auto& ks = kits();
    CHECK(ks.size() >= 4, "too few kits");
    for (size_t k = 0; k < ks.size(); ++k)
    {
        int mapped = 0;
        for (auto& e : ks[k].map) { const int idx = indexOfSound(e.sound); CHECK(idx >= 0, "kit %s refers to unknown sound %s", ks[k].name, e.sound); if (idx >= 0) ++mapped; }
        CHECK(kitSoundForNote((int) k, 36) >= 0 && kitSoundForNote((int) k, 38) >= 0 && kitSoundForNote((int) k, 39) >= 0 && kitSoundForNote((int) k, 42) >= 0 && kitSoundForNote((int) k, 46) >= 0,
              "kit %s lacks kick/snare/clap/hats on the GM notes", ks[k].name);
        CHECK(soundInfo(kitSoundForNote((int) k, 36)).cat == Cat::Kick && soundInfo(kitSoundForNote((int) k, 38)).cat == Cat::Snare &&
              soundInfo(kitSoundForNote((int) k, 39)).cat == Cat::Clap && soundInfo(kitSoundForNote((int) k, 42)).cat == Cat::Hat, "kit %s has wrong categories on GM notes", ks[k].name);
        std::printf("  %-18s %d notes\n", ks[k].name, mapped);
    }
    CHECK(kitSoundForNote(0, 3) == -1, "unmapped note should return -1");


    // ---- playback engine
    std::printf("[engine]\n");
    {
        auto bank = Bank::get();
        CHECK((int) bank->samples.size() == N && (int) bank->thumbs.size() == N, "bank size");
        auto run = [&](Engine& e, double sr, double secs, std::vector<float>* outL = nullptr, std::vector<float>* outR = nullptr)
        {
            const int total = (int) (secs * sr); std::vector<float> L((size_t) total, 0.f), R((size_t) total, 0.f);
            for (int i = 0; i < total; i += 256) { const int n = std::min(256, total - i); e.render(L.data() + i, R.data() + i, n); }
            if (outL) *outL = L;
            if (outR) *outR = R;
        };
        auto domHz = [&](const std::vector<float>& x, double sr, double t0, double t1, double lo, double hi)
        {
            Sample s; s.sr = sr; s.l = x; return dominantHz(s, t0, t1, lo, hi, 0.05);
        };
        const int sub = indexOfSound("Clean Sub 808");
        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            Engine e; e.prepare(sr);
            Settings st; st.sound = sub; e.setSettings(st);
            std::vector<float> x;
            e.noteOn(37, 1.f); run(e, sr, 1.0, &x);
            const double f1 = domHz(x, sr, 0.35, 0.9, 25, 140);
            CHECK(std::abs(f1 - 34.65) < 0.6, "808 root note at %.0f Hz host: %.2f Hz", sr, f1);
            e.reset(); e.noteOn(49, 1.f); run(e, sr, 1.0, &x);
            const double f2 = domHz(x, sr, 0.35, 0.9, 25, 140);
            CHECK(std::abs(f2 - 69.3) < 1.0, "808 one octave up at %.0f Hz host: %.2f Hz", sr, f2);
            st.keyMode = KeyMode::Fixed; e.setSettings(st); e.reset(); e.noteOn(61, 1.f); run(e, sr, 1.0, &x);
            CHECK(std::abs(domHz(x, sr, 0.35, 0.9, 25, 140) - 34.65) < 0.6, "Fixed key mode must not transpose");
            st.keyMode = KeyMode::Auto; st.tuneSemis = 12.f; e.setSettings(st); e.reset(); e.noteOn(37, 1.f); run(e, sr, 1.0, &x);
            CHECK(std::abs(domHz(x, sr, 0.35, 0.9, 25, 140) - 69.3) < 1.0, "Tune +12 st must double the pitch");
        }
        // non-pitched sounds do not move with the key in Auto, they do in "All keys pitched"
        {
            Engine e; e.prepare(48000.0); Settings st; st.sound = indexOfSound("Plan A Kick"); e.setSettings(st);
            std::vector<float> a, b; e.noteOn(36, 1.f); run(e, 48000, 0.3, &a); e.reset(); e.noteOn(60, 1.f); run(e, 48000, 0.3, &b);
            CHECK(a == b, "kick must sound identical on every key in Auto mode");
            st.keyMode = KeyMode::Pitched; e.setSettings(st); e.reset(); e.noteOn(72, 1.f); run(e, 48000, 0.3, &b);
            CHECK(a != b, "All-keys-pitched must transpose a kick");
        }
        // velocity is linear
        {
            Engine e; e.prepare(48000.0); Settings st; st.sound = indexOfSound("Plan A Snare"); e.setSettings(st);
            std::vector<float> a, b; e.noteOn(38, 1.f); run(e, 48000, 0.2, &a); e.reset(); e.noteOn(38, 0.5f); run(e, 48000, 0.2, &b);
            float pa = 0, pb = 0; for (float v : a) pa = std::max(pa, std::abs(v)); for (float v : b) pb = std::max(pb, std::abs(v));
            CHECK(std::abs(pb / pa - 0.5f) < 0.01f, "velocity scaling %.3f", pb / pa);
        }
        // 808 is monophonic, gated note-off, length control
        {
            Engine e; e.prepare(44100.0); Settings st; st.sound = sub; e.setSettings(st);
            e.noteOn(37, 1.f); run(e, 44100, 0.05); e.noteOn(40, 1.f); run(e, 44100, 0.05);
            CHECK(e.activeVoices() == 1, "808 must be monophonic (voices: %d)", e.activeVoices());
            e.noteOff(40); run(e, 44100, 0.1);
            CHECK(e.activeVoices() == 0, "808 must stop after note-off");
            st.length = 0.25f; e.setSettings(st); e.reset(); e.noteOn(37, 1.f); run(e, 44100, 0.5); CHECK(e.activeVoices() == 1, "808 at length 25%% should still ring at 0.5 s (len %.2f s)", 0.25 * bank->samples[(size_t) sub]->length() / kBankRate);
            run(e, 44100, 1.5); CHECK(e.activeVoices() == 0, "length 25%% must end the sound");
        }
        // choke + kit + unmapped notes
        {
            Engine e; e.prepare(48000.0); Settings st; st.kitMode = true; st.kit = 0; e.setSettings(st);
            e.noteOn(46, 1.f); run(e, 48000, 0.02); CHECK(e.activeVoices() == 1, "open hat should be playing");
            e.noteOn(42, 1.f); run(e, 48000, 0.02); CHECK(e.activeVoices() == 1, "closed hat must choke the open hat (voices %d)", e.activeVoices());
            e.reset(); e.noteOn(3, 1.f); CHECK(e.activeVoices() == 0, "unmapped note must not play");
            e.noteOn(36, 1.f); e.noteOn(38, 1.f); e.noteOn(39, 1.f); CHECK(e.activeVoices() == 3, "kick+snare+clap should layer");
        }
        // voice stealing, preview, allocation-free rendering
        {
            Engine e; e.prepare(48000.0); Settings st; st.sound = indexOfSound("Long Riser"); e.setSettings(st);
            std::vector<float> a(256), b(256);
            for (int i = 0; i < 100; ++i) e.noteOn(40 + i % 30, 1.f);
            CHECK(e.activeVoices() <= Engine::kVoices, "too many voices");
            gAllocs = 0; gCount = true;
            for (int i = 0; i < 200; ++i) { std::fill(a.begin(), a.end(), 0.f); std::fill(b.begin(), b.end(), 0.f); e.noteOn(40 + i % 12, 0.8f); e.noteOff(40 + i % 12); e.render(a.data(), b.data(), 256); }
            e.preview(indexOfSound("Plan A Kick"), 1.f); e.render(a.data(), b.data(), 256); e.allNotesOff();
            gCount = false;
            CHECK(gAllocs == 0, "engine allocated %ld times while rendering", (long) gAllocs);
            bool fin = true; for (float v : a) fin &= std::isfinite(v); CHECK(fin, "non finite engine output");
            e.reset(); e.preview(indexOfSound("Plan A Kick"), 1.f); e.preview(indexOfSound("Plan A Snare"), 1.f); run(e, 48000, 0.05);
            CHECK(e.activeVoices() == 1, "a new preview must replace the old one (voices %d)", e.activeVoices());
        }
    }

    if (report)
    {
        std::printf("[report]\n");
        for (int i = 0; i < N; ++i)
        {
            const auto& m = M[0][(size_t) i];
            std::printf("  %2d %-14s %-18s pk %6.1f ld %6.1f len %5.2f cen %6.0f\n", i, catName(soundInfo(i).cat), soundInfo(i).name.c_str(), m.peakDb, m.loudnessDb, m.lengthSec, m.centroidHz);
        }
    }
    if (!wavDir.empty())
        for (int i = 0; i < N; ++i)
        {
            std::string nm = soundInfo(i).name; for (auto& ch : nm) if (ch == ' ') ch = '_';
            char buf[64]; std::snprintf(buf, sizeof buf, "%s/%s_%02d_", wavDir.c_str(), catSingular(soundInfo(i).cat), i);
            std::string cat = buf; for (auto& ch : cat) if (ch == ' ' || ch == '&') ch = '_';
            if (soundInfo(i).cat == Cat::Bass808) nm += "_tuned_C#1";   // the 808 samples are tuned to C#1 (34.65 Hz)
            writeWav(cat + nm + ".wav", *bank[0][(size_t) i]);
        }

    std::printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
