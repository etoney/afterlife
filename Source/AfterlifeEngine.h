#pragma once
// AFTERLIFE - THE ENGINE
// One insert, two effects, side by side:
//
//   dry ──┬──────────────────────────────────────────────────────────┐
//         ├─ x reverb send ─ PRE-DELAY ─ convolution (the SPACE) ─ LOW CUT / HIGH CUT ─ WIDTH ─ DUCK ─ LEVEL ─┤
//         ├─ x delay send ── MAIN echo  (TIME, FEEDBACK) ─┐                                                 ├─ OUTPUT
//         └─ x section send ─ THROW echo (the section's own TIME, FEEDBACK) ─┴─ TONE / LOW CUT / DIRT ─ DUCK ─ LEVEL ─┘
//
// The sends follow the lanes while the song plays: inside a REVERB section the reverb send is the section's;
// inside a DELAY section the delay signal goes to the THROW echo instead of the MAIN one. The THROW echo keeps
// the settings of the last section it heard, so a throw rings out with its own time and feedback after the
// section ends, while the MAIN echo carries on with the knobs.
#include <juce_dsp/juce_dsp.h>
#include "Spaces.h"
#include "Sections.h"
#include <atomic>
#include <memory>

namespace after
{
enum DelayMode { kStereo = 0, kPingPong = 1, kMono = 2 };

struct Params
{
    // reverb
    bool rvOn = true;
    int space = 0;
    float rvSend = 0.25f, decay = 1.2f, predelayMs = 20.0f, rvLowCut = 150.0f, rvHighCut = 9000.0f, width = 1.0f, rvDuck = 0.0f, rvLevelDb = 0.0f;
    // delay
    bool dlOn = true, sync = true;
    int division = 9;   // 1/4
    float delayMs = 375.0f, feedback = 0.35f;
    int mode = kStereo;
    float tone = 6000.0f, dlLowCut = 150.0f, dirt = 0.1f, dlDuck = 0.0f, dlSend = 0.0f, dlLevelDb = 0.0f;
    // both
    float dry = 1.0f, outputDb = 0.0f;
    bool lanes = true;
};

struct Transport { bool playing = false; double timeSec = 0; double bpm = 120.0; };

struct LoadedIr { juce::AudioBuffer<float> audio; double sampleRate = 48000.0; juce::String name; };

//==============================================================================
// TPT state-variable filter (12 dB/oct), low-pass and high-pass outputs
struct SVF
{
    float g = 0, k = 1.41421356f, a1 = 1, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    void set (float hz, double sr)
    {
        g = (float) std::tan (juce::MathConstants<double>::pi * juce::jlimit (10.0, sr * 0.45, (double) hz) / sr);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    inline float lp (float x) { const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3; ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; return v2; }
    inline float hp (float x) { const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3; ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; return x - k * v1 - v2; }
};

//==============================================================================
// One stereo echo: a delay line with filtered, saturated feedback. Time changes either glide (turning the knob,
// like tape) or crossfade to the new time (a new throw starting: the old echoes jump, they don't warble).
struct Echo
{
    struct Cfg { float feedback = 0.3f; int mode = kStereo; float dirt = 0; };

    void prepare (double sampleRate, double maxSeconds)
    {
        sr = sampleRate;
        int size = 1;
        while (size < (int) (maxSeconds * sr) + 8) size <<= 1;
        mask = size - 1;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        glide = 1.0 - std::exp (-1.0 / (0.06 * sr));
        xfStep = 1.0f / (float) (0.03 * sr);
        reset();
    }
    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& f : lpf) f.reset();
        for (auto& f : hpf) f.reset();
        w = 0; fading = false; xf = 0;
    }
    void setFilters (float toneHz, float lowCutHz) { for (int c = 0; c < 2; ++c) { lpf[c].set (toneHz, sr); hpf[c].set (lowCutHz, sr); } }
    float maxDelay() const { return (float) (mask - 4); }

    // jump = crossfade to the new time; otherwise glide
    void setTime (float samples, bool jump)
    {
        samples = juce::jlimit (1.0f, maxDelay() - 64.0f, samples);
        tgt = samples;
        if (! started) { cur = samples; started = true; return; }
        if (jump && std::abs ((double) samples - (fading ? nxt : cur)) > 0.5)
        {
            if (fading) cur = nxt;
            nxt = samples; xf = 0; fading = true;
        }
    }

    inline float read (const std::vector<float>& b, double d) const
    {
        const double rp = (double) w - d;
        const double fl = std::floor (rp);
        const int i0 = (int) (juce::int64) fl & mask;
        const float fr = (float) (rp - fl);
        return b[(size_t) i0] + fr * (b[(size_t) ((i0 + 1) & mask)] - b[(size_t) i0]);
    }

    static inline float sat (float x, float drive) { return std::tanh (x * drive) / drive; }

    inline void tick (float inL, float inR, float& outL, float& outR, const Cfg& cfg)
    {
        // wow & flutter (DIRT)
        float wow = 0;   // only ever adds delay, so the echo never reads ahead of what's written
        if (cfg.dirt > 0.001f)
        {
            ph1 += 0.45f / (float) sr; if (ph1 >= 1.0f) ph1 -= 1.0f;
            ph2 += 5.7f / (float) sr; if (ph2 >= 1.0f) ph2 -= 1.0f;
            wow = cfg.dirt * (float) sr * (0.0022f * (1.0f + std::sin (6.2831853f * ph1)) + 0.00025f * (1.0f + std::sin (6.2831853f * ph2)));
        }
        if (! fading) cur += (tgt - cur) * glide;
        float yL = read (buf[0], cur + wow), yR = read (buf[1], cur + wow);
        if (fading)
        {
            const float zL = read (buf[0], nxt + wow), zR = read (buf[1], nxt + wow);
            yL += (zL - yL) * xf; yR += (zR - yR) * xf;
            xf += xfStep;
            if (xf >= 1.0f) { cur = nxt; fading = false; }
        }
        yL = hpf[0].hp (lpf[0].lp (yL));
        yR = hpf[1].hp (lpf[1].lp (yR));
        const float drive = 1.0f + 3.0f * cfg.dirt;
        const float sL = sat (yL, drive), sR = sat (yR, drive);
        const float fb = cfg.feedback;
        float wL, wR;
        switch (cfg.mode)
        {
            case kPingPong: { const float m = 0.5f * (inL + inR); wL = m + fb * sR; wR = fb * sL; break; }
            case kMono:     { const float m = 0.5f * (inL + inR); wL = wR = m + fb * sL; yR = yL; break; }
            default:        { wL = inL + fb * sL; wR = inR + fb * sR; break; }
        }
        buf[0][(size_t) w] = wL; buf[1][(size_t) w] = wR;
        w = (w + 1) & mask;
        outL = yL; outR = yR;
    }

    double sr = 48000.0;
    std::array<std::vector<float>, 2> buf;
    std::array<SVF, 2> lpf, hpf;
    int mask = 0, w = 0;
    double cur = 1, tgt = 1, nxt = 1, glide = 0.001;   // double: a float can't glide the last few samples at 18000
    float xf = 0, xfStep = 0.001f, ph1 = 0, ph2 = 0.3f;
    bool fading = false, started = false;
};

//==============================================================================
class Engine
{
public:
    Engine() = default;
    ~Engine() { stopBuilder(); }

    // ---- setup (not the audio thread)
    void prepare (double sampleRate, int maxBlockSize)
    {
        stopBuilder();
        sr = sampleRate; maxBlock = juce::jmax (16, maxBlockSize);
        for (auto* b : { &rvSendBuf, &mainSendBuf, &throwSendBuf }) b->assign ((size_t) maxBlock, 0.0f);
        revBuf.setSize (2, maxBlock);
        mainEcho.prepare (sr, 6.5); throwEcho.prepare (sr, 6.5);
        preSize = 1; while (preSize < (int) (0.3 * sr) + 8) preSize <<= 1;
        for (auto& b : preBuf) b.assign ((size_t) preSize, 0.0f);

        // the space, ready before the first block
        {
            const juce::ScopedLock l (buildLock);
            const auto key = wantedKey();
            double irSr = sr;
            auto ir = makeIr (key, irSr);
            conv.loadImpulseResponse (std::move (ir), irSr, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
            builtKey = key;
            const juce::SpinLock::ScopedLockType sl (pendingLock);
            hasPending = false;
        }
        conv.prepare ({ sr, (juce::uint32) maxBlock, 2 });

        for (auto* s : { &rvSendSm, &dlSendSm, &rvLevelSm, &dlLevelSm, &drySm, &outSm, &widthSm, &rvOnSm, &dlOnSm })
            s->reset (sr, 0.03);
        preSm.reset (sr, 0.08);
        snapSmoothers();
        reset();
        startBuilder();
    }
    void release() { stopBuilder(); }

    void reset()
    {
        conv.reset();
        mainEcho.reset(); throwEcho.reset();
        for (auto& b : preBuf) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& f : rvLow) f.reset();
        for (auto& f : rvHigh) f.reset();
        preW = 0; duckEnv = 0; latchedId = -2;
    }

    // every block (audio thread) or before prepare
    void setParams (const Params& p)
    {
        params = p;
        wantSpace.store (p.space);
        wantDecay.store (p.decay);
    }

    // the IR used by LOADED IR (message thread)
    void setLoadedIr (std::shared_ptr<const LoadedIr> ir)
    {
        std::atomic_store (&loaded, std::move (ir));
        ++loadedVersion;
    }
    std::shared_ptr<const LoadedIr> getLoadedIr() const { return std::atomic_load (&loaded); }

    // ---- the audio
    void process (float* const* io, int numCh, int numSamples, const Transport& tr, const SectionSet* sections)
    {
        if (numCh <= 0 || numSamples <= 0) return;
        consumePendingIr();
        int done = 0;
        while (done < numSamples)
        {
            const int n = juce::jmin (maxBlock, numSamples - done);
            float* ch[2] = { io[0] + done, io[numCh > 1 ? 1 : 0] + done };
            Transport t = tr;
            t.timeSec += (double) done / sr;
            chunk (ch, numCh > 1 ? 2 : 1, n, t, sections);
            done += n;
        }
    }

    // how long the effects keep ringing after the input stops (seconds)
    double tailSeconds() const
    {
        const auto& p = params;
        auto echoTail = [] (double t, double fb) { return fb < 0.001 ? t : t * std::log (0.001) / std::log (juce::jlimit (0.001, 0.95, fb)); };
        const double mainT = delaySeconds (p, -1), throwT = lastThrowSeconds.load();
        const double d = std::max (echoTail (mainT, p.feedback), echoTail (throwT, lastThrowFb.load()));
        const double r = p.decay * 1.15 + p.predelayMs * 0.001;
        return juce::jlimit (0.5, 30.0, std::max (p.rvOn ? r : 0.0, p.dlOn ? d : 0.0) + 0.2);
    }

    // the delay time in seconds for a division (-1 = the TIME knob)
    double delaySeconds (const Params& p, int division) const
    {
        const double bpm = juce::jlimit (20.0, 400.0, lastBpm.load());
        if (division >= 0) return divisions()[(size_t) juce::jlimit (0, (int) divisions().size() - 1, division)].quarters * 60.0 / bpm;
        if (p.sync) return divisions()[(size_t) juce::jlimit (0, (int) divisions().size() - 1, p.division)].quarters * 60.0 / bpm;
        return p.delayMs * 0.001;
    }

    // ---- for the screens (any thread)
    struct IrView { std::vector<float> outline; double seconds = 0; int space = 0; juce::String name; };
    std::shared_ptr<const IrView> irView() const { return std::atomic_load (&irv); }
    std::atomic<float> wetPeak { 0 }, inPeak { 0 }, revSendNow { 0 }, mainSendNow { 0 }, throwSendNow { 0 };
    std::atomic<double> lastBpm { 120.0 }, lastThrowSeconds { 0.5 }, lastThrowFb { 0.0 };
    std::atomic<int> throwSectionId { -1 };
    std::atomic<bool> rvInSection { false }, dlInSection { false };
    int irBuilds() const { return builds.load(); }

private:
    struct Key
    {
        int space = 0; int decayCs = 0; int loadedVer = 0; double rate = 0;
        bool operator== (const Key& o) const { return space == o.space && decayCs == o.decayCs && loadedVer == o.loadedVer && rate == o.rate; }
        bool operator!= (const Key& o) const { return ! (*this == o); }
    };
    Key wantedKey() const { return { wantSpace.load(), juce::roundToInt (wantDecay.load() * 100.0f), loadedVersion.load(), sr }; }

    juce::AudioBuffer<float> makeIr (const Key& k, double& irSr)
    {
        auto li = std::atomic_load (&loaded);
        const float decay = (float) k.decayCs * 0.01f;
        juce::AudioBuffer<float> ir;
        auto view = std::make_shared<IrView>();
        view->space = k.space;
        if (k.space >= loadedIrIndex() && li != nullptr)
        {
            irSr = li->sampleRate;
            ir = prepareLoadedIr (li->audio, li->sampleRate, decay);
            view->name = li->name;
        }
        else
        {
            irSr = k.rate;
            const int s = k.space >= loadedIrIndex() ? 0 : k.space;
            ir = buildSpace (s, decay, k.rate);
            view->name = k.space >= loadedIrIndex() ? juce::String ("NO IR LOADED (CHAMBER)") : juce::String (spaces()[(size_t) s].name);
        }
        view->outline = outline (ir);
        view->seconds = ir.getNumSamples() / juce::jmax (1.0, irSr);
        std::atomic_store (&irv, std::shared_ptr<const IrView> (view));
        ++builds;
        return ir;
    }

    // a new space waiting for the audio thread
    void consumePendingIr()
    {
        const juce::SpinLock::ScopedTryLockType l (pendingLock);
        if (! l.isLocked() || ! hasPending) return;
        conv.loadImpulseResponse (std::move (pending), pendingSr, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        hasPending = false;
    }

    struct Builder : juce::Thread
    {
        explicit Builder (Engine& e) : juce::Thread ("Afterlife spaces"), owner (e) {}
        void run() override
        {
            while (! threadShouldExit())
            {
                const auto key = owner.wantedKey();
                {
                const juce::ScopedLock l (owner.buildLock);
                if (key != owner.builtKey)
                {
                    double irSr = owner.sr;
                    auto ir = owner.makeIr (key, irSr);
                    {
                        const juce::SpinLock::ScopedLockType sl (owner.pendingLock);
                        std::swap (owner.pending, ir);
                        owner.pendingSr = irSr;
                        owner.hasPending = true;
                    }
                    owner.builtKey = key;
                }
                }
                wait (30);
            }
        }
        Engine& owner;
    };
    void startBuilder() { builder = std::make_unique<Builder> (*this); builder->startThread (juce::Thread::Priority::low); }
    void stopBuilder() { if (builder != nullptr) { builder->stopThread (4000); builder.reset(); } }

    void snapSmoothers()
    {
        const auto& p = params;
        rvSendSm.setCurrentAndTargetValue (p.rvSend); dlSendSm.setCurrentAndTargetValue (p.dlSend);
        rvLevelSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.rvLevelDb));
        dlLevelSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.dlLevelDb));
        drySm.setCurrentAndTargetValue (p.dry); outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.outputDb));
        widthSm.setCurrentAndTargetValue (p.width);
        rvOnSm.setCurrentAndTargetValue (p.rvOn ? 1.0f : 0.0f); dlOnSm.setCurrentAndTargetValue (p.dlOn ? 1.0f : 0.0f);
        preSm.setCurrentAndTargetValue (p.predelayMs * 0.001f * (float) sr);
    }

    void chunk (float* const* io, int numCh, int n, const Transport& tr, const SectionSet* sections)
    {
        const auto& p = params;
        if (tr.bpm > 0) lastBpm.store (tr.bpm);
        rvSendSm.setTargetValue (p.rvSend); dlSendSm.setTargetValue (p.dlSend);
        rvLevelSm.setTargetValue (juce::Decibels::decibelsToGain (p.rvLevelDb));
        dlLevelSm.setTargetValue (juce::Decibels::decibelsToGain (p.dlLevelDb));
        drySm.setTargetValue (p.dry); outSm.setTargetValue (juce::Decibels::decibelsToGain (p.outputDb));
        widthSm.setTargetValue (p.width);
        rvOnSm.setTargetValue (p.rvOn ? 1.0f : 0.0f); dlOnSm.setTargetValue (p.dlOn ? 1.0f : 0.0f);
        preSm.setTargetValue (juce::jlimit (0.0f, (float) preSize - 8.0f, p.predelayMs * 0.001f * (float) sr));
        for (int c = 0; c < 2; ++c) { rvLow[c].set (p.rvLowCut, sr); rvHigh[c].set (p.rvHighCut, sr); }
        mainEcho.setFilters (p.tone, p.dlLowCut); throwEcho.setFilters (p.tone, p.dlLowCut);
        mainEcho.setTime ((float) (delaySeconds (p, -1) * sr), false);

        const bool lanes = p.lanes && tr.playing && sections != nullptr;
        float* rs = rvSendBuf.data(); float* ms = mainSendBuf.data(); float* ts = throwSendBuf.data();
        const double dt = 1.0 / sr;
        for (int i = 0; i < n; ++i)
        {
            const double t = tr.timeSec + (double) i * dt;
            const float baseR = rvSendSm.getNextValue(), baseD = dlSendSm.getNextValue();
            float r = baseR, m = baseD, th = 0.0f;
            if (lanes)
            {
                if (const auto* s = sections->at (kReverbLane, t)) { const float w = s->weightAt (t); r = baseR + (s->send - baseR) * w; }
                if (const auto* s = sections->at (kDelayLane, t)) { const float w = s->weightAt (t); m = baseD * (1.0f - w); th = s->send * w; }
                // the throw echo takes the settings of the last delay section that started
                if (const auto* s = sections->latest (kDelayLane, t); s != nullptr && (s->id != latchedId || s->division != latchedDiv || s->feedback != latchedFb))
                {
                    latchedId = s->id; latchedDiv = s->division; latchedFb = s->feedback;
                    const double secs = delaySeconds (p, s->division);
                    throwEcho.setTime ((float) (secs * sr), true);
                    lastThrowSeconds.store (secs); lastThrowFb.store (s->feedback);
                    throwSectionId.store (s->id);
                }
            }
            rs[i] = r; ms[i] = m; ts[i] = th;
        }
        // the throw follows tempo / TIME-knob changes too
        if (latchedId >= 0) throwEcho.setTime ((float) (delaySeconds (p, latchedDiv) * sr), false);
        else throwEcho.setTime ((float) (delaySeconds (p, -1) * sr), false);
        revSendNow.store (rs[n - 1]); mainSendNow.store (ms[n - 1]); throwSendNow.store (ts[n - 1]);
        {
            const double tEnd = tr.timeSec + (double) (n - 1) * dt;
            rvInSection.store (lanes && sections->at (kReverbLane, tEnd) != nullptr);
            dlInSection.store (lanes && sections->at (kDelayLane, tEnd) != nullptr);
        }

        float* L = io[0]; float* R = io[numCh > 1 ? 1 : 0];
        float inPk = 0;
        for (int i = 0; i < n; ++i) inPk = juce::jmax (inPk, std::abs (L[i]), std::abs (R[i]));
        inPeak.store (juce::jmax (inPeak.load(), inPk));

        // ---- reverb: send -> pre-delay -> the space
        const bool rvLive = rvOnSm.getTargetValue() > 0.0f || rvOnSm.isSmoothing();
        float* vL = revBuf.getWritePointer (0); float* vR = revBuf.getWritePointer (1);
        if (rvLive)
        {
            for (int i = 0; i < n; ++i)
            {
                preBuf[0][(size_t) preW] = L[i] * rs[i];
                preBuf[1][(size_t) preW] = R[i] * rs[i];
                const float d = preSm.getNextValue();
                const float rp = (float) preW - d;
                const float fl = std::floor (rp);
                const int i0 = (int) fl & (preSize - 1), i1 = (i0 + 1) & (preSize - 1);
                const float fr = rp - fl;
                vL[i] = preBuf[0][(size_t) i0] + fr * (preBuf[0][(size_t) i1] - preBuf[0][(size_t) i0]);
                vR[i] = preBuf[1][(size_t) i0] + fr * (preBuf[1][(size_t) i1] - preBuf[1][(size_t) i0]);
                preW = (preW + 1) & (preSize - 1);
            }
            juce::dsp::AudioBlock<float> blk (revBuf.getArrayOfWritePointers(), 2, (size_t) n);
            conv.process (juce::dsp::ProcessContextReplacing<float> (blk));
        }
        else if (! rvWasOff) { conv.reset(); for (auto& b : preBuf) std::fill (b.begin(), b.end(), 0.0f); }
        rvWasOff = ! rvLive;

        // ---- delay + mix
        Echo::Cfg mainCfg { p.feedback, p.mode, p.dirt };
        Echo::Cfg throwCfg { latchedId >= 0 ? latchedFb : p.feedback, p.mode, p.dirt };
        const bool dlLive = dlOnSm.getTargetValue() > 0.0f || dlOnSm.isSmoothing();
        if (! dlLive && ! dlWasOff) { mainEcho.reset(); throwEcho.reset(); }
        dlWasOff = ! dlLive;
        const float att = 1.0f - std::exp (-1.0f / (float) (0.004 * sr)), rel = 1.0f - std::exp (-1.0f / (float) (0.25 * sr));
        float wetPk = 0;
        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = R[i];
            const float lvl = std::max (std::abs (inL), std::abs (inR));
            duckEnv += (lvl - duckEnv) * (lvl > duckEnv ? att : rel);
            const float duckAmt = juce::jmin (1.0f, duckEnv * 4.0f);

            float wL = 0, wR = 0;
            const float rvGain = rvOnSm.getNextValue() * rvLevelSm.getNextValue();
            const float wid = widthSm.getNextValue();
            if (rvLive)
            {
                float a = rvHigh[0].lp (rvLow[0].hp (vL[i])), b = rvHigh[1].lp (rvLow[1].hp (vR[i]));
                const float mid = 0.5f * (a + b), side = 0.5f * (a - b) * wid;
                const float g = rvGain * (1.0f - p.rvDuck * duckAmt);
                wL += (mid + side) * g; wR += (mid - side) * g;
            }
            const float dlGain = dlOnSm.getNextValue() * dlLevelSm.getNextValue();
            if (dlLive)
            {
                float mL, mR, tL, tR;
                mainEcho.tick (inL * ms[i], inR * ms[i], mL, mR, mainCfg);
                throwEcho.tick (inL * ts[i], inR * ts[i], tL, tR, throwCfg);
                const float g = dlGain * (1.0f - p.dlDuck * duckAmt);
                wL += (mL + tL) * g; wR += (mR + tR) * g;
            }
            wetPk = juce::jmax (wetPk, std::abs (wL), std::abs (wR));
            const float dry = drySm.getNextValue(), out = outSm.getNextValue();
            if (numCh > 1) { L[i] = (inL * dry + wL) * out; R[i] = (inR * dry + wR) * out; }
            else L[i] = (inL * dry + 0.5f * (wL + wR)) * out;
        }
        wetPeak.store (juce::jmax (wetPeak.load(), wetPk));
    }

    Params params;
    double sr = 48000.0;
    int maxBlock = 512;

    juce::dsp::Convolution conv { juce::dsp::Convolution::NonUniform { 512 } };
    juce::AudioBuffer<float> revBuf;
    std::array<std::vector<float>, 2> preBuf;
    int preSize = 1, preW = 0;
    std::array<SVF, 2> rvLow, rvHigh;
    Echo mainEcho, throwEcho;
    std::vector<float> rvSendBuf, mainSendBuf, throwSendBuf;
    juce::SmoothedValue<float> rvSendSm, dlSendSm, rvLevelSm, dlLevelSm, drySm, outSm, widthSm, rvOnSm, dlOnSm, preSm;
    float duckEnv = 0;
    int latchedId = -2, latchedDiv = -1;
    float latchedFb = 0.5f;
    bool rvWasOff = false, dlWasOff = false;

    // the spaces, built off the audio thread
    std::atomic<int> wantSpace { 0 }, loadedVersion { 0 }, builds { 0 };
    std::atomic<float> wantDecay { 1.2f };
    std::shared_ptr<const LoadedIr> loaded;
    std::shared_ptr<const IrView> irv;
    juce::CriticalSection buildLock;
    Key builtKey;
    juce::SpinLock pendingLock;
    juce::AudioBuffer<float> pending;
    double pendingSr = 48000.0;
    bool hasPending = false;
    std::unique_ptr<Builder> builder;
};
} // namespace after
