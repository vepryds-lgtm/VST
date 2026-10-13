#include "Params.h"
#include <unordered_map>

namespace aura
{
static const ParamDef kDefs[kNumParams] = {
#define AURA_DEF_F(id, name, mn, mx, df, sk) { #id, name, ParamType::Float,  mn, mx, df, sk, nullptr },
#define AURA_DEF_C(id, name, ch, df)         { #id, name, ParamType::Choice, 0.f, 0.f, (float) (df), 1.f, ch },
#define AURA_DEF_B(id, name, df)             { #id, name, ParamType::Bool,   0.f, 1.f, (df) ? 1.f : 0.f, 1.f, nullptr },
#define AURA_DEF_I(id, name, mn, mx, df)     { #id, name, ParamType::Int,    (float) (mn), (float) (mx), (float) (df), 1.f, nullptr },
    AURA_PARAMS(AURA_DEF_F, AURA_DEF_C, AURA_DEF_B, AURA_DEF_I)
#undef AURA_DEF_F
#undef AURA_DEF_C
#undef AURA_DEF_B
#undef AURA_DEF_I
};

const ParamDef& paramDef(int index) { return kDefs[juce::jlimit(0, (int) kNumParams - 1, index)]; }
float paramDefault(int index)       { return paramDef(index).defV; }

int indexOfParam(const juce::String& id)
{
    static const std::unordered_map<std::string, int> map = [] {
        std::unordered_map<std::string, int> m;
        for (int i = 0; i < kNumParams; ++i)
            m[kDefs[i].id] = i;
        return m;
    }();
    auto it = map.find(id.toStdString());
    return it == map.end() ? -1 : it->second;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& d = kDefs[i];
        const juce::ParameterID pid { d.id, 1 };
        switch (d.type)
        {
            case ParamType::Float:
                layout.add(std::make_unique<juce::AudioParameterFloat>(
                    pid, d.name, juce::NormalisableRange<float>(d.minV, d.maxV, 0.f, d.skew), d.defV));
                break;
            case ParamType::Choice:
                layout.add(std::make_unique<juce::AudioParameterChoice>(
                    pid, d.name, juce::StringArray::fromTokens(d.choices, "|", ""), (int) d.defV));
                break;
            case ParamType::Bool:
                layout.add(std::make_unique<juce::AudioParameterBool>(pid, d.name, d.defV > 0.5f));
                break;
            case ParamType::Int:
                layout.add(std::make_unique<juce::AudioParameterInt>(pid, d.name, (int) d.minV, (int) d.maxV, (int) d.defV));
                break;
        }
    }
    return layout;
}
} // namespace aura
