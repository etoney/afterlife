#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "AfterlifeEngine.h"
#include "Sections.h"
#include "Waveform.h"
#include "Presets.h"

class AfterlifeProcessor : public juce::AudioProcessor
                         #if JucePlugin_Enable_ARA
                           , public juce::AudioProcessorARAExtension
                         #endif
{
public:
   #if JucePlugin_Enable_ARA
    juce::AudioProcessorARAExtension* getARAClientExtensions() override { return this; }
   #endif
    AfterlifeProcessor();
    ~AfterlifeProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Afterlife"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return engine.tailSeconds(); }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;
    after::Params currentParams() const;
    after::Engine& getEngine() { return engine; }

    // ---- the lanes (message thread). Every edit is published to the audio thread at once.
    after::SectionSet getSections() const { const juce::ScopedLock l (sectionLock); return sections; }
    int addSection (int lane, after::Section s);              // returns its id (0 if there was no room)
    void updateSection (int lane, const after::Section& s);   // same id; clamped between its neighbours
    void removeSection (int lane, int id);
    void clearLane (int lane);
    void setSections (const after::SectionSet&);
    std::atomic<int> sectionsVersion { 0 };

    // ---- the timeline (written by the audio thread, read by the lanes)
    struct PlayState { bool playing = false; double seconds = 0, bpm = 0, ppq = 0, barPpq = 0; int num = 4, den = 4; bool hasMusical = false; };
    PlayState getPlayState() const { const juce::SpinLock::ScopedLockType l (playLock); return play; }
    after::TimelineWave& getWave() { return wave; }
    void forgetWave() { wave.clear(); }

    // ARA: the clips' waveforms laid out on the timeline (empty when not applied to a clip)
    struct AraWave { bool bound = false; int clips = 0, read = 0; std::vector<juce::uint8> bins; int version = -1; };
    const AraWave& araWave();   // message thread; rebuilt when the clips or their waveforms change
    void setAraWaveForTest (std::vector<juce::uint8> bins, int clips);

    // ---- LOADED IR
    bool loadIrFile (const juce::File& f, juce::String& error);   // message thread
    juce::File getIrFile() const { return irFile; }

    // ---- user presets (files shared by every project and DAW)
    static juce::File getUserPresetFolder();
    static juce::Array<juce::File> getUserPresets();
    static juce::File fileForPresetName (const juce::String& name);
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& file);
    juce::String getCurrentPresetName() const { return currentPresetName; }
    std::atomic<int> presetVersion { 0 };

private:
    void resetToDefaults();
    void publish();   // sections -> audio thread (call with sectionLock held)

    after::Engine engine;
    after::TimelineWave wave;

    mutable juce::CriticalSection sectionLock;
    after::SectionSet sections;
    std::shared_ptr<const after::SectionSet> live;                  // what the audio thread reads
    std::vector<std::shared_ptr<const after::SectionSet>> retired;  // freed here, never on the audio thread

    mutable juce::SpinLock playLock;
    PlayState play;

    AraWave ara;
    bool araTestMode = false;
    juce::File irFile;
    int currentProgram = 0;
    juce::String currentPresetName { "Init (Chamber + Quarter)" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AfterlifeProcessor)
};
