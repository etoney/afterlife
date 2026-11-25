#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "AfterlifeARA.h"

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "dev"
#endif

AfterlifeProcessor::AfterlifeProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Afterlife", createLayout())
{
    const juce::ScopedLock l (sectionLock);
    publish();
}

AfterlifeProcessor::~AfterlifeProcessor() { engine.release(); }

juce::AudioProcessorValueTreeState::ParameterLayout AfterlifeProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto dbAttr = AudioParameterFloatAttributes().withLabel ("dB")
        .withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; });
    auto pctAttr = AudioParameterFloatAttributes().withLabel ("%")
        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });
    auto msAttr = AudioParameterFloatAttributes().withLabel ("ms")
        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; });
    auto secAttr = AudioParameterFloatAttributes().withLabel ("s")
        .withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 2 : 1) + " s"; });
    auto hzAttr = AudioParameterFloatAttributes().withLabel ("Hz")
        .withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz"; });
    auto skewed = [] (float lo, float hi, float centre, float step = 0.0f)
    {
        NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    };

    // reverb
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "rv_on", 1 },       "Reverb On", true));
    p.push_back (std::make_unique<AudioParameterChoice>(ParameterID { "rv_space", 1 },    "Space", after::spaceNames(), 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "rv_send", 1 },     "Reverb Send",     NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "rv_decay", 1 },    "Decay",           skewed (0.2f, 15.0f, 2.5f, 0.01f), 1.2f, secAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "rv_predelay", 1 }, "Pre-Delay",       skewed (0.0f, 250.0f, 50.0f, 0.1f), 20.0f, msAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "rv_lowcut", 1 },   "Reverb Low Cut",  skewed (20.0f, 1000.0f, 200.0f, 1.0f), 150.0f, hzAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "rv_highcut", 1 },  "Reverb High Cut", skewed (1000.0f, 20000.0f, 6000.0f, 1.0f), 9000.0f, hzAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "rv_width", 1 },    "Width",           NormalisableRange<float> (0.0f, 150.0f, 1.0f), 100.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "rv_duck", 1 },     "Reverb Duck",     NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "rv_level", 1 },    "Reverb Level",    NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f, dbAttr));
    // delay
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "dl_on", 1 },       "Delay On", true));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "dl_sync", 1 },     "Delay Sync", true));
    p.push_back (std::make_unique<AudioParameterChoice>(ParameterID { "dl_div", 1 },      "Delay Time (Sync)", after::divisionNames(), after::defaultDivision()));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dl_ms", 1 },       "Delay Time (ms)", skewed (1.0f, 2000.0f, 300.0f, 1.0f), 375.0f, msAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dl_feedback", 1 }, "Feedback",        NormalisableRange<float> (0.0f, 95.0f, 0.1f), 35.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterChoice>(ParameterID { "dl_mode", 1 },     "Delay Mode", StringArray { "STEREO", "PING-PONG", "MONO" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dl_tone", 1 },     "Delay Tone",      skewed (800.0f, 20000.0f, 5000.0f, 1.0f), 6000.0f, hzAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dl_lowcut", 1 },   "Delay Low Cut",   skewed (20.0f, 1000.0f, 200.0f, 1.0f), 150.0f, hzAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dl_dirt", 1 },     "Dirt",            NormalisableRange<float> (0.0f, 100.0f, 1.0f), 10.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dl_duck", 1 },     "Delay Duck",      NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dl_send", 1 },     "Delay Send",      NormalisableRange<float> (0.0f, 100.0f, 0.1f), 15.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dl_level", 1 },    "Delay Level",     NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f, dbAttr));
    // both
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dry", 1 },         "Dry",             NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 },      "Output",          NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "lanes", 1 },       "Lanes On", true));
    return { p.begin(), p.end() };
}

after::Params AfterlifeProcessor::currentParams() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    after::Params p;
    p.rvOn = v ("rv_on") > 0.5f;
    p.space = juce::roundToInt (v ("rv_space"));
    p.rvSend = v ("rv_send") * 0.01f;
    p.decay = v ("rv_decay");
    p.predelayMs = v ("rv_predelay");
    p.rvLowCut = v ("rv_lowcut");
    p.rvHighCut = v ("rv_highcut");
    p.width = v ("rv_width") * 0.01f;
    p.rvDuck = v ("rv_duck") * 0.01f;
    p.rvLevelDb = v ("rv_level");
    p.dlOn = v ("dl_on") > 0.5f;
    p.sync = v ("dl_sync") > 0.5f;
    p.division = juce::roundToInt (v ("dl_div"));
    p.delayMs = v ("dl_ms");
    p.feedback = v ("dl_feedback") * 0.01f;
    p.mode = juce::roundToInt (v ("dl_mode"));
    p.tone = v ("dl_tone");
    p.dlLowCut = v ("dl_lowcut");
    p.dirt = v ("dl_dirt") * 0.01f;
    p.dlDuck = v ("dl_duck") * 0.01f;
    p.dlSend = v ("dl_send") * 0.01f;
    p.dlLevelDb = v ("dl_level");
    p.dry = v ("dry") * 0.01f;
    p.outputDb = v ("output");
    p.lanes = v ("lanes") > 0.5f;
    return p;
}

bool AfterlifeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    // mono in -> stereo out is fine too (the reverb and delay spread it)
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void AfterlifeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
   #if JucePlugin_Enable_ARA
    prepareToPlayForARA (sampleRate, samplesPerBlock, getMainBusNumOutputChannels(), getProcessingPrecision());
   #endif
    engine.setParams (currentParams());
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (0);
}

void AfterlifeProcessor::releaseResources()
{
   #if JucePlugin_Enable_ARA
    releaseResourcesForARA();
   #endif
    engine.release();
}

void AfterlifeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int outChs = juce::jmin (buffer.getNumChannels(), getTotalNumOutputChannels()), inChs = juce::jmin (outChs, getTotalNumInputChannels());
    if (inChs > 0) for (int c = inChs; c < outChs; ++c) buffer.copyFrom (c, 0, buffer, 0, 0, n);   // mono in, stereo out

    bool bound = false;
   #if JucePlugin_Enable_ARA
    if (isBoundToARA())
    {
        bound = true;
        processBlockForARA (buffer, isRealtime(), getPlayHead());   // the clip itself, read from the song
    }
   #endif

    // where the song is
    PlayState ps;
    {
        const auto prev = getPlayState();
        ps.seconds = prev.seconds;
    }
    if (auto* ph = getPlayHead())
        if (const auto pos = ph->getPosition())
        {
            ps.playing = pos->getIsPlaying();
            if (const auto ts = pos->getTimeInSamples(); ts.hasValue() && getSampleRate() > 0) ps.seconds = (double) *ts / getSampleRate();
            else if (const auto sec = pos->getTimeInSeconds()) ps.seconds = *sec;
            if (const auto bpm = pos->getBpm()) ps.bpm = *bpm;
            const auto ppq = pos->getPpqPosition();
            const auto bar = pos->getPpqPositionOfLastBarStart();
            const auto sig = pos->getTimeSignature();
            if (ppq.hasValue() && ps.bpm > 0)
            {
                ps.hasMusical = true;
                ps.ppq = *ppq;
                ps.barPpq = bar.hasValue() ? *bar : 0.0;
                if (sig.hasValue()) { ps.num = juce::jmax (1, sig->numerator); ps.den = juce::jmax (1, sig->denominator); }
            }
        }

    // the waveform behind the lanes: what's playing through, where it is (ARA reads the clips instead)
    if (ps.playing && ! bound) wave.capture (buffer.getArrayOfReadPointers(), outChs, n, ps.seconds, getSampleRate());

    {
        const juce::SpinLock::ScopedLockType l (playLock);
        play = ps;
    }

    after::Transport tr;
    tr.playing = ps.playing;
    tr.timeSec = ps.seconds;
    tr.bpm = ps.bpm > 0 ? ps.bpm : 120.0;
    engine.setParams (currentParams());
    const auto set = std::atomic_load (&live);
    engine.process (buffer.getArrayOfWritePointers(), outChs, n, tr, set.get());
}

//==============================================================================
void AfterlifeProcessor::publish()
{
    auto snap = std::make_shared<const after::SectionSet> (sections);
    if (auto old = std::atomic_load (&live)) retired.push_back (std::move (old));
    std::atomic_store (&live, std::shared_ptr<const after::SectionSet> (std::move (snap)));
    // free the old ones nobody's reading any more (never on the audio thread)
    retired.erase (std::remove_if (retired.begin(), retired.end(), [] (const auto& p) { return p.use_count() == 1; }), retired.end());
    ++sectionsVersion;
}

int AfterlifeProcessor::addSection (int lane, after::Section s)
{
    lane = juce::jlimit (0, 1, lane);
    const juce::ScopedLock l (sectionLock);
    s.start = juce::jmax (0.0, s.start);
    const auto room = sections.roomFor (lane, -1, s.start);
    if (room.getLength() < 0.01) return 0;
    s.end = juce::jmin (s.end, room.getEnd());
    if (s.end - s.start < 0.01) return 0;
    s.id = sections.nextId++;
    sections.lanes[(size_t) lane].push_back (s);
    after::SectionSet::sortLane (sections.lanes[(size_t) lane]);
    publish();
    return s.id;
}

void AfterlifeProcessor::updateSection (int lane, const after::Section& s)
{
    lane = juce::jlimit (0, 1, lane);
    const juce::ScopedLock l (sectionLock);
    auto& ln = sections.lanes[(size_t) lane];
    for (auto& x : ln)
        if (x.id == s.id)
        {
            // keep it between its neighbours
            double lo = 0.0, hi = 1.0e9;
            for (auto& o : ln)
            {
                if (o.id == s.id) continue;
                if (o.start < x.start) lo = juce::jmax (lo, o.end); else hi = juce::jmin (hi, o.start);
            }
            auto c = s;
            const double len = juce::jmax (0.01, c.end - c.start);
            const bool moving = std::abs ((s.end - s.start) - (x.end - x.start)) < 1.0e-6;   // same length: a move, not a resize
            if (c.start < lo) { c.start = lo; if (moving) c.end = c.start + len; }
            if (c.end > hi) { c.end = hi; if (moving) c.start = juce::jmax (lo, c.end - len); }
            if (c.end - c.start < 0.01) return;
            x = c;
            break;
        }
    after::SectionSet::sortLane (ln);
    publish();
}

void AfterlifeProcessor::removeSection (int lane, int id)
{
    const juce::ScopedLock l (sectionLock);
    auto& ln = sections.lanes[(size_t) juce::jlimit (0, 1, lane)];
    ln.erase (std::remove_if (ln.begin(), ln.end(), [id] (const after::Section& s) { return s.id == id; }), ln.end());
    publish();
}

void AfterlifeProcessor::clearLane (int lane)
{
    const juce::ScopedLock l (sectionLock);
    sections.lanes[(size_t) juce::jlimit (0, 1, lane)].clear();
    publish();
}

void AfterlifeProcessor::setSections (const after::SectionSet& s)
{
    const juce::ScopedLock l (sectionLock);
    sections = s;
    for (auto& ln : sections.lanes) after::SectionSet::sortLane (ln);
    publish();
}

//==============================================================================
const AfterlifeProcessor::AraWave& AfterlifeProcessor::araWave()
{
    if (araTestMode) return ara;
   #if JucePlugin_Enable_ARA
    if (! isBoundToARA()) { if (ara.bound) ara = {}; return ara; }
    ara.bound = true;
    auto* r = getPlaybackRenderer<after::AfterRenderer>();
    if (r == nullptr) { ara.clips = 0; return ara; }
    const auto clips = r->placedClips();
    double key = (double) r->getDocumentController().waveVersion.load() * 1000003.0;
    int read = 0;
    for (auto& c : clips) { key += c.playStart * 1.37 + c.playEnd * 2.11 + c.srcStart * 3.7; if (c.wave != nullptr) { ++read; key += 17.0; } }
    const int k = (int) std::fmod (key * 977.0, 2.0e9);
    if (k == ara.version && (int) clips.size() == ara.clips) return ara;
    ara.version = k; ara.clips = (int) clips.size(); ara.read = read;
    double last = 0;
    for (auto& c : clips) last = juce::jmax (last, c.playEnd);
    const int nb = juce::jlimit (0, after::kMaxBins, (int) std::ceil (last / after::kBinSeconds) + 1);
    ara.bins.assign ((size_t) nb, 0);
    for (auto& c : clips)
    {
        if (c.wave == nullptr) continue;
        const int b0 = juce::jmax (0, (int) (c.playStart / after::kBinSeconds)), b1 = juce::jmin (nb, (int) std::ceil (c.playEnd / after::kBinSeconds));
        for (int b = b0; b < b1; ++b)
        {
            const double srcT = (double) b * after::kBinSeconds - c.playStart + c.srcStart;
            const int sb = (int) (srcT / after::kBinSeconds);
            if (juce::isPositiveAndBelow (sb, (int) c.wave->bins.size()))
                ara.bins[(size_t) b] = juce::jmax (ara.bins[(size_t) b], c.wave->bins[(size_t) sb]);
        }
    }
   #endif
    return ara;
}

void AfterlifeProcessor::setAraWaveForTest (std::vector<juce::uint8> bins, int clips)
{
    araTestMode = true;
    ara.bound = true; ara.clips = clips; ara.read = clips; ara.bins = std::move (bins); ++ara.version;
}

//==============================================================================
bool AfterlifeProcessor::loadIrFile (const juce::File& f, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr) { error = "Afterlife couldn't read that file. Use a WAV, AIFF or FLAC impulse response."; return false; }
    if (reader->sampleRate <= 0 || reader->lengthInSamples < 16) { error = "That file is empty."; return false; }
    const int len = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (after::kMaxIrSeconds * reader->sampleRate));
    auto ir = std::make_shared<after::LoadedIr>();
    ir->audio.setSize (juce::jlimit (1, 2, (int) reader->numChannels), len);
    reader->read (&ir->audio, 0, len, 0, true, ir->audio.getNumChannels() > 1);
    ir->sampleRate = reader->sampleRate;
    ir->name = f.getFileNameWithoutExtension();
    engine.setLoadedIr (std::move (ir));
    irFile = f;
    if (auto* sp = apvts.getParameter ("rv_space")) sp->setValueNotifyingHost (sp->convertTo0to1 ((float) after::loadedIrIndex()));
    return true;
}

//==============================================================================
int AfterlifeProcessor::getNumPrograms() { return (int) after::factoryPresets().size(); }

const juce::String AfterlifeProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? after::factoryPresets()[(size_t) index].name : "";
}

void AfterlifeProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
}

void AfterlifeProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    currentPresetName = after::factoryPresets()[(size_t) index].name;
    resetToDefaults();
    for (auto& [id, v] : after::factoryPresets()[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
    ++presetVersion;
}

juce::File AfterlifeProcessor::getUserPresetFolder()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);   // ~/Library on macOS, %APPDATA% on Windows
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif
    return base.getChildFile ("Castle Danger").getChildFile ("Afterlife").getChildFile ("Presets");
}

juce::Array<juce::File> AfterlifeProcessor::getUserPresets()
{
    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.afterlife");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0; });
    return files;
}

juce::File AfterlifeProcessor::fileForPresetName (const juce::String& name)
{
    return getUserPresetFolder().getChildFile (juce::File::createLegalFileName (name.trim()) + ".afterlife");
}

bool AfterlifeProcessor::saveUserPreset (const juce::String& rawName)
{
    const auto name = rawName.trim();
    if (name.isEmpty()) return false;
    const auto file = fileForPresetName (name);
    if (! file.getParentDirectory().createDirectory()) return false;
    juce::XmlElement xml ("AfterlifePreset");
    xml.setAttribute ("name", name);
    xml.setAttribute ("pluginVersion", JucePlugin_VersionString);
    if (irFile != juce::File()) xml.setAttribute ("irFile", irFile.getFullPathName());
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", rp->getParameterID());
            e->setAttribute ("value", (double) rp->convertFrom0to1 (rp->getValue()));
        }
    if (! xml.writeTo (file)) return false;
    currentPresetName = name;
    ++presetVersion;
    return true;
}

bool AfterlifeProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("AfterlifePreset")) return false;
    resetToDefaults();
    const juce::File ir (xml->getStringAttribute ("irFile"));
    if (xml->hasAttribute ("irFile") && ir.existsAsFile() && ir != irFile) { juce::String err; loadIrFile (ir, err); }
    for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        if (auto* rp = apvts.getParameter (e->getStringAttribute ("id")))
            rp->setValueNotifyingHost (rp->convertTo0to1 ((float) e->getDoubleAttribute ("value")));
    currentPresetName = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
    ++presetVersion;
    return true;
}

//==============================================================================
void AfterlifeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    state.setProperty ("irFile", irFile.getFullPathName(), nullptr);
    state.setProperty ("wave", wave.toBase64(), nullptr);
    state.removeChild (state.getChildWithName ("SECTIONS"), nullptr);
    state.appendChild (getSections().toTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void AfterlifeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto vt = juce::ValueTree::fromXml (*xml);
    currentProgram = vt.getProperty ("program", 0);
    currentPresetName = vt.getProperty ("presetName", getProgramName (currentProgram)).toString();
    const auto irPath = vt.getProperty ("irFile", "").toString();
    const auto waveData = vt.getProperty ("wave", "").toString();
    const auto secTree = vt.getChildWithName ("SECTIONS").createCopy();
    vt.removeChild (vt.getChildWithName ("SECTIONS"), nullptr);
    vt.removeProperty ("wave", nullptr);
    vt.removeProperty ("irFile", nullptr);

    if (irPath.isNotEmpty() && juce::File::isAbsolutePath (irPath))
    {
        const juce::File f (irPath);
        juce::String err;
        if (! (f.existsAsFile() && loadIrFile (f, err))) irFile = f;   // remembered, shown as missing
    }
    apvts.replaceState (vt);
    setSections (after::SectionSet::fromTree (secTree));
    wave.fromBase64 (waveData);
    ++presetVersion;
}

juce::AudioProcessorEditor* AfterlifeProcessor::createEditor() { return new AfterlifeEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AfterlifeProcessor(); }

#if JucePlugin_Enable_ARA
const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory()
{
    return juce::ARADocumentControllerSpecialisation::createARAFactory<after::AfterDocumentController>();
}
#endif
