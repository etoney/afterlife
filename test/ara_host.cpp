// AFTERLIFE - ARA HOST TEST
// A tiny ARA host: loads the built Afterlife VST3, applies it to a clip (an in-memory vocal take placed 2 s into
// the song), lets it read the clip, then plays the song through it and checks: the clip comes out on time and
// untouched with the sends at 0, and with the reverb up the tail rings on past the end of the clip.
// This is what Studio Pro does when you drag Afterlife onto a clip, minus the screen.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <ARA_API/ARAInterface.h>
#include <ARA_Library/Dispatch/ARAHostDispatch.h>
#include <cstdio>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what) { std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); if (! ok) ++failures; }
static constexpr double SR = 48000.0;

struct MemorySource
{
    AudioBuffer<float> audio;
    std::unique_ptr<ARAHostModel::AudioSource> source;
    using Converter = ARAHostModel::ConversionFunctions<MemorySource*, ARA::ARAAudioSourceHostRef>;
    bool read (float* const* buffers, int64 start, int64 n)
    {
        for (int c = 0; c < audio.getNumChannels(); ++c)
            for (int64 i = 0; i < n; ++i)
                buffers[c][i] = (start + i >= 0 && start + i < audio.getNumSamples()) ? audio.getSample (c, (int) (start + i)) : 0.0f;
        return true;
    }
};

class AudioAccess final : public ARA::Host::AudioAccessControllerInterface
{
public:
    struct Reader { ARA::ARAAudioSourceHostRef src; };
    using RC = ARAHostModel::ConversionFunctions<Reader*, ARA::ARAAudioReaderHostRef>;
    ARA::ARAAudioReaderHostRef createAudioReaderForSource (ARA::ARAAudioSourceHostRef s, bool use64) noexcept override
    {
        if (use64) return nullptr;
        auto r = std::make_unique<Reader> (Reader { s });
        auto ref = RC::toHostRef (r.get());
        const std::lock_guard<std::mutex> l (lock);
        readers.emplace (r.get(), std::move (r));
        return ref;
    }
    bool readAudioSamples (ARA::ARAAudioReaderHostRef r, ARA::ARASamplePosition pos, ARA::ARASampleCount n, void* const* buffers) noexcept override
    {
        ++reads;
        for (auto e = (int64) (pos + n), m = furthest.load(); e > m && ! furthest.compare_exchange_weak (m, e);) {}
        return MemorySource::Converter::fromHostRef (RC::fromHostRef (r)->src)->read (reinterpret_cast<float* const*> (buffers), pos, n);
    }
    void destroyAudioReader (ARA::ARAAudioReaderHostRef r) noexcept override { const std::lock_guard<std::mutex> l (lock); readers.erase (RC::fromHostRef (r)); }
    std::atomic<int> reads { 0 };
    std::atomic<int64> furthest { 0 };   // the furthest sample read so far
private:
    std::mutex lock;
    std::map<Reader*, std::unique_ptr<Reader>> readers;
};

class Archiving final : public ARA::Host::ArchivingControllerInterface
{
public:
    ARA::ARASize getArchiveSize (ARA::ARAArchiveReaderHostRef) noexcept override { return 0; }
    bool readBytesFromArchive (ARA::ARAArchiveReaderHostRef, ARA::ARASize, ARA::ARASize, ARA::ARAByte*) noexcept override { return false; }
    bool writeBytesToArchive (ARA::ARAArchiveWriterHostRef, ARA::ARASize, ARA::ARASize, const ARA::ARAByte*) noexcept override { return true; }
    void notifyDocumentArchivingProgress (float) noexcept override {}
    void notifyDocumentUnarchivingProgress (float) noexcept override {}
    ARA::ARAPersistentID getDocumentArchiveID (ARA::ARAArchiveReaderHostRef) noexcept override { return nullptr; }
};

class ModelUpdates final : public ARA::Host::ModelUpdateControllerInterface
{
public:
    void notifyAudioSourceAnalysisProgress (ARA::ARAAudioSourceHostRef, ARA::ARAAnalysisProgressState state, float) noexcept override
    {
        if (state == ARA::kARAAnalysisProgressStarted) started = true;
        if (state == ARA::kARAAnalysisProgressCompleted) completed = true;
    }
    void notifyAudioSourceContentChanged (ARA::ARAAudioSourceHostRef, const ARA::ARAContentTimeRange*, ARA::ContentUpdateScopes) noexcept override {}
    void notifyAudioModificationContentChanged (ARA::ARAAudioModificationHostRef, const ARA::ARAContentTimeRange*, ARA::ContentUpdateScopes) noexcept override {}
    void notifyPlaybackRegionContentChanged (ARA::ARAPlaybackRegionHostRef, const ARA::ARAContentTimeRange*, ARA::ContentUpdateScopes scopes) noexcept override
    {
        if (scopes.affectSamples()) regionChanged = true;
    }
    void notifyDocumentDataChanged() noexcept override {}
    bool started = false, completed = false, regionChanged = false;
};

class Playback final : public ARA::Host::PlaybackControllerInterface
{
public:
    void requestStartPlayback() noexcept override {}
    void requestStopPlayback() noexcept override {}
    void requestSetPlaybackPosition (ARA::ARATimePosition) noexcept override {}
    void requestSetCycleRange (ARA::ARATimePosition, ARA::ARATimeDuration) noexcept override {}
    void requestEnableCycle (bool) noexcept override {}
};

struct Head final : AudioPlayHead
{
    Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setTimeInSamples (pos); p.setIsPlaying (true); p.setTimeInSeconds ((double) pos / SR); return p; }
    int64 pos = 0;
};

static double energy (const AudioBuffer<float>& b, int a, int e)
{
    double s = 0; for (int i = jmax (0, a); i < jmin (e, b.getNumSamples()); ++i) s += (double) b.getSample (0, i) * b.getSample (0, i);
    return 10.0 * std::log10 (s + 1.0e-20);
}

int main (int argc, char** argv)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("usage: AfterlifeARAHost /path/to/Afterlife.vst3\n"); return 2; }

    VST3PluginFormat fmt;
    OwnedArray<PluginDescription> descs;
    fmt.findAllTypesForFile (descs, String (argv[1]));
    check (! descs.isEmpty(), "found Afterlife in " + String (argv[1]));
    if (descs.isEmpty()) return 1;
    check (descs[0]->hasARAExtension, "Afterlife says it has an ARA extension");

    String err;
    auto plugin = fmt.createInstanceFromDescription (*descs[0], SR, 512, err);
    check (plugin != nullptr, "loaded (" + err + ")");
    if (plugin == nullptr) return 1;

    ARAFactoryWrapper factory;
    bool gotFactory = false;
    createARAFactoryAsync (*plugin, [&] (ARAFactoryWrapper f) { factory = std::move (f); gotFactory = true; });
    for (int i = 0; i < 100 && ! gotFactory; ++i) MessageManager::getInstance()->runDispatchLoopUntil (20);
    check (factory.get() != nullptr, "got its ARA factory");
    if (factory.get() == nullptr) return 1;

    auto audioAccess = std::make_unique<AudioAccess>();
    auto* access = audioAccess.get();
    auto modelUpdates = std::make_unique<ModelUpdates>();
    auto* updates = modelUpdates.get();
    auto doc = ARAHostDocumentController::create (factory, "Afterlife ARA test", std::move (audioAccess), std::make_unique<Archiving>(),
                                                  nullptr, std::move (modelUpdates), std::make_unique<Playback>());
    check (doc != nullptr, "ARA document created");
    if (doc == nullptr) return 1;
    auto& dc = doc->getDocumentController();

    // the clip: 10 s of words, placed at 2 s in the song
    const double place = 2.0, clipLen = 10.0;
    MemorySource take;
    take.audio.setSize (1, (int) (clipLen * SR));
    take.audio.clear();
    for (double t = 0.3; t < clipLen - 0.6; t += 0.7)
        for (int i = (int) (t * SR); i < (int) ((t + 0.4) * SR); ++i) take.audio.setSample (0, i, 0.2f * (float) std::sin (6.283185307179586 * 330.0 * i / SR));

    std::unique_ptr<ARAHostModel::MusicalContext> ctx;
    std::unique_ptr<ARAHostModel::RegionSequence> seq;
    std::unique_ptr<ARAHostModel::AudioModification> mod;
    std::unique_ptr<ARAHostModel::PlaybackRegion> region;
    {
        const ARAEditGuard edit (dc);
        auto cp = ARAHostModel::MusicalContext::getEmptyProperties(); cp.name = "song"; cp.orderIndex = 0;
        ctx = std::make_unique<ARAHostModel::MusicalContext> (nullptr, dc, cp);
        auto sp = ARAHostModel::RegionSequence::getEmptyProperties(); sp.name = "Lead Vox"; sp.orderIndex = 0; sp.musicalContextRef = ctx->getPluginRef();
        seq = std::make_unique<ARAHostModel::RegionSequence> (nullptr, dc, sp);
        auto ap = ARAHostModel::AudioSource::getEmptyProperties();
        ap.name = "take"; ap.persistentID = "take"; ap.sampleCount = take.audio.getNumSamples(); ap.sampleRate = SR; ap.channelCount = 1; ap.merits64BitSamples = false;
        take.source = std::make_unique<ARAHostModel::AudioSource> (MemorySource::Converter::toHostRef (&take), dc, ap);
        auto mp = ARAHostModel::AudioModification::getEmptyProperties(); mp.persistentID = "mod";
        mod = std::make_unique<ARAHostModel::AudioModification> (nullptr, dc, *take.source, mp);
        auto rp = ARAHostModel::PlaybackRegion::getEmptyProperties();
        rp.transformationFlags = ARA::kARAPlaybackTransformationNoChanges;
        rp.startInModificationTime = 0.0; rp.durationInModificationTime = clipLen; rp.startInPlaybackTime = place; rp.durationInPlaybackTime = clipLen;
        rp.musicalContextRef = ctx->getPluginRef(); rp.regionSequenceRef = seq->getPluginRef();
        region = std::make_unique<ARAHostModel::PlaybackRegion> (nullptr, dc, *mod, rp);
    }

    auto ext = doc->bindDocumentToPluginInstance (*plugin, ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole,
                                                  ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole);
    check (ext.isValid(), "Afterlife bound to the document (applied to the clip)");
    auto renderer = ext.getPlaybackRendererInterface();
    renderer.add (*region);

    Head head;
    plugin->setPlayHead (&head);
    plugin->setPlayConfigDetails (1, 1, SR, 512);
    plugin->setNonRealtime (true);
    plugin->prepareToPlay (SR, 512);
    check (plugin->getLatencySamples() == 0, "no latency");

    // the DAW gives access to the samples: Afterlife reads the clip by itself (the waveform)
    {
        const ARAEditGuard edit (dc);
        take.source->enableAudioSourceSamplesAccess (true);
    }
    for (int i = 0; i < 400 && access->furthest.load() < take.audio.getNumSamples(); ++i) { MessageManager::getInstance()->runDispatchLoopUntil (10); dc.notifyModelUpdates(); }
    check (access->furthest.load() == take.audio.getNumSamples(), "it reads the whole clip (its waveform) without playing it (" + String (access->reads.load()) + " reads)");

    auto param = [&] (const String& name) -> AudioProcessorParameter*
    {
        for (auto* prm : plugin->getParameters()) if (prm->getName (64) == name) return prm;
        return nullptr;
    };
    auto set = [&] (const String& name, float pct)   // a 0-100 % parameter
    {
        if (auto* p = param (name)) p->setValueNotifyingHost (pct / 100.0f);
        else check (false, "found parameter " + name);
    };
    const int total = (int) ((place + clipLen + 3.0) * SR);
    auto play = [&]
    {
        AudioBuffer<float> out (1, total), block (1, 512);
        MidiBuffer midi;
        for (int pos = 0; pos < total; pos += 512)
        {
            const int n = jmin (512, total - pos);
            block.setSize (1, n, false, false, true);
            block.clear();
            head.pos = pos;
            plugin->processBlock (block, midi);
            out.copyFrom (0, pos, block, 0, 0, n);
        }
        return out;
    };

    // sends at 0: the clip comes out where it sits in the song, untouched
    set ("Reverb Send", 0.0f); set ("Delay Send", 0.0f);
    plugin->prepareToPlay (SR, 512);
    {
        const auto out = play();
        float worst = 0;
        const int at = (int) (place * SR);
        for (int i = 0; i < take.audio.getNumSamples(); ++i) worst = jmax (worst, std::abs (out.getSample (0, at + i) - take.audio.getSample (0, i)));
        check (worst < 1.0e-5f, String::formatted ("played through it: the clip comes out on time and untouched (max difference %.7f)", worst));
        check (energy (out, 0, (int) (place * SR)) < -150.0, "nothing before the clip starts");
    }
    // reverb up: the tail rings on past the end of the clip
    set ("Reverb Send", 100.0f);
    plugin->prepareToPlay (SR, 512);
    {
        const auto out = play();
        const int end = (int) ((place + clipLen) * SR);
        const double tail = energy (out, end + (int) (0.1 * SR), end + (int) (0.6 * SR));
        check (tail > -60.0, String::formatted ("with the reverb up, the tail rings on past the end of the clip (%.1f dB)", tail));
    }

    plugin->releaseResources();
    renderer.remove (*region);
    {
        const ARAEditGuard edit (dc);
        take.source->enableAudioSourceSamplesAccess (false);
        region.reset(); mod.reset(); take.source.reset(); seq.reset(); ctx.reset();
    }
    doc.reset();
    factory = {};
    plugin.reset();
    std::printf ("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
