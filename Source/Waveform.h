#pragma once
// AFTERLIFE - THE WAVEFORM BEHIND THE LANES
// The track's level along the DAW's timeline, one value per 5 ms "bin" (0 = not heard yet, 1..255 = -60..0 dB
// peak). As an insert, Afterlife can only hear what the DAW plays through it, so the waveform is drawn as the
// song plays (and saved with the song). With ARA it's read straight from the clips instead.
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>
#include <vector>

namespace after
{
constexpr double kBinSeconds = 0.005;
constexpr int kMaxBins = (int) (3600.0 / kBinSeconds);   // one hour of timeline

inline juce::uint8 binValue (float peak)
{
    const float db = juce::Decibels::gainToDecibels (peak, -60.0f);
    return (juce::uint8) (1 + juce::jlimit (0, 254, juce::roundToInt ((db + 60.0f) / 60.0f * 254.0f)));
}
// 0..1 for drawing (a log scale, like a DAW's "log waveform"), -1 = not heard yet
inline float binHeight (juce::uint8 v) { return v == 0 ? -1.0f : (float) (v - 1) / 254.0f; }

class TimelineWave
{
public:
    TimelineWave() : bins (new std::atomic<juce::uint8>[(size_t) kMaxBins]) { clear(); }

    void clear()
    {
        for (int i = 0; i < kMaxBins; ++i) bins[(size_t) i].store (0, std::memory_order_relaxed);
        highest.store (-1); resetAudio.store (true); ++version;
    }

    // audio thread: what's playing, at timeline seconds t0
    void capture (const float* const* ch, int numCh, int n, double t0, double sr)
    {
        if (numCh <= 0 || n <= 0 || sr <= 0) return;
        if (resetAudio.exchange (false)) { curBin = -1; curPeak = 0; }
        const double expected = curBin >= 0 ? lastEnd : -1.0;
        if (curBin >= 0 && std::abs (t0 - expected) > 0.02) flush();   // the playhead jumped
        for (int i = 0; i < n; ++i)
        {
            const double t = t0 + (double) i / sr;
            if (t < 0) continue;
            const int b = (int) (t / kBinSeconds);
            if (b >= kMaxBins) break;
            if (b != curBin) { flush(); curBin = b; curPeak = 0; }
            float m = 0;
            for (int c = 0; c < numCh; ++c) m = std::max (m, std::abs (ch[c][i]));
            curPeak = std::max (curPeak, m);
        }
        lastEnd = t0 + (double) n / sr;
        // show the bin in progress too (it's rewritten when it finishes)
        if (curBin >= 0)
        {
            bins[(size_t) curBin].store (binValue (curPeak), std::memory_order_relaxed);
            if (curBin > highest.load (std::memory_order_relaxed)) highest.store (curBin, std::memory_order_relaxed);
        }
    }
    void flush()
    {
        if (curBin >= 0 && curBin < kMaxBins)
        {
            bins[(size_t) curBin].store (binValue (curPeak), std::memory_order_relaxed);
            if (curBin > highest.load (std::memory_order_relaxed)) highest.store (curBin, std::memory_order_relaxed);
            ++version;
        }
        curBin = -1; curPeak = 0;
    }

    juce::uint8 at (int bin) const { return juce::isPositiveAndBelow (bin, kMaxBins) ? bins[(size_t) bin].load (std::memory_order_relaxed) : (juce::uint8) 0; }
    int lastBin() const { return highest.load(); }
    std::atomic<int> version { 0 };

    // ---- saved with the song: the bins up to the last one heard, gzipped
    juce::String toBase64() const
    {
        const int last = lastBin();
        if (last < 0) return {};
        juce::MemoryOutputStream raw;
        for (int i = 0; i <= last; ++i) raw.writeByte ((char) at (i));
        juce::MemoryOutputStream zipped;
        {
            juce::GZIPCompressorOutputStream gz (zipped, 9);
            gz.write (raw.getData(), raw.getDataSize());
        }
        return zipped.getMemoryBlock().toBase64Encoding();
    }
    void fromBase64 (const juce::String& s)
    {
        clear();
        if (s.isEmpty()) return;
        juce::MemoryBlock mb;
        if (! mb.fromBase64Encoding (s)) return;
        juce::MemoryInputStream in (mb, false);
        juce::GZIPDecompressorInputStream gz (in);
        juce::MemoryBlock out;
        gz.readIntoMemoryBlock (out, kMaxBins);
        const int n = std::min ((int) out.getSize(), kMaxBins);
        for (int i = 0; i < n; ++i) bins[(size_t) i].store ((juce::uint8) out[i], std::memory_order_relaxed);
        int hi = -1;
        for (int i = n - 1; i >= 0; --i) if (bins[(size_t) i].load() != 0) { hi = i; break; }
        highest.store (hi);
        ++version;
    }

private:
    std::unique_ptr<std::atomic<juce::uint8>[]> bins;
    std::atomic<int> highest { -1 };
    std::atomic<bool> resetAudio { false };
    // audio thread only
    int curBin = -1;
    float curPeak = 0;
    double lastEnd = 0;
};
} // namespace after
