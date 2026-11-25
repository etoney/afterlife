// Headless checks (run locally and in CI):
//  1) the effects: dry passes untouched, every space has a tail that dies away, the delay lands on time (sync and free)
//  2) the lanes: a REVERB section kills (or adds) reverb on one word only; a DELAY section throws an echo that keeps
//     ringing after the section ends, with its own time; LANES off / transport stopped ignore the sections
//  3) the waveform is drawn where the song played; sections, waveform and loaded IR survive a save + reload
//  4) every factory preset renders, LOADED IR works, user-preset round trip
//  5) renders the editor to PNGs
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static constexpr double SR = 48000.0;
static int failures = 0;
static void check (bool ok, const juce::String& what) { std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); if (! ok) ++failures; }

static bool writePng (const juce::Image& img, const juce::File& f)
{
    f.deleteFile();
    if (auto os = f.createOutputStream()) return juce::PNGImageFormat().writeImageToStream (img, *os);
    return false;
}

// a DAW transport at 120 BPM, 4/4
struct Head final : juce::AudioPlayHead
{
    juce::int64 pos = 0;
    bool playing = true;
    double bpm = 120.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setTimeInSamples (pos); p.setTimeInSeconds ((double) pos / SR); p.setIsPlaying (playing);
        p.setBpm (bpm);
        const double ppq = (double) pos / SR * bpm / 60.0;
        p.setPpqPosition (ppq); p.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
        p.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
        return p;
    }
};

static void setP (AfterlifeProcessor& p, const char* id, float v)
{
    auto* rp = p.apvts.getParameter (id);
    rp->setValueNotifyingHost (rp->convertTo0to1 (v));
}

// plays 'in' through the processor from timeline position 0, returns what comes out
static juce::AudioBuffer<float> render (AfterlifeProcessor& p, Head& head, const juce::AudioBuffer<float>& in, int block = 512)
{
    juce::AudioBuffer<float> out (in.getNumChannels(), in.getNumSamples()), buf (in.getNumChannels(), block);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < in.getNumSamples(); pos += block)
    {
        const int n = juce::jmin (block, in.getNumSamples() - pos);
        buf.setSize (in.getNumChannels(), n, false, false, true);
        for (int c = 0; c < in.getNumChannels(); ++c) buf.copyFrom (c, 0, in, c, pos, n);
        head.pos = pos;
        p.processBlock (buf, midi);
        for (int c = 0; c < in.getNumChannels(); ++c) out.copyFrom (c, pos, buf, c, 0, n);
    }
    return out;
}

static juce::AudioBuffer<float> impulses (double seconds, std::initializer_list<double> at, float amp = 0.8f)
{
    juce::AudioBuffer<float> b (2, (int) (seconds * SR));
    b.clear();
    for (double t : at) { const int i = (int) (t * SR); b.setSample (0, i, amp); b.setSample (1, i, amp); }
    return b;
}

static double energy (const juce::AudioBuffer<float>& b, double t0, double t1)
{
    double e = 0;
    const int a = juce::jmax (0, (int) (t0 * SR)), z = juce::jmin (b.getNumSamples(), (int) (t1 * SR));
    for (int c = 0; c < b.getNumChannels(); ++c) for (int i = a; i < z; ++i) e += (double) b.getSample (c, i) * b.getSample (c, i);
    return e;
}
static double db (double e) { return 10.0 * std::log10 (e + 1.0e-20); }
static float peakIn (const juce::AudioBuffer<float>& b, double t0, double t1, int* where = nullptr)
{
    float m = 0;
    const int a = juce::jmax (0, (int) (t0 * SR)), z = juce::jmin (b.getNumSamples(), (int) (t1 * SR));
    for (int i = a; i < z; ++i) { const float v = std::abs (b.getSample (0, i)); if (v > m) { m = v; if (where) *where = i; } }
    return m;
}

// effects off unless a test turns them on
static void quiet (AfterlifeProcessor& p)
{
    p.setCurrentProgram (0);
    setP (p, "rv_send", 0.0f); setP (p, "dl_send", 0.0f);
}

// a sung phrase for the pictures: words with gaps, a louder line at the end
static juce::AudioBuffer<float> vocal (double seconds)
{
    juce::AudioBuffer<float> b (2, (int) (seconds * SR));
    b.clear();
    juce::Random rnd (7);
    double t = 0.6;
    int k = 0;
    while (t < seconds - 0.5)
    {
        const double len = 0.18 + 0.32 * rnd.nextDouble();
        const float amp = (0.08f + 0.25f * rnd.nextFloat()) * (std::fmod (t, 8.0) > 5.0 ? 1.6f : 1.0f);
        for (int i = (int) (t * SR); i < (int) ((t + len) * SR) && i < b.getNumSamples(); ++i)
        {
            const double u = (i / SR - t) / len;
            const float env = (float) std::sin (juce::MathConstants<double>::pi * u);
            const float s = amp * env * (float) (0.6 * std::sin (6.2831853 * 220.0 * i / SR) + 0.4 * (rnd.nextFloat() * 2.0 - 1.0));
            b.setSample (0, i, s); b.setSample (1, i, s);
        }
        t += len + ((++k % 5 == 0) ? 0.7 : 0.08 + 0.1 * rnd.nextDouble());
    }
    return b;
}

int main (int argc, char** argv)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File dir (argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory());
    dir.createDirectory();

    // ---------------- the effects
    {
        AfterlifeProcessor p; Head head;
        p.setPlayHead (&head);
        p.setPlayConfigDetails (2, 2, SR, 512);
        check (p.getProgramName (p.getCurrentProgram()) == "Init (Chamber + Quarter)", "opens on Init (Chamber + Quarter)");

        // dry passes untouched with both sends at 0
        {
            quiet (p);
            p.prepareToPlay (SR, 512);
            juce::AudioBuffer<float> noise (2, (int) SR);
            juce::Random rnd (3);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < noise.getNumSamples(); ++i) noise.setSample (c, i, rnd.nextFloat() * 0.5f - 0.25f);
            const auto out = render (p, head, noise);
            float worst = 0;
            for (int c = 0; c < 2; ++c) for (int i = 2000; i < noise.getNumSamples(); ++i) worst = juce::jmax (worst, std::abs (out.getSample (c, i) - noise.getSample (c, i)));
            check (worst < 1.0e-5f, juce::String::formatted ("sends at 0: the dry track passes untouched (max difference %.7f)", worst));
            check (p.getLatencySamples() == 0, "no latency");
        }

        // every space has a tail that dies away at its DECAY
        for (int s = 0; s < after::numBuiltInSpaces(); ++s)
        {
            quiet (p);
            setP (p, "dry", 0.0f); setP (p, "rv_send", 100.0f); setP (p, "rv_space", (float) s); setP (p, "rv_decay", 2.0f); setP (p, "rv_predelay", 0.0f);
            setP (p, "dl_on", 0.0f);
            p.prepareToPlay (SR, 512);
            const auto out = render (p, head, impulses (4.0, { 0.1 }));
            const double early = db (energy (out, 0.35, 0.85)), late = db (energy (out, 2.4, 2.9));
            bool finite = true;
            for (int i = 0; i < out.getNumSamples(); ++i) finite = finite && std::isfinite (out.getSample (0, i));
            check (finite && early > -60.0 && early - late > 18.0,
                   juce::String::formatted ("%s: a tail (%.1f dB) that dies away (%.1f dB lower 2 s later)", after::spaces()[(size_t) s].name, early, early - late));
        }

        // a longer DECAY rings longer
        {
            auto tailAt = [&] (float decay)
            {
                quiet (p);
                setP (p, "dry", 0.0f); setP (p, "rv_send", 100.0f); setP (p, "rv_space", 3.0f); setP (p, "rv_decay", decay); setP (p, "dl_on", 0.0f);
                p.prepareToPlay (SR, 512);
                const auto out = render (p, head, impulses (3.5, { 0.1 }));
                return db (energy (out, 2.5, 3.0)) - db (energy (out, 0.3, 0.8));
            };
            const double shortT = tailAt (1.0f), longT = tailAt (6.0f);
            check (longT > shortT + 20.0, juce::String::formatted ("DECAY 6 s rings longer than 1 s (%.1f vs %.1f dB, 2 s on)", longT, shortT));
        }

        // PRE-DELAY: nothing comes out before it
        {
            quiet (p);
            setP (p, "dry", 0.0f); setP (p, "rv_send", 100.0f); setP (p, "rv_space", 5.0f); setP (p, "rv_predelay", 100.0f); setP (p, "dl_on", 0.0f);
            p.prepareToPlay (SR, 512);
            const auto out = render (p, head, impulses (1.0, { 0.1 }));
            check (peakIn (out, 0.1, 0.19) < 1.0e-4f && peakIn (out, 0.2, 0.4) > 1.0e-3f, "PRE-DELAY 100 ms: the reverb starts 100 ms late");
        }

        // the delay lands on time: 1/4 at 120 BPM = 500 ms
        {
            quiet (p);
            setP (p, "dry", 0.0f); setP (p, "rv_on", 0.0f); setP (p, "dl_send", 100.0f); setP (p, "dl_feedback", 0.0f); setP (p, "dl_dirt", 0.0f); setP (p, "dl_tone", 20000.0f); setP (p, "dl_lowcut", 20.0f);
            p.prepareToPlay (SR, 512);
            int at = 0;
            const auto out = render (p, head, impulses (2.0, { 0.25 }));
            peakIn (out, 0.3, 1.9, &at);
            check (std::abs (at - (int) (0.75 * SR)) <= 2, juce::String::formatted ("SYNC 1/4 at 120 BPM: the echo lands 500 ms later (%.2f ms)", (at - 0.25 * SR) / SR * 1000.0));
            setP (p, "dl_div", 7.0f);   // 1/8.
            p.prepareToPlay (SR, 512);
            const auto out2 = render (p, head, impulses (2.0, { 0.25 }));
            peakIn (out2, 0.3, 1.9, &at);
            check (std::abs (at - (int) ((0.25 + 0.375) * SR)) <= 2, juce::String::formatted ("SYNC 1/8. lands 375 ms later (%.2f ms)", (at - 0.25 * SR) / SR * 1000.0));
            setP (p, "dl_sync", 0.0f); setP (p, "dl_ms", 250.0f);
            p.prepareToPlay (SR, 512);
            const auto out3 = render (p, head, impulses (2.0, { 0.25 }));
            peakIn (out3, 0.3, 1.9, &at);
            check (std::abs (at - (int) (0.5 * SR)) <= 2, juce::String::formatted ("FREE 250 ms lands 250 ms later (%.2f ms)", (at - 0.25 * SR) / SR * 1000.0));
            // feedback repeats
            setP (p, "dl_feedback", 50.0f);
            p.prepareToPlay (SR, 512);
            const auto out4 = render (p, head, impulses (2.0, { 0.25 }));
            const float e1 = peakIn (out4, 0.49, 0.51), e2 = peakIn (out4, 0.74, 0.76), e3 = peakIn (out4, 0.99, 1.01);
            check (e2 > 0.35f * e1 && e2 < 0.65f * e1 && e3 < e2, juce::String::formatted ("FEEDBACK 50%%: each repeat about half the last (%.3f, %.3f, %.3f)", e1, e2, e3));
            // ping-pong: the first echo is on the left, the next on the right
            setP (p, "dl_mode", 1.0f);
            p.prepareToPlay (SR, 512);
            const auto out5 = render (p, head, impulses (2.0, { 0.25 }));
            auto chPeak = [&] (int c, double t) { float m = 0; for (int i = (int) ((t - 0.01) * SR); i < (int) ((t + 0.01) * SR); ++i) m = juce::jmax (m, std::abs (out5.getSample (c, i))); return m; };
            check (chPeak (0, 0.5) > 10.0f * chPeak (1, 0.5) && chPeak (1, 0.75) > 10.0f * chPeak (0, 0.75), "PING-PONG: left, then right");
        }
    }

    // ---------------- speed (printed, not checked: CI machines vary)
    {
        AfterlifeProcessor p; Head head;
        p.setPlayHead (&head);
        p.setPlayConfigDetails (2, 2, SR, 512);
        for (int sp : { 0, 3, 7 })
        {
            p.setCurrentProgram (0);
            setP (p, "rv_space", (float) sp); setP (p, "rv_decay", after::spaces()[(size_t) sp].decay);
            p.prepareToPlay (SR, 512);
            const auto in = vocal (10.0);
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            render (p, head, in);
            const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
            std::printf ("[INFO] %s (%.1f s IR): 10 s of audio in %.0f ms (%.1fx real time)\n", after::spaces()[(size_t) sp].name, after::spaces()[(size_t) sp].decay * 1.15, ms, 10000.0 / ms);
        }
    }

    // ---------------- the lanes
    {
        AfterlifeProcessor p; Head head;
        p.setPlayHead (&head);
        p.setPlayConfigDetails (2, 2, SR, 512);

        // REVERB lane: base send 100%, a KILL section around one word
        {
            quiet (p);
            setP (p, "dry", 0.0f); setP (p, "rv_send", 100.0f); setP (p, "rv_space", 5.0f); setP (p, "rv_decay", 0.6f); setP (p, "dl_on", 0.0f); setP (p, "rv_predelay", 0.0f);
            after::Section s; s.start = 1.0; s.end = 1.5; s.send = 0.0f; s.fadeMs = 0.0f;
            const int id = p.addSection (after::kReverbLane, s);
            check (id > 0, "add a REVERB section");
            p.prepareToPlay (SR, 512);
            const auto out = render (p, head, impulses (3.5, { 1.2, 2.5 }));
            const double killed = db (energy (out, 1.2, 1.7)), kept = db (energy (out, 2.5, 3.0));
            check (killed < kept - 60.0, juce::String::formatted ("KILL section: no reverb on the word inside it (%.1f dB) but the next word gets it (%.1f dB)", killed, kept));
            p.clearLane (after::kReverbLane);
        }
        // REVERB lane: base 0, a section adds reverb to one word
        {
            quiet (p);
            setP (p, "dry", 0.0f); setP (p, "rv_send", 0.0f); setP (p, "rv_space", 5.0f); setP (p, "rv_decay", 0.8f); setP (p, "dl_on", 0.0f); setP (p, "rv_predelay", 0.0f);
            after::Section s; s.start = 1.0; s.end = 1.4; s.send = 1.0f;
            p.addSection (after::kReverbLane, s);
            p.prepareToPlay (SR, 512);
            const auto out = render (p, head, impulses (4.0, { 0.5, 1.2, 2.6 }));
            const double before = db (energy (out, 0.5, 0.95)), on = db (energy (out, 1.2, 1.6)), after = db (energy (out, 2.6, 3.0));
            const double ringsOut = db (energy (out, 1.4, 1.6));
            check (on > -40.0 && before < on - 60.0 && after < on - 60.0,
                   juce::String::formatted ("a section adds reverb to its word only (%.1f dB; before %.1f, after %.1f)", on, before, after));
            check (ringsOut > on - 30.0, juce::String::formatted ("and the tail rings out past the section's end (%.1f dB after it ends)", ringsOut));

            // LANES off: the section is ignored
            setP (p, "lanes", 0.0f);
            p.prepareToPlay (SR, 512);
            const auto off = render (p, head, impulses (4.0, { 1.2 }));
            check (db (energy (off, 1.2, 1.6)) < -100.0, "SECTIONS OFF: the section is ignored");
            setP (p, "lanes", 1.0f);
            // transport stopped: sections need the song to be playing
            head.playing = false;
            p.prepareToPlay (SR, 512);
            const auto stopped = render (p, head, impulses (4.0, { 1.2 }));
            check (db (energy (stopped, 1.2, 1.6)) < -100.0, "transport stopped: the knobs rule, sections wait for playback");
            head.playing = true;
            p.clearLane (after::kReverbLane);
        }
        // DELAY lane: a throw on one word, 1/8 (250 ms) while the TIME knob is 1/4, rings out after the section
        {
            quiet (p);
            setP (p, "dry", 0.0f); setP (p, "rv_on", 0.0f); setP (p, "dl_send", 0.0f); setP (p, "dl_div", 9.0f); setP (p, "dl_feedback", 10.0f);
            setP (p, "dl_dirt", 0.0f); setP (p, "dl_tone", 20000.0f); setP (p, "dl_lowcut", 20.0f);
            after::Section s; s.start = 1.0; s.end = 1.3; s.send = 1.0f; s.division = 6; s.feedback = 0.6f; s.fadeMs = 10.0f;
            p.addSection (after::kDelayLane, s);
            p.prepareToPlay (SR, 512);
            const auto out = render (p, head, impulses (4.0, { 1.1, 2.5 }));
            int at = 0;
            const float first = peakIn (out, 1.15, 1.5, &at);
            const float third = peakIn (out, 1.84, 1.86);
            check (std::abs (at - (int) (1.35 * SR)) <= 3, juce::String::formatted ("THROW: the echo uses the section's 1/8, not the knob's 1/4 (%.1f ms after the word)", (at - 1.1 * SR) / SR * 1000.0));
            check (third > 0.25f * first, juce::String::formatted ("THROW rings out with the section's 60%% feedback after the section ends (3rd repeat %.0f%% of the 1st)", 100.0 * third / juce::jmax (1.0e-9f, first)));
            p.prepareToPlay (SR, 512);
            const auto alone = render (p, head, impulses (4.0, { 1.1 }));   // the same, without the next word
            float diff = 0;
            for (int i = (int) (2.5 * SR); i < alone.getNumSamples(); ++i) diff = juce::jmax (diff, std::abs (out.getSample (0, i) - alone.getSample (0, i)));
            check (diff < 1.0e-6f, juce::String::formatted ("the next word, outside the section, gets no echo (only the throw's tail is there: %.8f)", diff));
            p.clearLane (after::kDelayLane);
        }
        // DELAY lane KILL: base send on, a section with send 0
        {
            quiet (p);
            setP (p, "dry", 0.0f); setP (p, "rv_on", 0.0f); setP (p, "dl_send", 100.0f); setP (p, "dl_feedback", 0.0f);
            after::Section s; s.start = 1.0; s.end = 1.3; s.send = 0.0f; s.fadeMs = 5.0f;
            p.addSection (after::kDelayLane, s);
            p.prepareToPlay (SR, 512);
            const auto out = render (p, head, impulses (3.0, { 1.1, 2.0 }));
            check (peakIn (out, 1.55, 1.65) < 1.0e-4f && peakIn (out, 2.45, 2.55) > 0.1f, "DELAY KILL section: no echo for the word inside it, the next word echoes");
            p.clearLane (after::kDelayLane);
        }
        // sections can't overlap, and get clamped between their neighbours
        {
            after::Section a; a.start = 2.0; a.end = 3.0;
            after::Section b; b.start = 2.5; b.end = 4.0;
            after::Section c; c.start = 3.5; c.end = 5.0;
            const int ia = p.addSection (0, a), ib = p.addSection (0, b), ic = p.addSection (0, c);
            check (ia > 0 && ib == 0 && ic > 0, "a section can't start inside another");
            auto moved = *p.getSections().find (0, ic);
            moved.start = 2.6; moved.end = 4.1;   // moved left onto the first
            p.updateSection (0, moved);
            const auto after = p.getSections();
            const auto* m = after.find (0, ic);
            check (m != nullptr && m->start >= 3.0 - 1.0e-9 && std::abs (m->length() - 1.5) < 1.0e-6, "moving a section stops at its neighbour (and keeps its length)");
            p.clearLane (0);
        }
    }

    // ---------------- the waveform, and save / reload
    {
        AfterlifeProcessor p; Head head;
        p.setPlayHead (&head);
        p.setPlayConfigDetails (2, 2, SR, 512);
        p.prepareToPlay (SR, 512);
        juce::AudioBuffer<float> sig (2, (int) (4.0 * SR));
        sig.clear();
        for (int i = (int) (1.0 * SR); i < (int) (3.0 * SR); ++i) { const float s = 0.3f * (float) std::sin (6.2831853 * 300.0 * i / SR); sig.setSample (0, i, s); sig.setSample (1, i, s); }
        render (p, head, sig);
        auto& w = p.getWave();
        int heardLoud = 0, quietBins = 0;
        for (int b = 210; b < 590; ++b) if (after::binHeight (w.at (b)) > 0.7f) ++heardLoud;
        for (int b = 0; b < 190; ++b) if (after::binHeight (w.at (b)) < 0.05f) ++quietBins;
        check (heardLoud == 380 && quietBins == 190 && w.lastBin() >= 790, juce::String::formatted ("the waveform is drawn where it played (%d of 380 loud bins, last bin %d)", heardLoud, w.lastBin()));

        after::Section rs; rs.start = 1.0; rs.end = 1.25; rs.send = 0.8f; rs.fadeMs = 40.0f;
        after::Section ds; ds.start = 2.0; ds.end = 2.5; ds.send = 0.9f; ds.division = 10; ds.feedback = 0.7f;
        p.addSection (0, rs); p.addSection (1, ds);
        setP (p, "rv_decay", 3.3f);
        juce::MemoryBlock state;
        p.getStateInformation (state);

        AfterlifeProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        const auto set = q.getSections();
        const bool secOk = set.lanes[0].size() == 1 && set.lanes[1].size() == 1
                        && std::abs (set.lanes[0][0].send - 0.8f) < 1.0e-6f && std::abs (set.lanes[0][0].fadeMs - 40.0f) < 1.0e-4f
                        && set.lanes[1][0].division == 10 && std::abs (set.lanes[1][0].feedback - 0.7f) < 1.0e-6f && std::abs (set.lanes[1][0].end - 2.5) < 1.0e-9;
        check (secOk, "sections survive a save + reload");
        int same = 0;
        for (int b = 0; b <= w.lastBin(); ++b) if (q.getWave().at (b) == w.at (b)) ++same;
        check (same == w.lastBin() + 1 && q.getWave().lastBin() == w.lastBin(), "the waveform survives a save + reload (" + juce::String (state.getSize()) + " bytes of state)");
        check (std::abs (q.apvts.getRawParameterValue ("rv_decay")->load() - 3.3f) < 0.01f, "the knobs survive a save + reload");
        check (q.getTailLengthSeconds() > 3.3, "tail length reported to the DAW covers DECAY (" + juce::String (q.getTailLengthSeconds(), 2) + " s)");
    }

    // ---------------- presets, mono, LOADED IR, user presets
    {
        AfterlifeProcessor p; Head head;
        p.setPlayHead (&head);
        p.setPlayConfigDetails (2, 2, SR, 512);
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            p.prepareToPlay (SR, 512);
            const auto out = render (p, head, vocal (3.0));
            float pk = 0; bool finite = true;
            for (int c = 0; c < 2; ++c) for (int k = 0; k < out.getNumSamples(); ++k) { finite = finite && std::isfinite (out.getSample (c, k)); pk = juce::jmax (pk, std::abs (out.getSample (c, k))); }
            check (finite && pk < 4.0f && pk > 0.01f, "preset '" + p.getProgramName (i) + "' renders (peak " + juce::String (pk, 2) + ")");
        }
        // mono in, mono out
        {
            AfterlifeProcessor m; Head h2;
            m.setPlayHead (&h2);
            m.setPlayConfigDetails (1, 1, SR, 512);
            m.prepareToPlay (SR, 512);
            juce::AudioBuffer<float> in (1, (int) SR); in.clear(); in.setSample (0, 1000, 0.8f);
            juce::AudioBuffer<float> buf (1, 512); juce::MidiBuffer midi;
            float pk = 0;
            for (int pos = 0; pos < in.getNumSamples(); pos += 512)
            {
                buf.copyFrom (0, 0, in, 0, pos, juce::jmin (512, in.getNumSamples() - pos));
                h2.pos = pos; m.processBlock (buf, midi);
                if (pos > 2000) pk = juce::jmax (pk, buf.getMagnitude (0, 0, 512));
            }
            check (std::isfinite (pk) && pk > 1.0e-4f, "mono track: reverb comes out");
        }
        // LOADED IR: write a little IR to disk and load it
        {
            const auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("afterlife_test_ir.wav");
            {
                juce::AudioBuffer<float> ir (2, (int) (0.8 * 44100.0));
                juce::Random rnd (5);
                for (int c = 0; c < 2; ++c) for (int i = 0; i < ir.getNumSamples(); ++i) ir.setSample (c, i, (rnd.nextFloat() * 2.0f - 1.0f) * std::exp (-6.9f * (float) i / (0.6f * 44100.0f)));
                f.deleteFile();
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> wr (wav.createWriterFor (new juce::FileOutputStream (f), 44100.0, 2, 24, {}, 0));
                wr->writeFromAudioSampleBuffer (ir, 0, ir.getNumSamples());
            }
            juce::String err;
            check (p.loadIrFile (f, err), "load an IR file (" + err + ")");
            check (juce::roundToInt (p.apvts.getRawParameterValue ("rv_space")->load()) == after::loadedIrIndex(), "loading an IR switches SPACE to LOADED IR");
            p.setCurrentProgram (0);
            setP (p, "rv_space", (float) after::loadedIrIndex());
            setP (p, "dry", 0.0f); setP (p, "rv_send", 100.0f); setP (p, "dl_on", 0.0f); setP (p, "rv_decay", 10.0f); setP (p, "rv_predelay", 0.0f);
            p.prepareToPlay (SR, 512);
            const auto out = render (p, head, impulses (2.0, { 0.2 }));
            const auto v = p.getEngine().irView();
            check (db (energy (out, 0.25, 0.6)) > -40.0 && db (energy (out, 1.3, 1.9)) < -90.0 && v != nullptr && v->name == "afterlife_test_ir",
                   "LOADED IR: the file's own tail (resampled from 44.1 kHz), and it ends where the file ends");
            juce::MemoryBlock st; p.getStateInformation (st);
            AfterlifeProcessor q; q.setStateInformation (st.getData(), (int) st.getSize());
            check (q.getIrFile() == f && q.getEngine().getLoadedIr() != nullptr, "the loaded IR is reloaded with the song");
            f.deleteFile();
        }
        // user presets: save -> change -> load -> delete
        {
            p.setCurrentProgram (0);
            setP (p, "rv_decay", 4.4f); setP (p, "dl_feedback", 72.0f);
            check (p.saveUserPreset ("__test after"), "save user preset");
            const auto f = AfterlifeProcessor::fileForPresetName ("__test after");
            check (f.existsAsFile() && f.getFullPathName().contains ("Castle Danger"), "preset file in the Castle Danger folder: " + f.getFullPathName());
            p.setCurrentProgram (0);
            check (p.loadUserPreset (f) && std::abs (p.apvts.getRawParameterValue ("rv_decay")->load() - 4.4f) < 0.01f
                   && std::abs (p.apvts.getRawParameterValue ("dl_feedback")->load() - 72.0f) < 0.01f, "load user preset");
            f.deleteFile();
        }
    }

    // ---------------- GUI
    {
        AfterlifeProcessor p; Head head;
        p.setPlayHead (&head);
        p.setPlayConfigDetails (2, 2, SR, 512);
        p.setCurrentProgram (2);   // Vocal: Throws Only
        setP (p, "rv_send", 18.0f);
        p.prepareToPlay (SR, 512);
        // a vocal, played through once: the waveform gets drawn
        const auto v = vocal (22.0);
        auto rs = [] (double a, double b, float send, float fade = 25.0f) { after::Section s; s.start = a; s.end = b; s.send = send; s.fadeMs = fade; return s; };
        p.addSection (0, rs (3.2, 4.6, 0.85f, 120.0f));
        p.addSection (0, rs (7.9, 8.6, 0.0f));
        p.addSection (0, rs (12.0, 14.5, 0.65f, 200.0f));
        auto d1 = rs (5.25, 5.75, 1.0f); d1.division = 10; d1.feedback = 0.55f;
        auto d2 = rs (9.5, 10.0, 0.8f); d2.division = 6; d2.feedback = 0.4f;
        auto d3 = rs (14.75, 15.5, 0.9f); d3.division = -1; d3.feedback = 0.7f;
        p.addSection (1, d1); const int sel = p.addSection (1, d2); p.addSection (1, d3);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* ae = dynamic_cast<AfterlifeEditor*> (ed.get());
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
        const int stopAt = (int) (10.2 * SR);
        for (int pos = 0; pos < v.getNumSamples(); pos += 512)
        {
            buf.copyFrom (0, 0, v, 0, pos, 512); buf.copyFrom (1, 0, v, 1, pos, 512);
            head.pos = pos; p.processBlock (buf, midi);
            if (pos >= stopAt && pos < stopAt + 512 * 40 && (pos / 512) % 4 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
            if (pos >= stopAt + 512 * 40) break;
        }
        // the playhead at 10.6 s, in the middle of a throw; view the first 16 s
        ae->getLanes().setFollow (false);
        ae->getLanes().setView (0.0, 16.5);
        ae->getLanes().select (1, sel);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        check (writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f), dir.getChildFile ("gui.png")), "main snapshot");

        ae->getLanes().select (-1, 0);
        ae->getLanes().setView (2.5, 4.0);   // zoomed in to the words
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        check (writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), dir.getChildFile ("gui_zoom.png")), "zoomed-in snapshot");

        ae->getSaveButton().triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        check (writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), dir.getChildFile ("gui_save.png")), "save dialog snapshot");
    }
    // ARA: the whole clip drawn at once, before playing anything
    {
        AfterlifeProcessor p; Head head;
        p.setPlayHead (&head);
        p.setPlayConfigDetails (2, 2, SR, 512);
        p.setCurrentProgram (3);
        p.prepareToPlay (SR, 512);
        const auto v = vocal (40.0);
        std::vector<juce::uint8> bins ((size_t) (40.0 / after::kBinSeconds), 0);
        for (size_t b = 0; b < bins.size(); ++b)
        {
            float m = 0;
            const int a = (int) (b * after::kBinSeconds * SR);
            for (int i = a; i < a + (int) (after::kBinSeconds * SR) && i < v.getNumSamples(); ++i) m = juce::jmax (m, std::abs (v.getSample (0, i)));
            bins[b] = (b * after::kBinSeconds >= 4.0) ? after::binValue (m) : 0;   // the clip starts at 0:04
        }
        p.setAraWaveForTest (std::move (bins), 1);
        after::Section s; s.start = 18.0; s.end = 21.0; s.send = 0.9f; s.fadeMs = 300.0f;
        p.addSection (0, s);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        dynamic_cast<AfterlifeEditor*> (ed.get())->getLanes().setFollow (false);
        dynamic_cast<AfterlifeEditor*> (ed.get())->getLanes().setView (0.0, 40.0);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        check (writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), dir.getChildFile ("gui_ara.png")), "ARA snapshot: the whole clip drawn at once");
    }

    std::printf ("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
