#pragma once
// AFTERLIFE - THE SPACES
// The convolution reverb's rooms. Each built-in space is an impulse response grown from noise: a few early
// reflections off the walls, a tail that thickens as the echoes multiply, highs that die faster than lows (or
// slower, in stone), and for some rooms a ring or a flutter of their own. DECAY sets how long the tail takes to
// fall 60 dB. LOADED IR uses any WAV / AIFF / FLAC you load instead, and DECAY then trims its tail.
// No third-party recordings are bundled: every built-in IR is generated here, the same every time.
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

namespace after
{
constexpr float kMaxIrSeconds = 16.0f;

struct SpaceDef
{
    const char* name;
    const char* blurb;
    float decay;          // default decay (s), used by the presets
    float attackMs;       // how long the tail takes to swell in
    float densityMs;      // how long until the echoes are a dense wash
    float lowMult, highMult;   // decay of lows / highs relative to DECAY
    float brightHz;       // overall top end
    int erCount; float erStartMs, erSpreadMs, erGain;   // early reflections
    float flutterMs, flutterGain;                       // flutter echo between parallel walls
    float comb1Ms, comb2Ms, combFb;                     // the room's own ring
    float width;
};

inline const std::vector<SpaceDef>& spaces()
{
    static const std::vector<SpaceDef> s {
        { "CHAMBER",     "A small stone room. Tight and natural.",                 1.2f,   3.0f,  12.0f, 1.10f, 0.55f,  9000.0f, 10,  3.0f,  25.0f, 0.50f,  0.0f, 0.0f,  0.0f,  0.0f, 0.00f, 0.85f },
        { "CRYPT",       "Low stone ceiling. Boxy, close and cold.",               1.6f,   2.0f,   8.0f, 1.25f, 0.75f,  7000.0f, 14,  2.0f,  18.0f, 0.70f,  0.0f, 0.0f,  6.1f,  9.7f, 0.35f, 0.60f },
        { "CATACOMB",    "Long tunnels. The walls flutter back at you.",           3.2f,   8.0f,  30.0f, 1.20f, 0.45f,  5500.0f,  6,  8.0f,  60.0f, 0.50f, 27.0f, 0.35f, 0.0f,  0.0f, 0.00f, 0.90f },
        { "CATHEDRAL",   "Huge and holy. A slow swell and a long, dark tail.",     5.5f,  35.0f,  60.0f, 1.30f, 0.50f,  8000.0f,  8, 18.0f, 110.0f, 0.35f,  0.0f, 0.0f,  0.0f,  0.0f, 0.00f, 1.00f },
        { "CISTERN",     "An underground tank. Concrete and water, it rings.",     7.0f,  15.0f,  40.0f, 1.00f, 0.95f, 11000.0f,  6, 12.0f,  80.0f, 0.40f,  0.0f, 0.0f, 11.3f, 17.9f, 0.55f, 1.00f },
        { "IRON PLATE",  "A steel plate. Instantly dense, bright and smooth.",     2.2f,   0.5f,   1.0f, 0.80f, 0.85f, 14000.0f,  0,  0.0f,   0.0f, 0.00f,  0.0f, 0.0f,  0.0f,  0.0f, 0.00f, 1.00f },
        { "MAUSOLEUM",   "A marble hall. Bright, polished, a little cruel.",       3.0f,  10.0f,  25.0f, 1.15f, 0.80f, 10000.0f, 12,  6.0f,  60.0f, 0.45f,  0.0f, 0.0f,  0.0f,  0.0f, 0.00f, 0.95f },
        { "THE VOID",    "No walls at all. It blooms in and never quite ends.",   11.0f, 250.0f, 120.0f, 1.40f, 0.35f,  4500.0f,  0,  0.0f,   0.0f, 0.00f,  0.0f, 0.0f,  0.0f,  0.0f, 0.00f, 1.00f },
    };
    return s;
}
inline int numBuiltInSpaces() { return (int) spaces().size(); }
inline int loadedIrIndex() { return numBuiltInSpaces(); }   // the last choice: the IR the user loaded

inline juce::StringArray spaceNames()
{
    juce::StringArray n;
    for (auto& s : spaces()) n.add (s.name);
    n.add ("LOADED IR");
    return n;
}

//==============================================================================
namespace irdetail
{
// one-pole low-pass, used to split noise into bands and darken the top end
struct OnePole
{
    float a = 0, z = 0;
    void setHz (float hz, double sr) { a = (float) std::exp (-juce::MathConstants<double>::twoPi * hz / sr); }
    float operator() (float x) { z = (1.0f - a) * x + a * z; return z; }
};

inline void normaliseEnergy (juce::AudioBuffer<float>& ir, float perChannelEnergy = 0.25f)
{
    for (int c = 0; c < ir.getNumChannels(); ++c)
    {
        double e = 0;
        const float* d = ir.getReadPointer (c);
        for (int i = 0; i < ir.getNumSamples(); ++i) e += (double) d[i] * d[i];
        if (e > 1.0e-20) ir.applyGain (c, 0, ir.getNumSamples(), (float) std::sqrt (perChannelEnergy / e));
    }
}

inline void fadeTail (juce::AudioBuffer<float>& ir, float fraction)
{
    const int n = ir.getNumSamples(), f = juce::jmax (1, (int) ((float) n * fraction));
    for (int c = 0; c < ir.getNumChannels(); ++c)
        for (int i = 0; i < f; ++i)
            ir.getWritePointer (c)[n - f + i] *= 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (float) i / (float) f);
}
} // namespace irdetail

//==============================================================================
// A built-in space at the given decay (seconds to fall 60 dB), stereo, at the given sample rate.
inline juce::AudioBuffer<float> buildSpace (int index, float decaySec, double sr)
{
    using namespace irdetail;
    const auto& d = spaces()[(size_t) juce::jlimit (0, numBuiltInSpaces() - 1, index)];
    const float rt = juce::jlimit (0.1f, 15.0f, decaySec);
    const float attack = d.attackMs * 0.001f, density = juce::jmax (0.0005f, d.densityMs * 0.001f);
    const int len = juce::jmin ((int) ((rt * 1.15f + attack * 2.0f + d.erStartMs * 0.001f + 0.05f) * sr), (int) (kMaxIrSeconds * sr));
    juce::AudioBuffer<float> ir (2, juce::jmax (64, len));
    ir.clear();
    const float k60 = 6.9078f;   // ln(1000): -60 dB
    const float tLow = rt * d.lowMult, tMid = rt, tHigh = rt * d.highMult;

    for (int ch = 0; ch < 2; ++ch)
    {
        juce::Random rnd ((juce::int64) (9001 + index * 131 + ch * 7919));
        OnePole lowSplit, highSplit;
        lowSplit.setHz (350.0f, sr); highSplit.setHz (3500.0f, sr);
        float* h = ir.getWritePointer (ch);
        for (int i = 0; i < ir.getNumSamples(); ++i)
        {
            const float t = (float) (i / sr);
            float n = rnd.nextFloat() * 2.0f - 1.0f;
            // the echoes start sparse and thicken into a wash
            if (t < density)
            {
                const float p = juce::jlimit (0.02f, 1.0f, (t / density) * (t / density) + 0.02f);
                n = rnd.nextFloat() < p ? n / std::sqrt (p) : 0.0f;
            }
            const float lo = lowSplit (n), belowHigh = highSplit (n);
            const float mid = belowHigh - lo, hi = n - belowHigh;
            const float onset = attack > 0 ? 1.0f - std::exp (-t / attack) : 1.0f;
            h[i] = onset * (2.2f * lo * std::exp (-k60 * t / tLow) + mid * std::exp (-k60 * t / tMid) + 0.8f * hi * std::exp (-k60 * t / tHigh));
        }

        // early reflections off the walls
        for (int k = 0; k < d.erCount; ++k)
        {
            const float u = rnd.nextFloat();
            const float t = (d.erStartMs + d.erSpreadMs * std::pow (u, 1.5f)) * 0.001f + (ch == 0 ? 0.0f : 0.0007f * rnd.nextFloat());
            const int at = (int) (t * sr);
            const float g = d.erGain * std::exp (-0.15f * (float) k) * (0.5f + 0.5f * rnd.nextFloat()) * (rnd.nextBool() ? 1.0f : -1.0f);
            for (int j = 0; j < 3 && at + j < ir.getNumSamples(); ++j) h[at + j] += g * (j == 1 ? 1.0f : 0.45f);
        }

        // flutter: two parallel walls bouncing it back and forth
        if (d.flutterMs > 0)
        {
            float t = d.flutterMs * 0.001f * (ch == 0 ? 1.0f : 1.5f);
            float sign = 1.0f;
            while (t < rt * 0.6f)
            {
                const int at = (int) (t * sr);
                if (at + 2 >= ir.getNumSamples()) break;
                const float g = d.flutterGain * std::exp (-k60 * t / (rt * 0.5f)) * sign;
                h[at] += 0.45f * g; h[at + 1] += g; h[at + 2] += 0.45f * g;
                t += d.flutterMs * 0.001f * (0.98f + 0.04f * rnd.nextFloat());
                sign = -sign;
            }
        }

        // the room's own ring (two feedback combs)
        for (float ms : { d.comb1Ms, d.comb2Ms })
        {
            if (ms <= 0 || d.combFb <= 0) continue;
            const int D = juce::jmax (1, (int) (ms * 0.001f * (ch == 0 ? 1.0f : 1.037f) * sr));
            for (int i = D; i < ir.getNumSamples(); ++i) h[i] += d.combFb * h[i - D];
        }

        // top end, and no DC
        OnePole tone; tone.setHz (d.brightHz, sr);
        OnePole dc; dc.setHz (20.0f, sr);
        for (int i = 0; i < ir.getNumSamples(); ++i) { const float x = tone (h[i]); h[i] = x - dc (x); }
    }

    // width: blend the two (decorrelated) sides toward mono for the narrower rooms
    if (d.width < 0.999f)
    {
        float* l = ir.getWritePointer (0); float* r = ir.getWritePointer (1);
        for (int i = 0; i < ir.getNumSamples(); ++i)
        {
            const float m = 0.5f * (l[i] + r[i]), s = 0.5f * (l[i] - r[i]) * d.width;
            l[i] = m + s; r[i] = m - s;
        }
    }
    fadeTail (ir, 0.08f);
    normaliseEnergy (ir);
    return ir;
}

// A loaded IR, ready to convolve: stereo, leading silence trimmed, the tail trimmed to DECAY (when it's longer),
// and levelled to the same loudness as the built-in spaces.
inline juce::AudioBuffer<float> prepareLoadedIr (const juce::AudioBuffer<float>& raw, double rawSr, float decaySec)
{
    using namespace irdetail;
    if (raw.getNumSamples() < 8 || raw.getNumChannels() < 1) { juce::AudioBuffer<float> z (2, 64); z.clear(); z.setSample (0, 0, 0.5f); z.setSample (1, 0, 0.5f); return z; }
    // trim leading silence (keep a millisecond before the first sound)
    float peak = 0;
    for (int c = 0; c < raw.getNumChannels(); ++c) peak = juce::jmax (peak, raw.getMagnitude (c, 0, raw.getNumSamples()));
    int first = 0;
    const float thresh = peak * 0.001f;
    for (; first < raw.getNumSamples(); ++first)
    {
        bool loud = false;
        for (int c = 0; c < raw.getNumChannels(); ++c) loud = loud || std::abs (raw.getSample (c, first)) > thresh;
        if (loud) break;
    }
    first = juce::jmax (0, first - (int) (0.001 * rawSr));
    const int avail = raw.getNumSamples() - first;
    const int want = juce::jlimit (32, (int) (kMaxIrSeconds * rawSr), (int) (juce::jmax (0.1f, decaySec) * 1.15f * rawSr));
    const int len = juce::jmin (avail, want);
    juce::AudioBuffer<float> ir (2, juce::jmax (32, len));
    ir.clear();
    for (int c = 0; c < 2; ++c) ir.copyFrom (c, 0, raw, juce::jmin (c, raw.getNumChannels() - 1), first, len);
    if (len < avail) fadeTail (ir, 0.25f);   // trimmed: fade out the last quarter
    else fadeTail (ir, 0.01f);
    normaliseEnergy (ir);
    return ir;
}

// The IR's outline for the screen: peak level (dB) in 'points' slices
inline std::vector<float> outline (const juce::AudioBuffer<float>& ir, int points = 160)
{
    std::vector<float> o ((size_t) points, -90.0f);
    const int n = ir.getNumSamples();
    if (n <= 0) return o;
    float top = 1.0e-9f;
    for (int c = 0; c < ir.getNumChannels(); ++c) top = juce::jmax (top, ir.getMagnitude (c, 0, n));
    for (int p = 0; p < points; ++p)
    {
        const int a = (int) ((juce::int64) p * n / points), b = juce::jmax (a + 1, (int) ((juce::int64) (p + 1) * n / points));
        float m = 0;
        for (int c = 0; c < ir.getNumChannels(); ++c) m = juce::jmax (m, ir.getMagnitude (c, a, b - a));
        o[(size_t) p] = juce::Decibels::gainToDecibels (m / top, -90.0f);
    }
    return o;
}
} // namespace after
