#include "DrumEngine.h"
#include <algorithm>
#include <cmath>
#include <mutex>

namespace drums
{
std::shared_ptr<const Bank> Bank::get()
{
    static std::once_flag once;
    static std::shared_ptr<const Bank> instance;
    std::call_once(once, []
    {
        auto b = std::make_shared<Bank>();
        const int n = numSounds();
        for (int i = 0; i < n; ++i)
        {
            auto s = renderSound(i, kBankRate);
            std::array<float, kThumbPoints> t {};
            const int len = s->length();
            float mx = 1e-9f;
            for (int p = 0; p < kThumbPoints; ++p)
            {
                const int a = (int) ((int64_t) len * p / kThumbPoints), z = std::max(a + 1, (int) ((int64_t) len * (p + 1) / kThumbPoints));
                float m = 0.f;
                for (int k = a; k < std::min(z, len); ++k)
                {
                    m = std::max(m, std::abs(s->l[(size_t) k]));
                    if (!s->isMono()) m = std::max(m, std::abs(s->r[(size_t) k]));
                }
                t[(size_t) p] = m; mx = std::max(mx, m);
            }
            for (auto& v : t) v = std::pow(v / mx, 0.55f);     // compressed so quiet tails stay visible
            b->samples.push_back(std::move(s));
            b->thumbs.push_back(t);
        }
        instance = std::move(b);
    });
    return instance;
}

Engine::Engine(std::shared_ptr<const Bank> b) : bankPtr(std::move(b)) {}

void Engine::prepare(double sr) { hostRate = sr > 1000.0 ? sr : 48000.0; reset(); }

void Engine::reset() { for (auto& v : voices) v = Voice(); }

int Engine::activeVoices() const
{
    int n = 0; for (const auto& v : voices) n += v.active ? 1 : 0;
    return n;
}

void Engine::fade(Voice& v, double seconds)
{
    if (!v.active) return;
    const float step = (float) (1.0 / std::max(8.0, seconds * hostRate));
    if (!v.releasing || step > v.envStep) v.envStep = step;     // never make an already running fade slower
    v.releasing = true;
}

void Engine::start(int sound, double semis, float vel, int note, bool isPreview)
{
    if (sound < 0 || sound >= (int) bankPtr->samples.size()) return;
    const SoundInfo& info = soundInfo(sound);
    const Sample* smp = bankPtr->samples[(size_t) sound].get();

    // 808s are monophonic; hats of the same choke group cut each other; a new preview replaces the old one
    for (auto& v : voices)
    {
        if (!v.active) continue;
        if (info.cat == Cat::Bass808 && v.bass) fade(v, 0.012);
        else if (info.choke != 0 && v.choke == info.choke) fade(v, 0.004);
        else if (isPreview && v.preview) fade(v, 0.015);
    }
    Voice* slot = nullptr;
    for (auto& v : voices) if (!v.active) { slot = &v; break; }
    if (!slot)
    {
        slot = &voices[0];
        for (auto& v : voices) if (v.age < slot->age) slot = &v;
    }
    Voice& v = *slot;
    v = Voice();
    v.s = smp; v.active = true; v.pos = 0.0;
    v.ratio = std::pow(2.0, semis / 12.0) * (kBankRate / hostRate);
    v.gain = std::max(0.f, std::min(1.f, vel));
    v.note = note; v.sound = sound; v.choke = info.choke; v.gated = info.gated; v.held = true;
    v.preview = isPreview; v.bass = info.cat == Cat::Bass808; v.age = ++counter;
    const double len = (double) smp->length();
    v.endPos = std::max(8.0, len * (double) std::max(0.05f, std::min(1.f, st.length)));
    v.fadeLen = (st.length >= 0.999f) ? 0.0 : std::min(0.03 * kBankRate, 0.25 * v.endPos);
}

void Engine::noteOn(int note, float vel)
{
    if (vel <= 0.f) { noteOff(note); return; }
    if (st.kitMode)
    {
        const int s = kitSoundForNote(st.kit, note);
        if (s >= 0) start(s, st.tuneSemis, vel, note, false);
        return;
    }
    const int s = st.sound;
    if (s < 0 || s >= numSounds()) return;
    const int root = soundInfo(s).rootNote;
    double semis = 0.0;
    switch (st.keyMode)
    {
        case KeyMode::Auto:    semis = root >= 0 ? note - root : 0; break;
        case KeyMode::Pitched: semis = note - (root >= 0 ? root : 60); break;
        case KeyMode::Fixed:   semis = 0; break;
    }
    start(s, semis + st.tuneSemis, vel, note, false);
}

void Engine::noteOff(int note)
{
    for (auto& v : voices)
        if (v.active && v.held && v.note == note)
        {
            v.held = false;
            if (v.gated) fade(v, 0.040);
        }
}

void Engine::allNotesOff()
{
    for (auto& v : voices)
        if (v.active) { v.held = false; if (v.gated) fade(v, 0.040); }
}

void Engine::preview(int sound, float vel) { start(sound, 0.0, vel, -1, true); }

float Engine::hermite(const float* d, int n, double pos)
{
    const int i = (int) pos; const float f = (float) (pos - i);
    auto at = [&](int k) { return (k < 0 || k >= n) ? 0.f : d[k]; };
    const float y0 = at(i - 1), y1 = at(i), y2 = at(i + 1), y3 = at(i + 2);
    const float c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + c0;
}

void Engine::render(float* L, float* R, int num)
{
    for (auto& v : voices)
    {
        if (!v.active) continue;
        const int n = v.s->length();
        const float* dl = v.s->l.data();
        const float* dr = v.s->isMono() ? dl : v.s->r.data();
        const float g = v.gain;
        for (int i = 0; i < num; ++i)
        {
            if (v.pos >= v.endPos || v.pos >= (double) n) { v.active = false; break; }
            float gain = g * v.env;
            if (v.fadeLen > 0.0 && v.pos > v.endPos - v.fadeLen)
            {
                const float u = (float) ((v.endPos - v.pos) / v.fadeLen);          // 1 -> 0
                gain *= 0.5f - 0.5f * std::cos(3.14159265f * std::max(0.f, u));
            }
            L[i] += hermite(dl, n, v.pos) * gain;
            R[i] += hermite(dr, n, v.pos) * gain;
            v.pos += v.ratio;
            if (v.releasing)
            {
                v.env -= v.envStep;
                if (v.env <= 0.f) { v.active = false; break; }
            }
        }
    }
}
} // namespace drums
