#include "DrumSounds.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <functional>
#include <map>

namespace drums
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
inline double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float db2g(double db) { return (float) std::pow(10.0, db / 20.0); }

// ------------------------------------------------------------------------------------------------- DSP helpers
struct Rng
{
    uint32_t s;
    explicit Rng(uint32_t seed = 1) : s(seed * 2654435761u + 12345u) { if (s == 0) s = 1; }
    float white() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.f / 2147483648.f); }
    float uni()   { return white() * 0.5f + 0.5f; }
};

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float process(float x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return (float) y; }
    static Biquad lp(double f, double q, double sr)
    {
        f = clampd(f, 10.0, sr * 0.45); const double w = 2 * kPi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q), a0 = 1 + al;
        Biquad b; b.b0 = (1 - c) / 2 / a0; b.b1 = (1 - c) / a0; b.b2 = b.b0; b.a1 = -2 * c / a0; b.a2 = (1 - al) / a0; return b;
    }
    static Biquad hp(double f, double q, double sr)
    {
        f = clampd(f, 10.0, sr * 0.45); const double w = 2 * kPi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q), a0 = 1 + al;
        Biquad b; b.b0 = (1 + c) / 2 / a0; b.b1 = -(1 + c) / a0; b.b2 = b.b0; b.a1 = -2 * c / a0; b.a2 = (1 - al) / a0; return b;
    }
    static Biquad bp(double f, double q, double sr)   // constant 0 dB peak gain
    {
        f = clampd(f, 10.0, sr * 0.45); const double w = 2 * kPi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q), a0 = 1 + al;
        Biquad b; b.b0 = al / a0; b.b1 = 0; b.b2 = -al / a0; b.a1 = -2 * c / a0; b.a2 = (1 - al) / a0; return b;
    }
};

// zero-delay-feedback state variable filter that can be modulated per sample
struct Svf
{
    double ic1 = 0, ic2 = 0;
    void run(float x, double f, double q, double sr, float& lp, float& bp, float& hp)
    {
        const double g = std::tan(kPi * clampd(f, 10.0, sr * 0.45) / sr), k = 1.0 / q;
        const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const double v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        bp = (float) v1; lp = (float) v2; hp = (float) (x - k * v1 - v2);
    }
};

inline double polyBlep(double t, double dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
    return 0.0;
}
struct Square
{
    double ph = 0;
    float next(double inc)
    {
        ph += inc; if (ph >= 1.0) ph -= 1.0;
        double v = ph < 0.5 ? 1.0 : -1.0;
        v += polyBlep(ph, inc);
        v -= polyBlep(std::fmod(ph + 0.5, 1.0), inc);
        return (float) v;
    }
};
struct Saw
{
    double ph = 0;
    float next(double inc) { ph += inc; if (ph >= 1.0) ph -= 1.0; return (float) (2.0 * ph - 1.0 - polyBlep(ph, inc)); }
};

struct Ctx
{
    double sr = 44100.0;
    int n = 0;
    bool st = false;
    std::vector<float> L, R;
    Rng rng { 1 };
    void alloc(double secs, bool stereo)
    {
        n = std::max(32, (int) (secs * sr));
        L.assign((size_t) n, 0.f);
        R.assign(stereo ? (size_t) n : 0u, 0.f);
        st = stereo;
    }
    double t(int i) const { return (double) i / sr; }
};

inline float expEnv(double t, double tau) { return (float) std::exp(-t / tau); }
inline float onset(double t, double a) { return a <= 0 ? 1.f : (float) (1.0 - std::exp(-t / a)); }
inline float sat(float x, float drive) { return drive < 0.01f ? x : std::tanh(drive * x) / std::tanh(drive); }
inline float crush(float x, int bits) { if (bits <= 0) return x; const float q = (float) (1 << bits); return std::round(x * q) / q; }

// small Schroeder room, baked into the sound (this is what gives claps / snares their "space" without a reverb plug-in)
void addRoom(Ctx& c, float mix, float rt60, float hpHz = 350.f, float dampHz = 5500.f)
{
    if (mix <= 0.f) return;
    if (!c.st) { c.R = c.L; c.st = true; }
    static const double dl[2][4] = { { 0.0297, 0.0371, 0.0411, 0.0437 }, { 0.0307, 0.0383, 0.0425, 0.0451 } };
    static const double apT[2][2] = { { 0.0050, 0.0017 }, { 0.0053, 0.0019 } };
    std::vector<float> mid((size_t) c.n);
    Biquad hpf = Biquad::hp(hpHz, 0.7, c.sr);
    for (int i = 0; i < c.n; ++i) mid[(size_t) i] = hpf.process(0.5f * (c.L[(size_t) i] + c.R[(size_t) i]));
    const float d = (float) std::exp(-2 * kPi * dampHz / c.sr);
    std::vector<float> wet[2];
    for (int ch = 0; ch < 2; ++ch)
    {
        std::vector<float> cb[4]; int ci[4] {}; float cf[4] {}; float g[4];
        for (int k = 0; k < 4; ++k) { cb[k].assign((size_t) std::max(8, (int) (dl[ch][k] * c.sr)), 0.f); g[k] = (float) std::pow(10.0, -3.0 * dl[ch][k] / std::max(0.05f, rt60)); }
        std::vector<float> ab[2]; int ai[2] {};
        for (int k = 0; k < 2; ++k) ab[k].assign((size_t) std::max(4, (int) (apT[ch][k] * c.sr)), 0.f);
        wet[ch].assign((size_t) c.n, 0.f);
        for (int i = 0; i < c.n; ++i)
        {
            const float x = mid[(size_t) i];
            float acc = 0.f;
            for (int k = 0; k < 4; ++k)
            {
                const float y = cb[k][(size_t) ci[k]];
                cf[k] = y * (1.f - d) + cf[k] * d;
                cb[k][(size_t) ci[k]] = x + cf[k] * g[k];
                if (++ci[k] >= (int) cb[k].size()) ci[k] = 0;
                acc += y;
            }
            acc *= 0.25f;
            for (int k = 0; k < 2; ++k)
            {
                const float bo = ab[k][(size_t) ai[k]];
                const float out = -acc + bo;
                ab[k][(size_t) ai[k]] = acc + bo * 0.5f;
                if (++ai[k] >= (int) ab[k].size()) ai[k] = 0;
                acc = out;
            }
            wet[ch][(size_t) i] = acc;
        }
    }
    for (int i = 0; i < c.n; ++i) { c.L[(size_t) i] += wet[0][(size_t) i] * mix * 2.f; c.R[(size_t) i] += wet[1][(size_t) i] * mix * 2.f; }
}

// ------------------------------------------------------------------------------------------------- sound families
struct KickP { float f0, f1, sweep, decay, body2, click, clickHz, drive, len, lp; };
void kick(Ctx& c, const KickP& p)
{
    c.alloc(p.len, false);
    double ph = 0;
    Biquad cb = Biquad::bp(p.clickHz, 0.9, c.sr), lpf = Biquad::lp(p.lp > 0 ? p.lp : 20000.0, 0.7, c.sr);
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i), f = p.f0 + (p.f1 - p.f0) * std::exp(-t / p.sweep);
        ph += 2 * kPi * f / c.sr;
        const float amp = onset(t, 0.0006) * expEnv(t, p.decay);
        float s = (float) (std::sin(ph) + p.body2 * std::sin(2 * ph + 0.7)) * amp;
        s += cb.process(c.rng.white()) * expEnv(t, 0.0016) * p.click * 2.5f;
        s = sat(s, p.drive);
        c.L[(size_t) i] = p.lp > 0 ? lpf.process(s) : s;
    }
}

struct BassP { float gs, gt, decay, drive, harm, click, clickHz, lp, len, fmIdx, fmRatio; };
void bass808(Ctx& c, const BassP& p)
{
    constexpr double f0 = 34.6478;     // C#1: the sample's own pitch (MIDI note 37 plays it as-is)
    c.alloc(p.len, false);
    double ph = 0;
    Biquad cb = Biquad::bp(p.clickHz, 0.9, c.sr), lpf = Biquad::lp(p.lp > 0 ? p.lp : 20000.0, 0.7, c.sr);
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i), f = f0 * (1.0 + (p.gs - 1.0) * (p.gt > 0 ? std::exp(-t / p.gt) : 0.0));
        ph += 2 * kPi * f / c.sr;
        const double fm = p.fmIdx * std::exp(-t / 0.14) * std::sin(p.fmRatio * ph);
        double s = std::sin(ph + fm);
        if (p.harm > 0) s += p.harm * (0.55 * std::sin(2 * ph + 0.3) + 0.3 * std::sin(3 * ph + 0.9));
        float y = (float) s * onset(t, 0.0025) * expEnv(t, p.decay);
        y += cb.process(c.rng.white()) * expEnv(t, 0.002) * p.click * 1.5f;
        y = sat(y, p.drive);
        c.L[(size_t) i] = p.lp > 0 ? lpf.process(y) : y;
    }
}

struct ClapP { int bursts; float spacing, bp, q, burstTau, tailTau, tailLvl, room, rt, len; int crushBits; };
void clap(Ctx& c, const ClapP& p)
{
    c.alloc(p.len, true);
    for (int ch = 0; ch < 2; ++ch)
    {
        Rng r(7u + (uint32_t) ch * 7919u + (uint32_t) (p.bp));
        Biquad b = Biquad::bp(p.bp, p.q, c.sr), h = Biquad::hp(450.0, 0.7, c.sr);
        auto& out = ch == 0 ? c.L : c.R;
        const double lastBurst = (p.bursts - 1) * (double) p.spacing + (ch ? 0.0006 * p.bursts : 0.0);
        for (int i = 0; i < c.n; ++i)
        {
            const double t = c.t(i);
            float env = 0.f;
            for (int k = 0; k < p.bursts; ++k)
            {
                const double tk = k * (double) p.spacing + (ch ? 0.0006 * (k + 1) : 0.0);
                if (t >= tk) env += onset(t - tk, 0.0004) * expEnv(t - tk, p.burstTau) * (k == p.bursts - 1 ? 1.0f : 0.8f);
            }
            if (t >= lastBurst) env += p.tailLvl * onset(t - lastBurst, 0.002) * expEnv(t - lastBurst, p.tailTau);
            float x = h.process(b.process(r.white())) * env;
            if (p.crushBits > 0) x = crush(x, p.crushBits);
            out[(size_t) i] = sat(x * 1.6f, 1.4f);
        }
    }
    addRoom(c, p.room, p.rt);
}

struct SnareP { float tone, sweep, toneTau, toneAmt, tone2, noiseTau, noiseAmt, hp, lp, attack, drive, click, room, rt, len; };
void snare(Ctx& c, const SnareP& p)
{
    c.alloc(p.len, true);
    double ph = 0, ph2 = 0;
    Rng rl(31u), rr(32u);
    Biquad hl = Biquad::hp(p.hp, 0.7, c.sr), hr = hl, ll = Biquad::lp(p.lp, 0.7, c.sr), lr = ll;
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i), f = p.tone * (1.0 + (p.sweep - 1.0) * std::exp(-t / 0.010));
        ph += 2 * kPi * f / c.sr; ph2 += 2 * kPi * f * 2.13 / c.sr;
        const float tn = (float) (std::sin(ph) * p.toneAmt * expEnv(t, p.toneTau) + std::sin(ph2) * p.tone2 * expEnv(t, p.toneTau * 0.5)) * onset(t, 0.0003);
        const float ne = onset(t, p.attack) * expEnv(t, p.noiseTau) * p.noiseAmt;
        const float nl = ll.process(hl.process(rl.white())) * ne, nr = lr.process(hr.process(rr.white())) * ne;
        const float ck = expEnv(t, 0.0012) * p.click;
        c.L[(size_t) i] = sat(tn + nl + ck * rl.white() * 0.5f, p.drive);
        c.R[(size_t) i] = sat(tn + nr + ck * rr.white() * 0.5f, p.drive);
    }
    addRoom(c, p.room, p.rt);
}

struct HatP { float base, decay, decay2, tail2, hp, peak, peakGain, lp, noiseMix, metalMix, attack, len; int crushBits; };
void hat(Ctx& c, const HatP& p)
{
    c.alloc(p.len, true);
    static const double ratios[6] = { 1.0, 1.4471, 1.6170, 1.9265, 2.5028, 2.6637 };
    Square osc[6];
    Rng rl(51u), rr(52u);
    Biquad h1 = Biquad::hp(p.hp, 0.7, c.sr), h2 = Biquad::hp(p.hp, 0.7, c.sr), pk = Biquad::bp(p.peak, 1.4, c.sr);
    Biquad nh1 = Biquad::hp(p.hp * 0.9, 0.7, c.sr), nh2 = nh1, lpL = Biquad::lp(p.lp, 0.7, c.sr), lpR = lpL;
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i);
        float m = 0.f;
        for (int k = 0; k < 6; ++k) m += osc[k].next(p.base * ratios[k] / c.sr);
        m *= (1.f / 6.f);
        float metal = h2.process(h1.process(m));
        metal += pk.process(metal) * p.peakGain;
        const float env = onset(t, p.attack) * (expEnv(t, p.decay) + p.tail2 * expEnv(t, p.decay2));
        const float wl = nh2.process(nh1.process(rl.white())), wr = nh2.process(nh1.process(rr.white()));
        float l = (metal * p.metalMix + wl * p.noiseMix) * env, r = (metal * p.metalMix + wr * p.noiseMix) * env;
        l = lpL.process(l); r = lpR.process(r);
        if (p.crushBits > 0) { l = crush(l, p.crushBits); r = crush(r, p.crushBits); }
        c.L[(size_t) i] = l * 3.f; c.R[(size_t) i] = r * 3.f;
    }
}

struct Partial { float ratio, amp, tau; };
struct ModalP
{
    float f0, sweep, sweepTau;
    std::vector<Partial> parts;
    float noise, noiseHz, noiseQ, noiseTau;
    float drive, hp, len, detuneCents;
};
void modal(Ctx& c, const ModalP& p)
{
    const bool stereo = p.detuneCents > 0.f;
    c.alloc(p.len, stereo);
    Biquad nb = Biquad::bp(p.noiseHz, p.noiseQ, c.sr), nb2 = nb;
    Rng rl(71u), rr(72u);
    for (int ch = 0; ch < (stereo ? 2 : 1); ++ch)
    {
        auto& out = ch == 0 ? c.L : c.R;
        const double det = stereo ? std::pow(2.0, (ch == 0 ? 1.0 : -1.0) * p.detuneCents / 1200.0) : 1.0;
        std::vector<double> ph(p.parts.size(), 0.0);
        Biquad& nbq = ch == 0 ? nb : nb2;
        Rng& rg = ch == 0 ? rl : rr;
        for (int i = 0; i < c.n; ++i)
        {
            const double t = c.t(i), fm = det * p.f0 * (1.0 + (p.sweep - 1.0) * std::exp(-t / p.sweepTau));
            float s = 0.f;
            for (size_t k = 0; k < p.parts.size(); ++k)
            {
                ph[k] += 2 * kPi * fm * p.parts[k].ratio / c.sr;
                s += (float) std::sin(ph[k]) * p.parts[k].amp * expEnv(t, p.parts[k].tau);
            }
            s *= onset(t, 0.0002);
            if (p.noise > 0.f) s += nbq.process(rg.white()) * expEnv(t, p.noiseTau) * p.noise;
            out[(size_t) i] = sat(s, p.drive);
        }
    }
}

// ---------------- special percussion ----------------
void cowbell(Ctx& c, float f1, float f2, float decay, float len)
{
    c.alloc(len, false);
    Square a, b;
    Biquad bp = Biquad::bp((double) f2, 2.2, c.sr), hp = Biquad::hp(350.0, 0.7, c.sr);
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i);
        float s = 0.6f * a.next(f1 / c.sr) + 0.6f * b.next(f2 / c.sr);
        s = hp.process(bp.process(s) * 2.5f + s * 0.25f);
        const float env = onset(t, 0.0003) * (0.65f * expEnv(t, 0.012) + 0.35f * expEnv(t, decay));
        c.L[(size_t) i] = sat(s * env, 1.5f);
    }
}

void snap(Ctx& c)
{
    c.alloc(0.22, true);
    Rng rl(81u), rr(82u);
    Biquad bl = Biquad::bp(2300.0, 2.2, c.sr), br = bl, hp = Biquad::hp(600.0, 0.7, c.sr);
    double ph = 0;
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i);
        const float e = expEnv(t, 0.0025) + 0.6f * onset(t - 0.0075, 0.0004) * expEnv(std::max(0.0, t - 0.0075), 0.004) * (t >= 0.0075 ? 1.f : 0.f);
        ph += 2 * kPi * (1250.0 + 450.0 * std::exp(-t / 0.02)) / c.sr;
        const float ping = (float) std::sin(ph) * expEnv(t, 0.045) * 0.35f;
        c.L[(size_t) i] = sat(hp.process(bl.process(rl.white()) * e * 2.2f + ping), 1.6f);
        c.R[(size_t) i] = sat(hp.process(br.process(rr.white()) * e * 2.2f + ping), 1.6f);
    }
    addRoom(c, 0.06f, 0.35f);
}

void shaker(Ctx& c, float hpHz, float swell, float decay, int shakes, float spacing, float len)
{
    c.alloc(len, true);
    Rng rl(91u), rr(92u);
    Biquad hl = Biquad::hp(hpHz, 0.7, c.sr), hr = hl, bl = Biquad::bp(hpHz * 1.5, 0.6, c.sr), br = bl;
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i);
        float e = 0.f;
        for (int k = 0; k < shakes; ++k) { const double tk = k * (double) spacing, d = t - tk; if (d >= 0) e += (float) (k == 0 ? 1.0 : 0.7) * onset(d, swell) * expEnv(d, decay); }
        c.L[(size_t) i] = hl.process(bl.process(rl.white()) * 2.f) * e;
        c.R[(size_t) i] = hr.process(br.process(rr.white()) * 2.f) * e;
    }
}

void tambourine(Ctx& c)
{
    c.alloc(0.4, true);
    Rng rl(101u), rr(102u);
    Biquad bl = Biquad::bp(6500.0, 0.8, c.sr), br = bl;
    static const double jf[5] = { 5800, 7200, 8900, 10400, 12100 };
    std::vector<double> ph(5, 0.0);
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i);
        float e = 0.f;
        for (int k = 0; k < 3; ++k) { const double d = t - k * 0.014; if (d >= 0) e += onset(d, 0.0005) * expEnv(d, 0.03 + 0.03 * k); }
        float jingle = 0.f;
        for (int k = 0; k < 5; ++k) { ph[(size_t) k] += 2 * kPi * jf[k] / c.sr; jingle += (float) std::sin(ph[(size_t) k]) * 0.12f * expEnv(t, 0.09 + 0.02 * k); }
        jingle *= onset(t, 0.0004);
        c.L[(size_t) i] = sat(bl.process(rl.white()) * e * 1.6f + jingle, 1.2f);
        c.R[(size_t) i] = sat(br.process(rr.white()) * e * 1.6f + jingle, 1.2f);
    }
}

void pluck(Ctx& c, float f0, float damp, float len)    // Karplus-Strong
{
    c.alloc(len, false);
    const int dl = std::max(4, (int) std::round(c.sr / f0));
    std::vector<float> buf((size_t) dl);
    Rng r(111u);
    Biquad ex = Biquad::lp(5000.0, 0.7, c.sr);
    for (int i = 0; i < dl; ++i) buf[(size_t) i] = ex.process(r.white());
    int idx = 0; float prev = 0.f;
    for (int i = 0; i < c.n; ++i)
    {
        const float cur = buf[(size_t) idx];
        const float avg = (cur + prev) * 0.5f;
        buf[(size_t) idx] = avg * damp;
        prev = cur;
        c.L[(size_t) i] = cur * onset(c.t(i), 0.0004);
        if (++idx >= dl) idx = 0;
    }
}

void sweepTone(Ctx& c, float f0, float f1, float tau, float decay, float fmIdx, float len, float drive)
{
    c.alloc(len, false);
    double ph = 0;
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i), f = f1 + (f0 - f1) * std::exp(-t / tau);
        ph += 2 * kPi * f / c.sr;
        const double fm = fmIdx * std::exp(-t / (decay * 0.5)) * std::sin(ph * 3.5);
        c.L[(size_t) i] = sat((float) std::sin(ph + fm) * onset(t, 0.0004) * expEnv(t, decay), drive);
    }
}

void blip(Ctx& c)
{
    c.alloc(0.18, false);
    Square a; Biquad lp = Biquad::lp(7000.0, 0.7, c.sr);
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i), f = t < 0.022 ? 1568.0 : 3136.0;
        const float s = crush(a.next(f / c.sr) * 0.7f, 6);
        c.L[(size_t) i] = lp.process(s) * onset(t, 0.0003) * expEnv(t, 0.035);
    }
}

void tickStick(Ctx& c)
{
    c.alloc(0.09, false);
    Rng r(121u);
    Biquad hp = Biquad::hp(5500.0, 0.7, c.sr);
    double ph = 0;
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i);
        ph += 2 * kPi * 3100.0 / c.sr;
        c.L[(size_t) i] = sat(hp.process(r.white()) * expEnv(t, 0.0025) * 0.9f + (float) std::sin(ph) * expEnv(t, 0.0045) * 0.6f, 1.4f);
    }
}

// ---------------- FX ----------------
void riser(Ctx& c, float len, float lo, float hi, bool down, float tonal, float room, float qres)
{
    c.alloc(len, true);
    Rng rl(131u), rr(132u);
    Svf sl, sr_;
    double phA = 0;
    for (int i = 0; i < c.n; ++i)
    {
        const double u = (double) i / c.n, uu = down ? 1.0 - u : u;
        const double fc = lo * std::pow(hi / lo, uu);
        const float amp = down ? (float) std::pow(1.0 - u, 1.5) * onset(c.t(i), 0.004) : (float) std::pow(u, 2.2);
        float lpv, bpv, hpv, lpv2, bpv2, hpv2;
        sl.run(rl.white(), fc, qres, c.sr, lpv, bpv, hpv);
        sr_.run(rr.white(), fc, qres, c.sr, lpv2, bpv2, hpv2);
        float tone = 0.f;
        if (tonal > 0.f) { phA += 2 * kPi * (180.0 * std::pow(8.0, uu)) / c.sr; tone = (float) std::sin(phA) * tonal * 0.4f; }
        c.L[(size_t) i] = (bpv * 0.9f + tone) * amp * 1.4f;
        c.R[(size_t) i] = (bpv2 * 0.9f + tone) * amp * 1.4f;
    }
    addRoom(c, room, 1.2f);
}

void subDrop(Ctx& c, float f1, float f0, float tau, float decay, float len, float drive)
{
    c.alloc(len, false);
    double ph = 0;
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i), f = f0 + (f1 - f0) * std::exp(-t / tau);
        ph += 2 * kPi * f / c.sr;
        c.L[(size_t) i] = sat((float) std::sin(ph) * onset(t, 0.003) * expEnv(t, decay), drive);
    }
}

void impact(Ctx& c)
{
    c.alloc(2.8, true);
    double ph = 0; Rng r(141u); Biquad lp = Biquad::lp(2800.0, 0.7, c.sr);
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i), f = 38.0 + 62.0 * std::exp(-t / 0.10);
        ph += 2 * kPi * f / c.sr;
        const float body = (float) std::sin(ph) * onset(t, 0.002) * expEnv(t, 0.45);
        const float nz = lp.process(r.white()) * expEnv(t, 0.07) * 0.8f;
        const float s = sat(body * 1.1f + nz, 1.8f);
        c.L[(size_t) i] = s; c.R[(size_t) i] = s;
    }
    addRoom(c, 0.35f, 2.2f, 250.f, 3500.f);
}

void zap(Ctx& c)
{
    c.alloc(0.22, false);
    double ph = 0;
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i), f = 140.0 + 4800.0 * std::exp(-t / 0.028);
        ph += 2 * kPi * f / c.sr;
        const double fm = 3.0 * std::exp(-t / 0.05) * std::sin(ph * 7.0);
        c.L[(size_t) i] = sat((float) std::sin(ph + fm) * onset(t, 0.0003) * expEnv(t, 0.06), 1.3f);
    }
}

void tapeStop(Ctx& c)
{
    c.alloc(1.5, true);
    static const double base[3] = { 220.0, 277.18, 329.63 };
    Saw sw[2][3]; double det[2] = { 1.003, 0.997 };
    Biquad lpL = Biquad::lp(4000.0, 0.8, c.sr);
    double fall = 1.0;
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i);
        fall = std::exp(-std::pow(t / 0.85, 1.7) * 3.2);          // pitch factor goes to ~0
        float l = 0.f, r = 0.f;
        for (int k = 0; k < 3; ++k)
        {
            l += sw[0][k].next(clampd(base[k] * det[0] * fall, 5.0, 4000.0) / c.sr);
            r += sw[1][k].next(clampd(base[k] * det[1] * fall, 5.0, 4000.0) / c.sr);
        }
        const float env = onset(t, 0.004) * (t < 0.1 ? 1.f : expEnv(t - 0.1, 0.5));
        lpL = Biquad::lp(clampd(4200.0 * fall + 120.0, 100.0, 5000.0), 0.8, c.sr);   // filter closes with the pitch
        c.L[(size_t) i] = sat(lpL.process(l) * env * 0.3f, 1.3f);
        c.R[(size_t) i] = sat(lpL.process(r) * env * 0.3f, 1.3f);
    }
}

void crackle(Ctx& c)
{
    c.alloc(1.4, true);
    Rng rl(151u), rr(152u);
    Biquad hl = Biquad::hp(1800.0, 0.7, c.sr), hr = hl;
    float zl = 0.f, zr = 0.f;
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i);
        const double dens = (900.0 * std::exp(-t / 0.55) + 40.0) / c.sr;
        float l = 0.f, r = 0.f;
        if (rl.uni() < dens) { const float a = rl.uni(); l = (rl.white() < 0 ? -1.f : 1.f) * a * a * 1.2f; }
        if (rr.uni() < dens) { const float a = rr.uni(); r = (rr.white() < 0 ? -1.f : 1.f) * a * a * 1.2f; }
        zl = zl * 0.35f + l; zr = zr * 0.35f + r;
        c.L[(size_t) i] = hl.process(zl) + hl.process(0.f) * 0.f + rl.white() * 0.012f * expEnv(t, 0.8);
        c.R[(size_t) i] = hr.process(zr) + rr.white() * 0.012f * expEnv(t, 0.8);
    }
}

void breath(Ctx& c)
{
    c.alloc(1.3, true);
    Rng rl(161u), rr(162u);
    Svf sl, sr_;
    for (int i = 0; i < c.n; ++i)
    {
        const double u = (double) i / c.n, fc = 700.0 + 1900.0 * std::sin(kPi * u);
        const float amp = (float) std::pow(std::sin(kPi * std::pow(u, 0.8)), 2.0);
        float a, b, d, a2, b2, d2;
        sl.run(rl.white(), fc, 0.9, c.sr, a, b, d); sr_.run(rr.white(), fc * 1.04, 0.9, c.sr, a2, b2, d2);
        c.L[(size_t) i] = (b * 0.8f + a * 0.15f) * amp * 1.6f;
        c.R[(size_t) i] = (b2 * 0.8f + a2 * 0.15f) * amp * 1.6f;
    }
    addRoom(c, 0.18f, 1.3f);
}

void chirpRun(Ctx& c)
{
    c.alloc(0.85, true);
    static const double semis[8] = { 0, 3, 7, 10, 12, 15, 19, 22 };    // C# minor 7 climb
    Square sq[2]; Biquad lp = Biquad::lp(5200.0, 0.7, c.sr);
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i);
        const int k = (int) std::min(7.0, t / 0.055);
        const double tk = t - k * 0.055, f = 554.37 * std::pow(2.0, semis[k] / 12.0);
        const float env = onset(tk, 0.0006) * expEnv(tk, 0.03) * (k == 7 ? 1.f : 0.85f);
        const float a = crush(sq[0].next(f / c.sr) * 0.45f, 7), b = crush(sq[1].next(f * 1.004 / c.sr) * 0.45f, 7);
        c.L[(size_t) i] = lp.process(a) * env; c.R[(size_t) i] = lp.process(b) * env;
    }
    addRoom(c, 0.2f, 0.9f);
}

void vocalOoh(Ctx& c)
{
    c.alloc(1.7, true);
    Saw s[2][3]; static const double det[3] = { 0.995, 1.0, 1.006 };
    Svf f1[2], f2[2], f3[2];
    Rng rg(171u);
    for (int i = 0; i < c.n; ++i)
    {
        const double t = c.t(i), u = (double) i / c.n;
        const float amp = (float) std::pow(std::sin(kPi * std::pow(u, 0.7)), 1.5);
        const double vib = 1.0 + 0.004 * std::sin(2 * kPi * 5.2 * t) * std::min(1.0, t / 0.5);
        for (int ch = 0; ch < 2; ++ch)
        {
            float x = 0.f;
            for (int k = 0; k < 3; ++k) x += s[ch][k].next(233.08 * det[k] * (ch ? 1.002 : 0.998) * vib / c.sr);
            x = x / 3.f + rg.white() * 0.03f;
            float l1, b1, h1, l2, b2, h2, l3, b3, h3;
            f1[ch].run(x, 330.0, 6.0, c.sr, l1, b1, h1); f2[ch].run(x, 860.0, 8.0, c.sr, l2, b2, h2); f3[ch].run(x, 2400.0, 8.0, c.sr, l3, b3, h3);
            (ch ? c.R : c.L)[(size_t) i] = (b1 * 1.2f + b2 * 0.7f + b3 * 0.25f) * amp * 1.2f;
        }
    }
    addRoom(c, 0.22f, 1.6f);
}

void glassPing(Ctx& c)
{
    ModalP m { 2093.f, 1.f, 0.01f, { { 1.f, 0.7f, 1.2f }, { 2.76f, 0.45f, 0.9f }, { 5.4f, 0.3f, 0.5f }, { 8.93f, 0.18f, 0.25f }, { 13.3f, 0.08f, 0.12f } },
               0.f, 0.f, 1.f, 0.f, 0.f, 500.f, 1.8f, 6.f };
    modal(c, m);
    addRoom(c, 0.25f, 1.8f);
}

// ------------------------------------------------------------------------------------------------- finishing / loudness
struct Spec { float hp, target, ceil; bool mono; };
Spec specFor(Cat c)
{
    switch (c)
    {
        case Cat::Kick:    return { 28.f, -10.0f,  -1.5f, true };
        case Cat::Bass808: return { 20.f, -12.5f,  -1.5f, true };
        case Cat::Clap:    return { 220.f, -15.5f, -1.5f, false };
        case Cat::Snare:   return { 80.f, -14.5f, -1.5f, false };
        case Cat::Hat:     return { 500.f, -24.5f, -3.0f, false };
        case Cat::Perc:    return { 70.f, -20.5f, -3.0f, false };
        case Cat::Fx:      break;
    }
    return { 25.f, -22.0f, -3.0f, false };      // Fx
}

Biquad kShelf(double sr)
{
    const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
    const double K = std::tan(kPi * f0 / sr), Vh = std::pow(10.0, G / 20.0), Vb = std::pow(Vh, 0.4996667741545416);
    const double a0 = 1.0 + K / Q + K * K;
    Biquad b; b.b0 = (Vh + Vb * K / Q + K * K) / a0; b.b1 = 2.0 * (K * K - Vh) / a0; b.b2 = (Vh - Vb * K / Q + K * K) / a0;
    b.a1 = 2.0 * (K * K - 1.0) / a0; b.a2 = (1.0 - K / Q + K * K) / a0; return b;
}
Biquad kHigh(double sr)
{
    const double f0 = 38.13547087602444, Q = 0.5003270373238773, K = std::tan(kPi * f0 / sr), a0 = 1.0 + K / Q + K * K;
    Biquad b; b.b0 = 1.0; b.b1 = -2.0; b.b2 = 1.0; b.a1 = 2.0 * (K * K - 1.0) / a0; b.a2 = (1.0 - K / Q + K * K) / a0; return b;
}

double loudnessDb(const Sample& s)    // loudest 80 ms, K-weighted, stereo-equivalent (a mono sound counts on both channels)
{
    const int n = s.length();
    if (n == 0) return -120.0;
    Biquad shL = kShelf(s.sr), hiL = kHigh(s.sr), shR = shL, hiR = hiL;
    std::vector<double> e((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const float l = hiL.process(shL.process(s.l[(size_t) i]));
        float r = l;
        if (!s.isMono()) r = hiR.process(shR.process(s.r[(size_t) i]));
        e[(size_t) i] = (double) l * l + (double) r * r;
    }
    const int w = std::max(8, (int) (0.08 * s.sr)), hop = std::max(1, (int) (0.005 * s.sr));
    double best = 0.0, acc = 0.0;
    for (int i = 0; i < std::min(w, n); ++i) acc += e[(size_t) i];
    best = acc / w;
    for (int start = hop; start + 1 < n; start += hop)
    {
        acc = 0.0;
        for (int i = start; i < std::min(n, start + w); ++i) acc += e[(size_t) i];
        best = std::max(best, acc / w);
    }
    return best > 1e-14 ? 10.0 * std::log10(best) : -120.0;
}

float peakOf(const Sample& s)
{
    float pk = 0.f;
    for (float v : s.l) pk = std::max(pk, std::abs(v));
    for (float v : s.r) pk = std::max(pk, std::abs(v));
    return pk;
}

Sample finish(Ctx& c, const SoundInfo& info)
{
    const Spec sp = specFor(info.cat);
    const int n = c.n;
    const bool stereoIn = c.st && !c.R.empty();
    // 1) mono bass / sides
    if (sp.mono && stereoIn) { for (int i = 0; i < n; ++i) c.L[(size_t) i] = 0.5f * (c.L[(size_t) i] + c.R[(size_t) i]); c.R.clear(); }
    // 2) high-pass (2nd order, twice for the bass-free categories) and DC removal
    {
        Biquad h1 = Biquad::hp(sp.hp, 0.707, c.sr), h2 = h1;
        Biquad r1 = h1, r2 = h1;
        const bool st = !c.R.empty();
        for (int i = 0; i < n; ++i)
        {
            float l = h1.process(c.L[(size_t) i]);
            if (info.cat != Cat::Kick && info.cat != Cat::Bass808) l = h2.process(l);
            c.L[(size_t) i] = l;
            if (st) { float r = r1.process(c.R[(size_t) i]); if (info.cat != Cat::Kick && info.cat != Cat::Bass808) r = r2.process(r); c.R[(size_t) i] = r; }
        }
    }
    // 3) mono-compatible stereo: sides are high-passed so the low/mid body stays centred
    if (!c.R.empty())
    {
        const double a = 1.0 - std::exp(-2.0 * kPi * 250.0 / c.sr);
        double lpS = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (c.L[(size_t) i] + c.R[(size_t) i]), sraw = 0.5f * (c.L[(size_t) i] - c.R[(size_t) i]);
            lpS += a * (sraw - lpS);
            const float s = sraw - (float) lpS;
            c.L[(size_t) i] = m + s; c.R[(size_t) i] = m - s;
        }
    }
    // 4) trim the tail at -66 dB, fade in / out
    float pk = 0.f;
    for (int i = 0; i < n; ++i) { pk = std::max(pk, std::abs(c.L[(size_t) i])); if (!c.R.empty()) pk = std::max(pk, std::abs(c.R[(size_t) i])); }
    int last = n - 1;
    const float thr = pk * 0.0005f;
    while (last > 8 && std::abs(c.L[(size_t) last]) < thr && (c.R.empty() || std::abs(c.R[(size_t) last]) < thr)) --last;
    int len = std::min(n, last + 1 + (int) (0.01 * c.sr));
    const int fadeOut = std::max(16, std::min((int) (0.04 * c.sr), len / 4));
    const int fadeIn = std::max(2, (int) (0.0002 * c.sr));
    Sample out; out.sr = c.sr;
    out.l.assign(c.L.begin(), c.L.begin() + len);
    if (!c.R.empty()) out.r.assign(c.R.begin(), c.R.begin() + len);
    for (int i = 0; i < len; ++i)
    {
        float g = 1.f;
        if (i < fadeIn) g *= 0.5f - 0.5f * std::cos((float) kPi * (float) i / (float) fadeIn);
        const int d = len - 1 - i;
        if (d < fadeOut) g *= 0.5f - 0.5f * std::cos((float) kPi * (float) d / (float) fadeOut);
        out.l[(size_t) i] *= g; if (!out.r.empty()) out.r[(size_t) i] *= g;
    }
    // 5) level: loudness target of the category (+ per-sound trim) but never above the peak ceiling
    const double M = loudnessDb(out);
    const float pkNow = std::max(1e-9f, peakOf(out));
    const float gLoud = db2g(sp.target + info.levelDb - M);
    const float gPeak = db2g(sp.ceil + info.levelDb * 0.f) / pkNow;
    const float g = std::min(gLoud, gPeak * 1.0f);
    const float ceilLin = db2g(sp.ceil);
    for (auto* ch : { &out.l, &out.r })
        for (auto& v : *ch)
        {
            float x = v * g;
            const float a = std::abs(x) / ceilLin;
            if (a > 0.8f) x = (x < 0 ? -1.f : 1.f) * ceilLin * (0.8f + 0.2f * std::tanh((a - 0.8f) / 0.2f));
            v = x;
        }
    return out;
}

// ------------------------------------------------------------------------------------------------- catalog
struct Entry { SoundInfo info; std::function<void(Ctx&)> fn; };

void addKick(std::vector<Entry>& v, const char* name, const char* tag, KickP p, float lvl = 0.f)
{ v.push_back({ { name, Cat::Kick, tag, -1, false, 0, lvl }, [p](Ctx& c) { kick(c, p); } }); }
void add808(std::vector<Entry>& v, const char* name, const char* tag, BassP p, float lvl = 0.f)
{ v.push_back({ { name, Cat::Bass808, tag, 37, true, 0, lvl }, [p](Ctx& c) { bass808(c, p); } }); }
void addClap(std::vector<Entry>& v, const char* name, const char* tag, ClapP p, float lvl = 0.f)
{ v.push_back({ { name, Cat::Clap, tag, -1, false, 0, lvl }, [p](Ctx& c) { clap(c, p); } }); }
void addSnare(std::vector<Entry>& v, const char* name, const char* tag, SnareP p, float lvl = 0.f)
{ v.push_back({ { name, Cat::Snare, tag, -1, false, 0, lvl }, [p](Ctx& c) { snare(c, p); } }); }
void addHat(std::vector<Entry>& v, const char* name, const char* tag, HatP p, float lvl = 0.f, int choke = 1)
{ v.push_back({ { name, Cat::Hat, tag, -1, false, choke, lvl }, [p](Ctx& c) { hat(c, p); } }); }
void addPerc(std::vector<Entry>& v, const char* name, const char* tag, int root, std::function<void(Ctx&)> fn, float lvl = 0.f)
{ v.push_back({ { name, Cat::Perc, tag, root, false, 0, lvl }, std::move(fn) }); }
void addFx(std::vector<Entry>& v, const char* name, const char* tag, std::function<void(Ctx&)> fn, float lvl = 0.f, int root = -1)
{ v.push_back({ { name, Cat::Fx, tag, root, false, 0, lvl }, std::move(fn) }); }
auto modalFn(ModalP p) { return [p](Ctx& c) { modal(c, p); }; }

const std::vector<Entry>& entries()
{
    static const std::vector<Entry> all = [] {
        std::vector<Entry> v;
        // ============================================ KICKS ===========================================
        //          name              tag                                f0   f1    sweep  decay body2 click clkHz drive len  lp
        addKick(v, "Plan A Kick",    "tight, bouncy, clean punch",    { 50, 190, 0.032f, 0.24f, 0.12f, 0.35f, 2800, 1.6f, 0.55f, 0 });
        addKick(v, "Soft Thump",     "round and warm, no click",      { 46, 120, 0.045f, 0.34f, 0.05f, 0.10f, 1500, 1.2f, 0.70f, 0 });
        addKick(v, "Round Punch",    "full body, slightly driven",    { 55, 160, 0.028f, 0.22f, 0.20f, 0.20f, 2200, 2.2f, 0.50f, 0 });
        addKick(v, "Click Knock",    "hard beater click, short body", { 58, 240, 0.020f, 0.16f, 0.10f, 0.70f, 3500, 1.8f, 0.35f, 0 });
        addKick(v, "Dark Sub Kick",  "deep and dark, long sub",       { 41, 100, 0.060f, 0.50f, 0.00f, 0.05f,  900, 1.1f, 0.95f, 900 });
        addKick(v, "Short Dry",      "tiny and dry for fast rolls",   { 62, 200, 0.018f, 0.10f, 0.15f, 0.30f, 3000, 1.5f, 0.25f, 0 });
        addKick(v, "Boom Kick",      "big, saturated, long tail",     { 44, 150, 0.050f, 0.45f, 0.15f, 0.25f, 2000, 2.8f, 0.90f, 0 });
        addKick(v, "Air Kick",       "soft with a breath of air",     { 52, 140, 0.035f, 0.28f, 0.10f, 0.20f, 6000, 1.3f, 0.50f, 7000 });

        // ============================================ 808 =============================================
        //         name                 tag                             gs   gt     decay drive harm  click clkHz lp   len  fmIdx fmRat
        add808(v, "Clean Sub 808",   "pure sub, long and smooth",      { 1.0f, 0.00f, 1.30f, 1.0f, 0.00f, 0.00f, 1200,    0, 3.2f, 0.f, 0.f });
        add808(v, "Warm 808",        "soft saturation, rounded",       { 1.25f, 0.025f, 0.95f, 3.4f, 0.30f, 0.10f, 1200,    0, 2.6f, 0.f, 0.f });
        add808(v, "Slide 808",       "pitch dives in at the start",    { 1.7f, 0.07f, 1.20f, 1.8f, 0.10f, 0.05f, 1000,    0, 3.0f, 0.f, 0.f });
        add808(v, "Punchy 808",      "short and tight",                { 1.0f, 0.00f, 0.50f, 2.2f, 0.10f, 0.20f, 1500,    0, 1.6f, 0.f, 0.f });
        add808(v, "Long Boom 808",   "very long, rolling sub",         { 1.0f, 0.00f, 2.20f, 1.5f, 0.05f, 0.00f, 1000,    0, 4.2f, 0.f, 0.f });
        add808(v, "Dirty 808",       "gritty, driven, filtered",       { 1.0f, 0.00f, 1.10f, 6.0f, 0.15f, 0.15f, 1400, 1500, 2.8f, 0.f, 0.f });
        add808(v, "Bell 808",        "sub with a glassy bell tone",    { 1.0f, 0.00f, 0.90f, 1.4f, 0.00f, 0.00f, 1000,    0, 2.4f, 1.6f, 4.f });
        add808(v, "Hollow 808",      "audible on small speakers",      { 1.0f, 0.00f, 1.00f, 2.0f, 0.75f, 0.05f, 1000,    0, 2.6f, 0.f, 0.f });

        // ============================================ CLAPS ===========================================
        //        name              tag                                  n  space  bp    q     burstT  tailT  tailL room  rt    len  crush
        addClap(v, "Plan A Clap",   "crisp, medium room",             { 3, 0.0105f, 1500, 0.9f, 0.004f, 0.11f, 0.55f, 0.18f, 0.5f, 0.55f, 0 });
        addClap(v, "Tight Clap",    "short and dry",                  { 2, 0.0080f, 1800, 1.0f, 0.003f, 0.06f, 0.50f, 0.08f, 0.3f, 0.32f, 0 });
        addClap(v, "Wide Clap",     "wide, airy tail",                { 4, 0.0120f, 1300, 0.8f, 0.005f, 0.16f, 0.60f, 0.28f, 0.9f, 0.95f, 0 });
        addClap(v, "Snappy Clap",   "bright and snappy",              { 3, 0.0070f, 2400, 1.2f, 0.003f, 0.07f, 0.40f, 0.10f, 0.35f, 0.38f, 0 });
        addClap(v, "Soft Clap",     "soft, rounded, laid back",       { 3, 0.0130f, 1100, 0.7f, 0.006f, 0.12f, 0.50f, 0.20f, 0.5f, 0.55f, 0 });
        addClap(v, "Ghost Clap",    "tiny ghost note clap",           { 2, 0.0060f, 1900, 1.0f, 0.003f, 0.03f, 0.30f, 0.05f, 0.2f, 0.20f, 0 }, -5.f);
        addClap(v, "Reverb Clap",   "big space, long wash",           { 3, 0.0100f, 1600, 0.9f, 0.004f, 0.12f, 0.50f, 0.50f, 1.6f, 1.90f, 0 });
        addClap(v, "Lo-Fi Clap",    "crushed, dusty",                 { 4, 0.0090f, 1400, 1.4f, 0.004f, 0.09f, 0.45f, 0.10f, 0.4f, 0.45f, 6 });

        // ============================================ SNARES ==========================================
        //         name              tag                               tone swp  toneT  toneA tone2 noiseT nAmt  hp    lp     att    drv   click room  rt   len
        addSnare(v, "Plan A Snare",  "clean crack with a short room", { 195, 1.6f, 0.045f, 0.70f, 0.20f, 0.14f, 0.80f, 1800, 9000, 0.0010f, 1.5f, 0.30f, 0.12f, 0.5f, 0.50f });
        addSnare(v, "Crisp Snare",   "tight and bright",              { 220, 1.5f, 0.030f, 0.60f, 0.15f, 0.10f, 0.85f, 2500, 12000, 0.0008f, 1.2f, 0.25f, 0.06f, 0.3f, 0.32f });
        addSnare(v, "Rim Snap",      "short rimshot-style crack",     { 330, 1.3f, 0.020f, 0.60f, 0.40f, 0.07f, 0.70f, 3000, 11000, 0.0005f, 1.7f, 0.80f, 0.05f, 0.3f, 0.22f });
        addSnare(v, "Long Room Snare","big room, long decay",         { 180, 1.5f, 0.060f, 0.70f, 0.15f, 0.20f, 0.80f, 1500, 9500, 0.0010f, 1.4f, 0.20f, 0.40f, 1.4f, 1.60f });
        addSnare(v, "Soft Snare",    "soft and warm, laid back",      { 175, 1.4f, 0.050f, 0.80f, 0.10f, 0.12f, 0.60f, 1200, 6500, 0.0015f, 1.1f, 0.10f, 0.15f, 0.5f, 0.45f });
        addSnare(v, "Tom Snare",     "deep tom body with noise",      { 150, 1.5f, 0.090f, 1.00f, 0.20f, 0.08f, 0.45f,  900, 8000, 0.0010f, 1.3f, 0.15f, 0.12f, 0.4f, 0.45f });
        addSnare(v, "Dry Tick Snare","minimal, very dry",             { 240, 1.2f, 0.012f, 0.25f, 0.00f, 0.05f, 0.90f, 2000, 11000, 0.0005f, 1.2f, 0.35f, 0.00f, 0.0f, 0.18f });

        // ============================================ HATS ============================================
        //         name              tag                            base  decay  dec2  tail2 hp    peak   pkG  lp     noise metal att    len  crush
        addHat(v, "Plan A Hat",      "crisp, light, bouncy",     { 205, 0.028f, 0.08f, 0.15f, 7500, 10000, 0.6f, 16000, 0.50f, 1.0f, 0.0004f, 0.14f, 0 });
        addHat(v, "Tight Hat",       "very short, clean tick",   { 205, 0.016f, 0.05f, 0.10f, 8500, 11000, 0.5f, 16000, 0.40f, 1.0f, 0.0003f, 0.09f, 0 });
        addHat(v, "Soft Hat",        "soft, smooth top end",     { 205, 0.040f, 0.10f, 0.10f, 6000, 8500, 0.4f, 12000, 0.70f, 0.7f, 0.0008f, 0.18f, 0 });
        addHat(v, "Sizzle Hat",      "bright with a short sizzle",{ 205, 0.060f, 0.14f, 0.30f, 9000, 12000, 0.7f, 17000, 0.60f, 1.0f, 0.0004f, 0.28f, 0 });
        addHat(v, "Tick Hat",        "tiny tick for fast rolls", { 205, 0.008f, 0.03f, 0.05f, 9500, 12000, 0.4f, 17000, 0.50f, 1.0f, 0.0002f, 0.05f, 0 });
        addHat(v, "Lo-Fi Hat",       "dusty and crushed",        { 205, 0.030f, 0.08f, 0.12f, 5000, 7000, 0.3f, 9000, 0.60f, 0.8f, 0.0006f, 0.15f, 5 });
        addHat(v, "Plan A Open Hat", "open, bright, medium tail",{ 205, 0.160f, 0.30f, 0.30f, 7000, 10000, 0.6f, 16000, 0.50f, 1.0f, 0.0005f, 0.65f, 0 }, 0.f);
        addHat(v, "Short Open",      "short open hat",           { 205, 0.090f, 0.18f, 0.25f, 7500, 10500, 0.5f, 16000, 0.50f, 1.0f, 0.0005f, 0.38f, 0 });
        addHat(v, "Wash Open Hat",   "long airy wash",           { 205, 0.320f, 0.60f, 0.35f, 6500, 9500, 0.5f, 15000, 0.60f, 0.9f, 0.0010f, 1.10f, 0 });
        v.push_back({ { "Shaker Tick", Cat::Hat, "soft shaker, no choke", -1, false, 0, 0.f }, [](Ctx& c) { shaker(c, 5500.f, 0.010f, 0.06f, 1, 0.f, 0.22f); } });

        // ============================================ PERCS ===========================================
        addPerc(v, "Rim Click",   "sharp dry rim",           -1, modalFn({ 1750, 1.f, 0.01f, { { 1, 0.8f, 0.006f }, { 2.3f, 0.5f, 0.004f }, { 4.1f, 0.3f, 0.003f } }, 0.6f, 4000, 1.f, 0.002f, 1.4f, 400, 0.12f, 0.f }));
        addPerc(v, "Wood Block",  "hollow wood tok",         -1, modalFn({ 900, 1.f, 0.01f, { { 1, 0.9f, 0.035f }, { 2.76f, 0.4f, 0.014f }, { 5.4f, 0.2f, 0.007f } }, 0.2f, 3000, 1.f, 0.003f, 1.2f, 300, 0.2f, 0.f }));
        addPerc(v, "Clave",       "bright wooden clave",     -1, modalFn({ 2400, 1.f, 0.01f, { { 1, 0.9f, 0.055f }, { 2.1f, 0.3f, 0.03f } }, 0.1f, 5000, 1.f, 0.002f, 1.2f, 500, 0.2f, 0.f }));
        addPerc(v, "Soft Tom",    "round tom, pitchable",    55, modalFn({ 180, 1.8f, 0.020f, { { 1, 0.9f, 0.20f }, { 1.59f, 0.25f, 0.10f }, { 2.14f, 0.12f, 0.06f } }, 0.2f, 1500, 1.f, 0.01f, 1.5f, 60, 0.5f, 0.f }));
        addPerc(v, "Low Conga",   "warm low conga",          50, modalFn({ 140, 1.5f, 0.015f, { { 1, 0.9f, 0.18f }, { 2.4f, 0.3f, 0.06f } }, 0.3f, 900, 1.f, 0.01f, 1.3f, 70, 0.4f, 0.f }));
        addPerc(v, "High Conga",  "tight high conga",        64, modalFn({ 320, 1.4f, 0.012f, { { 1, 0.9f, 0.14f }, { 2.4f, 0.3f, 0.05f } }, 0.3f, 1800, 1.f, 0.008f, 1.3f, 100, 0.35f, 0.f }));
        addPerc(v, "Cowbell",     "small clean cowbell",     -1, [](Ctx& c) { cowbell(c, 540.f, 800.f, 0.16f, 0.45f); });
        addPerc(v, "Finger Snap", "dry finger snap",         -1, [](Ctx& c) { snap(c); });
        addPerc(v, "Pluck Hit",   "plucked string, pitchable", 60, [](Ctx& c) { pluck(c, 261.63f, 0.995f, 1.2f); });
        addPerc(v, "Glass Bell",  "soft glassy bell",        84, modalFn({ 1046.5f, 1.f, 0.01f, { { 1, 0.7f, 1.2f }, { 2.76f, 0.4f, 0.8f }, { 5.4f, 0.25f, 0.4f }, { 8.93f, 0.12f, 0.2f } }, 0.f, 0.f, 1.f, 0.f, 0.f, 300, 1.8f, 5.f }));
        addPerc(v, "Marimba Tap", "mellow wooden tone",      72, modalFn({ 523.25f, 1.f, 0.01f, { { 1, 0.9f, 0.40f }, { 4.0f, 0.25f, 0.09f }, { 10.0f, 0.08f, 0.03f } }, 0.f, 0.f, 1.f, 0.f, 0.f, 120, 0.9f, 0.f }));
        addPerc(v, "Kalimba Pluck","thumb piano pluck",      79, modalFn({ 784.f, 1.f, 0.01f, { { 1, 0.8f, 0.55f }, { 5.4f, 0.25f, 0.20f }, { 13.3f, 0.07f, 0.06f } }, 0.1f, 4000, 1.f, 0.004f, 0.f, 200, 1.0f, 3.f }));
        addPerc(v, "Bubble Pop",  "quick rising bloop",      -1, [](Ctx& c) { sweepTone(c, 350.f, 1300.f, 0.03f, 0.05f, 0.f, 0.20f, 1.2f); });
        addPerc(v, "Droplet",     "falling water drop",      -1, [](Ctx& c) { sweepTone(c, 1200.f, 300.f, 0.04f, 0.08f, 0.6f, 0.30f, 1.2f); });
        addPerc(v, "Tick Stick",  "tiny stick tick",         -1, [](Ctx& c) { tickStick(c); });
        addPerc(v, "Digital Blip","8-bit style blip",        -1, [](Ctx& c) { blip(c); });
        addPerc(v, "Tambourine",  "light shaken tambourine", -1, [](Ctx& c) { tambourine(c); });
        addPerc(v, "Deep Rim",    "low rim shot",            -1, modalFn({ 420, 1.f, 0.01f, { { 1, 0.9f, 0.05f }, { 2.4f, 0.4f, 0.02f }, { 3.9f, 0.2f, 0.01f } }, 0.5f, 2500, 1.f, 0.004f, 1.5f, 120, 0.3f, 0.f }));

        // ============================================ FX / TEXTURES ===================================
        addFx(v, "Reverse Swell", "noise swell into the beat",   [](Ctx& c) { riser(c, 1.3f, 300.f, 9000.f, false, 0.f, 0.1f, 1.2f); });
        addFx(v, "Long Riser",    "tonal + noise riser",         [](Ctx& c) { riser(c, 2.6f, 250.f, 11000.f, false, 0.7f, 0.15f, 1.6f); });
        addFx(v, "Down Sweep",    "falling noise sweep",         [](Ctx& c) { riser(c, 1.5f, 400.f, 9000.f, true, 0.f, 0.15f, 1.2f); });
        addFx(v, "Sub Drop",      "sub drop for transitions",    [](Ctx& c) { subDrop(c, 78.f, 28.f, 0.28f, 0.55f, 1.5f, 1.4f); });
        addFx(v, "Impact Hit",    "cinematic boom with room",    [](Ctx& c) { impact(c); });
        addFx(v, "Laser Zap",     "short sci-fi zap",            [](Ctx& c) { zap(c); });
        addFx(v, "Tape Stop",     "chord slowing to a stop",     [](Ctx& c) { tapeStop(c); });
        addFx(v, "Vinyl Crackle", "dusty crackle burst",         [](Ctx& c) { crackle(c); });
        addFx(v, "Air Breath",    "soft airy breath",            [](Ctx& c) { breath(c); });
        addFx(v, "Glass Ping",    "high shimmering ping",        [](Ctx& c) { glassPing(c); });
        addFx(v, "Chirp Run",     "rising digital arpeggio",     [](Ctx& c) { chirpRun(c); });
        addFx(v, "Ooh Swell",     "airy vocal-like swell",       [](Ctx& c) { vocalOoh(c); });
        addFx(v, "Reverse Snare", "short noise reverse",         [](Ctx& c) { riser(c, 0.65f, 600.f, 6000.f, false, 0.f, 0.05f, 1.4f); });
        return v;
    }();
    return all;
}
} // namespace

// ------------------------------------------------------------------------------------------------- public API
const char* catName(Cat c)
{
    static const char* n[kNumCats] = { "KICKS", "808s", "CLAPS", "SNARES", "HI-HATS", "PERCS", "FX & TEXTURES" };
    return n[(int) c];
}
const char* catSingular(Cat c)
{
    static const char* n[kNumCats] = { "Kick", "808", "Clap", "Snare", "Hi-hat", "Perc", "FX" };
    return n[(int) c];
}

int numSounds() { return (int) entries().size(); }
const SoundInfo& soundInfo(int i) { return entries()[(size_t) std::max(0, std::min(i, numSounds() - 1))].info; }
int indexOfSound(const std::string& name)
{
    for (int i = 0; i < numSounds(); ++i) if (entries()[(size_t) i].info.name == name) return i;
    return -1;
}

namespace
{
constexpr double kNativeRate = 48000.0;

// windowed-sinc sample-rate conversion (offline, used once per sound when the host does not run at the native rate)
std::vector<float> resampleSinc(const std::vector<float>& in, double srIn, double srOut)
{
    const double ratio = srOut / srIn;
    const int outN = std::max(32, (int) std::lround((double) in.size() * ratio));
    constexpr int H = 24, taps = 2 * H, phases = 2048;
    const double fc = 0.5 * std::min(1.0, ratio) * 0.96;               // cycles per input sample
    static thread_local std::vector<float> tableCache; static thread_local double tableFc = -1.0;
    if (std::abs(tableFc - fc) > 1e-12)
    {
        tableCache.assign((size_t) ((phases + 1) * taps), 0.f);
        for (int p = 0; p <= phases; ++p)
        {
            const double frac = (double) p / phases; double sum = 0.0;
            for (int k = 0; k < taps; ++k)
            {
                const double x = (double) (k - H + 1) - frac;           // distance of input sample (floor+k-H+1) from the output position
                const double sx = 2.0 * fc * x, sinc = std::abs(sx) < 1e-9 ? 1.0 : std::sin(kPi * sx) / (kPi * sx);
                const double u = x / (H + 0.5), w = std::abs(u) >= 1.0 ? 0.0 : 0.35875 + 0.48829 * std::cos(kPi * u) + 0.14128 * std::cos(2 * kPi * u) + 0.01168 * std::cos(3 * kPi * u);
                const double h = 2.0 * fc * sinc * w; sum += h; tableCache[(size_t) (p * taps + k)] = (float) h;
            }
            for (int k = 0; k < taps; ++k) tableCache[(size_t) (p * taps + k)] = (float) (tableCache[(size_t) (p * taps + k)] / sum);   // unity DC gain at every phase
        }
        tableFc = fc;
    }
    std::vector<float> out((size_t) outN, 0.f);
    const int n = (int) in.size();
    for (int i = 0; i < outN; ++i)
    {
        const double pos = (double) i / ratio; const int base = (int) std::floor(pos);
        const double fp = (pos - base) * phases; const int p0 = (int) fp; const float fr = (float) (fp - p0);
        const float* h0 = &tableCache[(size_t) (p0 * taps)]; const float* h1 = h0 + taps;
        float acc = 0.f;
        for (int k = 0; k < taps; ++k)
        {
            const int idx = base + k - H + 1;
            if (idx < 0 || idx >= n) continue;
            acc += in[(size_t) idx] * (h0[k] + (h1[k] - h0[k]) * fr);
        }
        out[(size_t) i] = acc;
    }
    return out;
}
} // namespace

std::shared_ptr<const Sample> renderSound(int index, double sr)
{
    const auto& e = entries()[(size_t) std::max(0, std::min(index, numSounds() - 1))];
    // every sound is synthesised at one fixed native rate, so it is note-for-note identical at any host rate
    Ctx c; c.sr = kNativeRate; c.rng = Rng((uint32_t) index + 1u);
    e.fn(c);
    Sample native = finish(c, e.info);
    if (std::abs(sr - kNativeRate) < 0.5) return std::make_shared<const Sample>(std::move(native));

    Sample out; out.sr = sr;
    out.l = resampleSinc(native.l, kNativeRate, sr);
    if (!native.isMono()) out.r = resampleSinc(native.r, kNativeRate, sr);
    // the converter can overshoot a little: keep the category's peak ceiling
    float pk = peakOf(out); const float ceilLin = db2g(specFor(e.info.cat).ceil);
    if (pk > ceilLin) { const float g = ceilLin / pk; for (auto& v : out.l) v *= g; for (auto& v : out.r) v *= g; }
    return std::make_shared<const Sample>(std::move(out));
}

namespace
{
void fft(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2 * kPi / (double) len;
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w(1);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl;
            }
        }
    }
}
} // namespace

Measure measure(const Sample& s)
{
    Measure m;
    const float pk = peakOf(s);
    m.peakDb = pk > 0 ? 20.0 * std::log10((double) pk) : -120.0;
    m.loudnessDb = loudnessDb(s);
    int last = s.length() - 1;
    const float thr = pk * 0.001f;     // -60 dB
    while (last > 0 && std::abs(s.l[(size_t) last]) < thr && (s.isMono() || std::abs(s.r[(size_t) last]) < thr)) --last;
    m.lengthSec = (double) (last + 1) / s.sr;
    double mean = 0.0;
    for (float v : s.l) mean += v;
    m.dcOffset = s.length() ? mean / s.length() : 0.0;
    // spectral centroid of the first 150 ms (mono mix, Hann window), ignoring content below 60 Hz
    const int N = 16384;
    std::vector<std::complex<double>> a((size_t) N);
    const int take = std::min(N, std::min(s.length(), (int) (0.15 * s.sr)));
    for (int i = 0; i < take; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos(2 * kPi * i / std::max(1, take - 1));
        const double x = s.isMono() ? s.l[(size_t) i] : 0.5 * (s.l[(size_t) i] + s.r[(size_t) i]);
        a[(size_t) i] = x * w;
    }
    fft(a);
    double num = 0, den = 0;
    for (int k = 1; k < N / 2; ++k)
    {
        const double f = (double) k * s.sr / N;
        if (f < 60.0) continue;
        if (f > 20000.0) break;     // audible band only, so the number does not depend on the sample rate
        const double p = std::norm(a[(size_t) k]);
        num += p * f; den += p;
    }
    m.centroidHz = den > 0 ? num / den : 0.0;
    return m;
}

// ------------------------------------------------------------------------------------------------- kits
const std::vector<Kit>& kits()
{
    static const std::vector<Kit> k = {
        { "Plan A Kit", { {36, "Plan A Kick"}, {35, "Short Dry"}, {37, "Rim Click"}, {38, "Plan A Snare"}, {39, "Plan A Clap"}, {40, "Ghost Clap"}, {41, "Low Conga"},
                          {42, "Plan A Hat"}, {43, "High Conga"}, {44, "Tight Hat"}, {45, "Soft Tom"}, {46, "Plan A Open Hat"}, {47, "Finger Snap"}, {49, "Reverse Swell"},
                          {51, "Glass Ping"}, {54, "Shaker Tick"}, {56, "Cowbell"}, {70, "Chirp Run"} } },
        { "Dark Thoughts Kit", { {36, "Dark Sub Kick"}, {35, "Soft Thump"}, {37, "Wood Block"}, {38, "Soft Snare"}, {39, "Soft Clap"}, {40, "Ghost Clap"}, {41, "Soft Tom"},
                                 {42, "Soft Hat"}, {43, "Low Conga"}, {44, "Tick Hat"}, {45, "Deep Rim"}, {46, "Wash Open Hat"}, {47, "Droplet"}, {49, "Sub Drop"},
                                 {51, "Air Breath"}, {54, "Shaker Tick"}, {56, "Marimba Tap"}, {70, "Ooh Swell"} } },
        { "Hard Bounce Kit", { {36, "Click Knock"}, {35, "Round Punch"}, {37, "Clave"}, {38, "Crisp Snare"}, {39, "Snappy Clap"}, {40, "Ghost Clap"}, {41, "High Conga"},
                               {42, "Tight Hat"}, {43, "Cowbell"}, {44, "Tick Hat"}, {45, "Tick Stick"}, {46, "Short Open"}, {47, "Digital Blip"}, {49, "Laser Zap"},
                               {51, "Impact Hit"}, {54, "Tambourine"}, {56, "Rim Click"}, {70, "Chirp Run"} } },
        { "Airy Melodic Kit", { {36, "Air Kick"}, {35, "Soft Thump"}, {37, "Kalimba Pluck"}, {38, "Soft Snare"}, {39, "Wide Clap"}, {40, "Ghost Clap"}, {41, "Marimba Tap"},
                                {42, "Sizzle Hat"}, {43, "Glass Bell"}, {44, "Soft Hat"}, {45, "Pluck Hit"}, {46, "Wash Open Hat"}, {47, "Bubble Pop"}, {49, "Reverse Swell"},
                                {51, "Glass Ping"}, {54, "Tambourine"}, {56, "Wood Block"}, {70, "Ooh Swell"} } },
        { "Lo-Fi Kit", { {36, "Short Dry"}, {35, "Soft Thump"}, {37, "Tick Stick"}, {38, "Dry Tick Snare"}, {39, "Lo-Fi Clap"}, {40, "Ghost Clap"}, {41, "Low Conga"},
                         {42, "Lo-Fi Hat"}, {43, "Wood Block"}, {44, "Tick Hat"}, {45, "Finger Snap"}, {46, "Short Open"}, {47, "Digital Blip"}, {49, "Tape Stop"},
                         {51, "Vinyl Crackle"}, {54, "Shaker Tick"}, {56, "Clave"}, {70, "Chirp Run"} } },
        { "Boom Kit", { {36, "Boom Kick"}, {35, "Dark Sub Kick"}, {37, "Deep Rim"}, {38, "Long Room Snare"}, {39, "Reverb Clap"}, {40, "Ghost Clap"}, {41, "Soft Tom"},
                        {42, "Sizzle Hat"}, {43, "Low Conga"}, {44, "Tight Hat"}, {45, "Tom Snare"}, {46, "Wash Open Hat"}, {47, "Droplet"}, {49, "Impact Hit"},
                        {51, "Down Sweep"}, {54, "Tambourine"}, {56, "Cowbell"}, {70, "Long Riser"} } },
    };
    return k;
}

int kitSoundForNote(int kit, int note)
{
    const auto& ks = kits();
    if (kit < 0 || kit >= (int) ks.size()) return -1;
    for (const auto& e : ks[(size_t) kit].map)
        if (e.note == note) return indexOfSound(e.sound);
    return -1;
}
} // namespace drums
