#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class AfterLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AfterLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool hi, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool hi, bool down) override;
    juce::Image rustDisc;
};

// Segmented row of chunky rusty buttons bound to a choice / bool parameter
class Selector : public juce::Component, public juce::SettableTooltipClient
{
public:
    Selector (juce::RangedAudioParameter& p, juce::StringArray labels, juce::StringArray subLabels = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    int getIndex() const { return index; }
    std::function<void()> onChange;
private:
    int segmentAt (juce::Point<int>) const;
    juce::StringArray labels, subs;
    juce::ParameterAttachment att;
    int index = 0, hover = -1;
};

// Round iron gauge in the header: how much reverb + delay is coming out
class WetGauge : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void set (float db) { wetDb = db; }
private:
    float wetDb = -60.0f;
};

// The space's impulse response, drawn as its decay
class IrScreen : public juce::Component
{
public:
    explicit IrScreen (AfterlifeProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    AfterlifeProcessor& proc;
};

// The echoes: where they land and how they fade (MAIN in cream, the THROW in yellow)
class EchoScreen : public juce::Component
{
public:
    explicit EchoScreen (AfterlifeProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    AfterlifeProcessor& proc;
};

//==============================================================================
// THE LANES: the song's waveform with a REVERB lane and a DELAY lane on top. Drag in a lane to mark a section;
// drag a section to move it, its edges to resize it, its top line to set its level. The view follows the
// DAW's playhead (FOLLOW), or scroll / zoom it yourself.
class LanesView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LanesView (AfterlifeProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    void tick();                       // the editor's timer: follow the playhead, repaint
    void zoomBy (double factor);       // around the playhead (or the middle)
    void setFollow (bool f) { follow = f; if (onFollowChanged) onFollowChanged(); }
    bool isFollowing() const { return follow; }
    void setSnap (int s) { snap = s; }
    void setView (double start, double length) { viewStart = juce::jmax (0.0, start); viewLen = juce::jlimit (0.25, 600.0, length); repaint(); }
    void select (int lane, int id) { selLane = lane; selId = id; if (onSelectionChanged) onSelectionChanged(); repaint(); }

    int getSelectedLane() const { return selLane; }
    int getSelectedId() const { return selId; }
    std::function<void()> onSelectionChanged, onFollowChanged;

    static constexpr int kGutter = 70, kRuler = 22, kGap = 6;

private:
    enum class Drag { none, create, move, resizeL, resizeR, level, scroll };
    struct Hit { int lane = -1; int id = 0; Drag what = Drag::none; };

    juce::Rectangle<float> laneRect (int lane) const;
    juce::Rectangle<float> rulerRect() const;
    double xToTime (float x) const;
    float timeToX (double t) const;
    float levelToY (int lane, float send) const;
    float yToLevel (int lane, float y) const;
    int laneAt (juce::Point<float>) const;
    Hit hitTest (juce::Point<float>) const;
    double snapTime (double t) const;
    double gridQuarters() const;
    double ppqAt (double t) const;
    double timeAtPpq (double q) const;
    float baseLevel (int lane) const;
    void showMenu (const juce::MouseEvent&, Hit);
    void paintLane (juce::Graphics&, int lane, const after::SectionSet&);
    void paintRuler (juce::Graphics&);
    juce::uint8 binAt (int bin) const;

    AfterlifeProcessor& proc;
    AfterlifeProcessor::PlayState ps;
    double viewStart = 0.0, viewLen = 12.0;
    bool follow = true;
    int snap = 2;
    int selLane = -1, selId = 0;
    Drag drag = Drag::none;
    int dragLane = -1, dragId = 0;
    double dragT0 = 0, dragView0 = 0;
    after::Section dragOrig;
    const AfterlifeProcessor::AraWave* araw = nullptr;
};

// The selected section's own settings
class Inspector : public juce::Component
{
public:
    Inspector (AfterlifeProcessor& p, LanesView& l);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();   // the selection or the section changed
private:
    struct Knob { juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow }; juce::Label label; };
    void setup (Knob&, const juce::String& name);
    void push();
    AfterlifeProcessor& proc;
    LanesView& lanes;
    Knob send, fade, time, feedback;
    juce::TextButton deleteButton { "DELETE" };
    bool has = false;
    int lane = -1;
    after::Section cur;
};

// In-window modal dialog (message, confirm, or text entry), drawn over the editor
class AfterDialog : public juce::Component
{
public:
    AfterDialog (const juce::String& title, const juce::String& message, bool withTextField,
                 const juce::String& initialText, juce::StringArray buttons,
                 std::function<void (int buttonIndex, const juce::String& text)> onClose);
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void parentHierarchyChanged() override;
    void mouseDown (const juce::MouseEvent&) override {}
private:
    void finish (int index);
    juce::String title, message;
    juce::TextEditor field;
    bool hasField;
    juce::OwnedArray<juce::TextButton> buttons;
    std::function<void (int, const juce::String&)> onClose;
    juce::Rectangle<int> panel;
};

//==============================================================================
class AfterlifeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AfterlifeEditor (AfterlifeProcessor&);
    ~AfterlifeEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    // used by the headless snapshot test
    juce::TextButton& getSaveButton() { return saveButton; }
    LanesView& getLanes() { return lanes; }

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Plate { juce::String title; juce::Rectangle<int> bounds; bool hot = false; };

    Knob& makeKnob (const juce::String& id, const juce::String& text, const juce::String& tip);
    void placeKnob (Knob&, juce::Rectangle<int>);
    void timerCallback() override;
    void buildBackdrop();
    void chooseIr();

    void refreshPresetBox();
    void stepPreset (int delta);
    void promptSave();
    void confirmDelete();
    void showDialog (const juce::String& title, const juce::String& message, bool withTextField, const juce::String& initialText,
                     juce::StringArray buttons, std::function<void (int, const juce::String&)> onResult);
    void showMessage (const juce::String& title, const juce::String& text);

    AfterlifeProcessor& proc;
    AfterLookAndFeel lnf;
    std::vector<std::unique_ptr<Knob>> knobs;
    Knob *dlDivKnob = nullptr, *dlMsKnob = nullptr;
    WetGauge gauge;
    juce::TooltipWindow tooltips { this, 700 };

    juce::ComboBox spaceBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> spaceAtt;
    juce::TextButton loadIrButton { "LOAD IR" };
    juce::ToggleButton rvOn { "ON" }, dlOn { "ON" }, followButton { "FOLLOW" }, lanesOn { "SECTIONS ON" };
    juce::AudioProcessorValueTreeState::ButtonAttachment rvOnAtt, dlOnAtt, lanesOnAtt;
    Selector syncSel, modeSel;
    IrScreen irScreen;
    EchoScreen echoScreen;

    juce::ComboBox snapBox;
    juce::TextButton zoomOut { "-" }, zoomIn { "+" }, forgetButton { "FORGET WAVE" };
    LanesView lanes;
    Inspector inspector;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, deleteButton { "DELETE" };
    juce::Array<juce::File> userFiles;
    int lastPresetVersion = -1, lastSectionsVersion = -1;
    std::unique_ptr<AfterDialog> dialog;
    std::unique_ptr<juce::FileChooser> chooser;
    static constexpr int kUserIdBase = 1001;

    std::vector<Plate> plates;
    juce::Rectangle<int> badgeArea, crestArea, presetArea, stripeArea, statusArea;
    juce::Image backdrop, logoImage;
    float wetMeter = 0, flicker = 0;
    juce::String lastStatus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AfterlifeEditor)
};
