#pragma once
// AFTERLIFE - ARA MODE
// Applied to the clip itself (Studio Pro: drag Afterlife onto the clip; Logic, Cubase, Reaper: as an ARA
// extension), Afterlife reads each clip as soon as the DAW lets it and draws the whole waveform in the lanes
// straight away: no playing the song through first. Playback reads the clip, and the reverb and delay (with
// their lanes) run on it exactly as they do on an insert. Tails ring past the end of the clip as normal.
//
//   AfterDocumentController  one per song: reads each audio source on a background thread (its waveform)
//   AfterRenderer            one per Afterlife instance: plays its clips back, and says where they sit on
//                            the timeline so the lanes can draw them
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Waveform.h"

#if JucePlugin_Enable_ARA
#include <map>
#include <memory>
#include <mutex>
#include <set>

namespace after
{
// a clip's waveform: one bin per 5 ms of the source
struct SourceWave { std::vector<juce::uint8> bins; double sampleRate = 48000.0; juce::int64 samples = 0; };

class AfterDocumentController;
class AfterRenderer;

class AfterAnalysisJob : public juce::ThreadPoolJob
{
public:
    AfterAnalysisJob (AfterDocumentController& dcIn, juce::ARAAudioSource* src)
        : juce::ThreadPoolJob ("Afterlife waveform"), dc (dcIn), source (src),
          reader (std::make_unique<juce::ARAAudioSourceReader> (src)),
          channels (juce::jmax (1, (int) src->getChannelCount())), length (src->getSampleCount()), rate (src->getSampleRate())
    {}
    JobStatus runJob() override;
private:
    AfterDocumentController& dc;
    juce::ARAAudioSource* source;
    std::unique_ptr<juce::ARAAudioSourceReader> reader;
    int channels;
    juce::int64 length;
    double rate;
};

//==============================================================================
class AfterDocumentController final : public juce::ARADocumentControllerSpecialisation
{
public:
    using juce::ARADocumentControllerSpecialisation::ARADocumentControllerSpecialisation;
    ~AfterDocumentController() override { pool.removeAllJobs (true, 4000); }

    std::shared_ptr<const SourceWave> waveFor (const juce::ARAAudioSource* src) const
    {
        const std::lock_guard<std::mutex> l (lock);
        const auto it = waves.find (src);
        return it == waves.end() ? nullptr : it->second;
    }
    std::atomic<int> waveVersion { 0 };

    // the renderers (one per Afterlife on a clip), so a clip's reader can be renewed when the DAW lets us read it
    void addRenderer (AfterRenderer* r) { const std::lock_guard<std::mutex> l (lock); renderers.insert (r); }
    void removeRenderer (AfterRenderer* r) { const std::lock_guard<std::mutex> l (lock); renderers.erase (r); }

    void waveDone (juce::ARAAudioSource* src, SourceWave w)
    {
        {
            const std::lock_guard<std::mutex> l (lock);
            if (live.count (src) == 0) return;
            waves[src] = std::make_shared<const SourceWave> (std::move (w));
        }
        ++waveVersion;
    }

protected:
    juce::ARAAudioSource* doCreateAudioSource (juce::ARADocument* document, ARA::ARAAudioSourceHostRef hostRef) noexcept override
    {
        auto* src = new juce::ARAAudioSource (document, hostRef);
        src->addListener (&watcher);
        { const std::lock_guard<std::mutex> l (lock); live.insert (src); }
        return src;
    }
    juce::ARAPlaybackRenderer* doCreatePlaybackRenderer() noexcept override;

    // nothing of our own in the song: the waveform is read again from the audio; the sections live in each
    // plugin's own state
    bool doRestoreObjectsFromStream (juce::ARAInputStream& input, const juce::ARARestoreObjectsFilter*) noexcept override { return ! input.failed(); }
    bool doStoreObjectsToStream (juce::ARAOutputStream&, const juce::ARAStoreObjectsFilter*) noexcept override { return true; }

private:
    struct Watcher : juce::ARAAudioSource::Listener
    {
        explicit Watcher (AfterDocumentController& d) : dc (d) {}
        void didEnableAudioSourceSamplesAccess (juce::ARAAudioSource* s, bool enable) override { dc.accessChanged (s, enable); }
        void doUpdateAudioSourceContent (juce::ARAAudioSource* s, juce::ARAContentUpdateScopes scopes) override { dc.contentChanged (s, scopes); }
        void willDestroyAudioSource (juce::ARAAudioSource* s) override { dc.sourceGoing (s); }
        AfterDocumentController& dc;
    };
    Watcher watcher { *this };

    void accessChanged (juce::ARAAudioSource* src, bool enable);
    void accessChangedImpl (juce::ARAAudioSource* src, bool enable)
    {
        if (! enable) { cancel (src); return; }
        if (waveFor (src) == nullptr) start (src);
    }
    void contentChanged (juce::ARAAudioSource* src, juce::ARAContentUpdateScopes scopes)
    {
        if (! scopes.affectSamples()) return;
        cancel (src);
        { const std::lock_guard<std::mutex> l (lock); waves.erase (src); }
        ++waveVersion;
        if (src->isSampleAccessEnabled()) start (src);
    }
    void sourceGoing (juce::ARAAudioSource* src)
    {
        cancel (src);
        src->removeListener (&watcher);
        { const std::lock_guard<std::mutex> l (lock); live.erase (src); waves.erase (src); }
        ++waveVersion;
    }
    void start (juce::ARAAudioSource* src)
    {
        cancel (src);
        auto* job = new AfterAnalysisJob (*this, src);
        jobs[src] = job;
        pool.addJob (job, true);
    }
    void cancel (juce::ARAAudioSource* src)
    {
        if (auto it = jobs.find (src); it != jobs.end()) { pool.removeJob (it->second, true, 4000); jobs.erase (it); }
    }

    juce::ThreadPool pool { juce::ThreadPoolOptions{}.withThreadName ("Afterlife ARA").withNumberOfThreads (1) };
    std::map<juce::ARAAudioSource*, AfterAnalysisJob*> jobs;   // document thread only
    mutable std::mutex lock;
    std::map<const juce::ARAAudioSource*, std::shared_ptr<const SourceWave>> waves;
    std::set<const juce::ARAAudioSource*> live;
    std::set<AfterRenderer*> renderers;
};

inline juce::ThreadPoolJob::JobStatus AfterAnalysisJob::runJob()
{
    if (source != nullptr) source->notifyAnalysisProgressStarted();
    SourceWave w;
    w.sampleRate = rate; w.samples = length;
    const juce::int64 perBin = juce::jmax ((juce::int64) 1, (juce::int64) std::llround (kBinSeconds * rate));
    w.bins.reserve ((size_t) (length / perBin + 1));
    const int block = 32768;
    juce::AudioBuffer<float> buf (channels, block);
    float peak = 0; juce::int64 inBin = 0;
    for (juce::int64 pos = 0; pos < length; pos += block)
    {
        if (shouldExit()) return jobHasFinished;
        const int n = (int) juce::jmin ((juce::int64) block, length - pos);
        bool ok = false;
        for (int tries = 0; tries < 50 && ! ok && ! shouldExit(); ++tries)
        {
            ok = reader->isValid() && reader->read (&buf, 0, n, pos, true, true);
            if (! ok) juce::Thread::sleep (20);
        }
        if (! ok) return jobHasFinished;
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < channels; ++c) peak = std::max (peak, std::abs (buf.getSample (c, i)));
            if (++inBin >= perBin) { w.bins.push_back (binValue (peak)); peak = 0; inBin = 0; }
        }
        if (source != nullptr && (pos / block) % 16 == 0) source->notifyAnalysisProgressUpdated ((float) ((double) pos / (double) juce::jmax ((juce::int64) 1, length)));
    }
    if (inBin > 0) w.bins.push_back (binValue (peak));
    if (source != nullptr) source->notifyAnalysisProgressCompleted();
    dc.waveDone (source, std::move (w));
    return jobHasFinished;
}

//==============================================================================
// Plays the clips back (the effects run on top, in the processor)
class AfterRenderer final : public juce::ARAPlaybackRenderer
{
public:
    AfterRenderer (ARA::PlugIn::DocumentController* d, AfterDocumentController& dcIn) : juce::ARAPlaybackRenderer (d), dc (dcIn) { dc.addRenderer (this); }
    ~AfterRenderer() override { dc.removeRenderer (this); }

    void prepareToPlay (double sampleRateIn, int maxBlock, int numChannelsIn, juce::AudioProcessor::ProcessingPrecision,
                        AlwaysNonRealtime alwaysNonRealtime) override
    {
        sampleRate = sampleRateIn; numChannels = numChannelsIn; maxBlockSize = maxBlock;
        buffered = alwaysNonRealtime == AlwaysNonRealtime::no;
        std::map<juce::ARAAudioSource*, Reader> fresh;
        for (auto* region : getPlaybackRegions())
        {
            auto* src = region->getAudioModification()->getAudioSource();
            if (fresh.count (src) == 0) fresh[src] = makeReader (src);
        }
        sourceBuffer.setSize (8, maxBlock);
        mixBuffer.setSize (juce::jmax (1, numChannels), maxBlock);
        const juce::SpinLock::ScopedLockType l (readerLock);
        std::swap (readers, fresh);
    }

    // the DAW just let us read a clip: a reader made before that (and its read-ahead buffer) only has silence,
    // so make a new one (document thread; the audio thread is only ever kept waiting for the swap)
    void sourceAccessEnabled (juce::ARAAudioSource* src)
    {
        {
            const juce::SpinLock::ScopedLockType l (readerLock);
            if (readers.count (src) == 0) return;
        }
        auto r = makeReader (src);
        const juce::SpinLock::ScopedLockType l (readerLock);
        if (auto it = readers.find (src); it != readers.end()) std::swap (it->second, r);
    }

    void releaseResources() override { std::map<juce::ARAAudioSource*, Reader> gone; const juce::SpinLock::ScopedLockType l (readerLock); std::swap (readers, gone); }

    bool processBlock (juce::AudioBuffer<float>& buffer, juce::AudioProcessor::Realtime realtime,
                       const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept override
    {
        const int numSamples = buffer.getNumSamples();
        const auto blockStart = positionInfo.getTimeInSamples().orFallback (0);
        bool success = true, any = false;
        const juce::SpinLock::ScopedTryLockType rl (readerLock);
        if (positionInfo.getIsPlaying() && rl.isLocked())
        {
            const auto blockRange = juce::Range<juce::int64>::withStartAndLength (blockStart, numSamples);
            for (auto* region : getPlaybackRegions())
            {
                const auto playRange = region->getSampleRange (sampleRate, juce::ARAPlaybackRegion::IncludeHeadAndTail::no);
                auto range = blockRange.getIntersectionWith (playRange);
                if (range.isEmpty()) continue;
                const juce::Range<juce::int64> modRange { region->getStartInAudioModificationSamples(), region->getEndInAudioModificationSamples() };
                const auto offset = modRange.getStart() - playRange.getStart();
                range = range.getIntersectionWith (modRange.movedToStartAt (playRange.getStart()));
                if (range.isEmpty()) continue;

                auto* src = region->getAudioModification()->getAudioSource();
                auto it = readers.find (src);
                if (it == readers.end() || std::abs (src->getSampleRate() - sampleRate) > 0.5) { success = false; continue; }
                if (it->second.buffered) static_cast<juce::BufferingAudioReader*> (it->second.reader.get())->setReadTimeout (realtime == juce::AudioProcessor::Realtime::no ? 100 : 0);

                const int n = (int) range.getLength(), at = (int) (range.getStart() - blockStart);
                const auto srcStart = range.getStart() + offset;
                const int srcCh = juce::jlimit (1, 8, (int) src->getChannelCount());
                if (! it->second.reader->read (&sourceBuffer, 0, n, srcStart, true, true)) {  success = false; continue; }
                if (! any) { mixBuffer.clear(); any = true; }
                for (int c = 0; c < numChannels; ++c)
                    for (int i = 0; i < n; ++i)
                    {
                        float x;
                        if (srcCh == numChannels) x = sourceBuffer.getSample (c, i);
                        else if (srcCh == 1) x = sourceBuffer.getSample (0, i);
                        else { x = 0; for (int k = 0; k < srcCh; ++k) x += sourceBuffer.getSample (k, i); x /= (float) srcCh; }
                        mixBuffer.addSample (c, at + i, x);
                    }
            }
        }
        buffer.clear();
        if (any) for (int c = 0; c < numChannels; ++c) buffer.copyFrom (c, 0, mixBuffer, c, 0, numSamples);
        return success;
    }
    using juce::ARAPlaybackRenderer::processBlock;

    // ---- for the lanes (message thread): where each clip sits on the timeline, and its waveform
    struct Placed { std::shared_ptr<const SourceWave> wave; double playStart = 0, playEnd = 0, srcStart = 0; };
    std::vector<Placed> placedClips() const
    {
        std::vector<Placed> out;
        for (auto* region : getPlaybackRegions())
        {
            Placed p;
            p.wave = dc.waveFor (region->getAudioModification()->getAudioSource());
            p.playStart = region->getStartInPlaybackTime();
            p.playEnd = region->getEndInPlaybackTime();
            p.srcStart = region->getStartInAudioModificationTime();
            out.push_back (std::move (p));
        }
        return out;
    }
    int numClips() const { return (int) getPlaybackRegions().size(); }
    AfterDocumentController& getDocumentController() const { return dc; }

private:
    struct Reader { std::unique_ptr<juce::AudioFormatReader> reader; bool buffered = false; };
    Reader makeReader (juce::ARAAudioSource* src)
    {
        auto r = std::make_unique<juce::ARAAudioSourceReader> (src);
        if (buffered)
            return { std::make_unique<juce::BufferingAudioReader> (r.release(), *timeSlice, juce::jmax (4 * maxBlockSize, juce::roundToInt (2.0 * sampleRate))), true };
        return { std::move (r), false };
    }
    struct SharedThread : juce::TimeSliceThread { SharedThread() : juce::TimeSliceThread ("Afterlife reader") { startThread(); } };

    AfterDocumentController& dc;
    juce::SharedResourcePointer<SharedThread> timeSlice;
    std::map<juce::ARAAudioSource*, Reader> readers;
    juce::AudioBuffer<float> sourceBuffer, mixBuffer;
    double sampleRate = 48000.0;
    int numChannels = 2, maxBlockSize = 512;
    bool buffered = true;
    juce::SpinLock readerLock;
};

inline void AfterDocumentController::accessChanged (juce::ARAAudioSource* src, bool enable)
{
    accessChangedImpl (src, enable);
    if (! enable) return;
    std::vector<AfterRenderer*> rs;
    { const std::lock_guard<std::mutex> l (lock); rs.assign (renderers.begin(), renderers.end()); }
    for (auto* r : rs) r->sourceAccessEnabled (src);
}

inline juce::ARAPlaybackRenderer* AfterDocumentController::doCreatePlaybackRenderer() noexcept
{
    return new AfterRenderer (getDocumentController(), *this);
}
} // namespace after
#endif
