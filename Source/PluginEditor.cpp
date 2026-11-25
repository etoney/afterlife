#include "PluginEditor.h"
#include "CDStyle.h"

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "dev"
#endif

//==============================================================================
AfterLookAndFeel::AfterLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, pal::paintDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, pal::rust.withAlpha (0.6f));
    setColour (juce::Label::textColourId, pal::paintDim);
    setColour (juce::ComboBox::textColourId, pal::paint);
    setColour (juce::ComboBox::arrowColourId, pal::signal);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1a1411));
    setColour (juce::PopupMenu::textColourId, pal::paint);
    setColour (juce::PopupMenu::headerTextColourId, pal::rustLight);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, pal::rust);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::textColourOffId, pal::paint);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0f0b09));
    setColour (juce::TextEditor::textColourId, pal::paint);
    setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff5a3420));
    setColour (juce::TextEditor::focusedOutlineColourId, pal::rustLight);
    setColour (juce::TextEditor::highlightColourId, pal::rust.withAlpha (0.6f));
    setColour (juce::CaretComponent::caretColourId, pal::signalHot);

    rustDisc = juce::Image (juce::Image::ARGB, 128, 128, true);
    juce::Image::BitmapData bd (rustDisc, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x)
        {
            const float n = tex::fbm ((float) x / 22.0f, (float) y / 22.0f, 501, 4);
            const float a = tex::smoothstep (0.5f, 0.7f, n);
            const auto c = tex::rustAt ((float) x * 2.0f, (float) y * 2.0f, 0.3f, 777);
            bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (c.r, c.g, c.b, a * 0.85f));
        }
}

juce::Font AfterLookAndFeel::getComboBoxFont (juce::ComboBox&) { return tex::plainBold (13.0f); }
juce::Font AfterLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return tex::stencil (juce::jmin (15.0f, (float) h * 0.62f)).withExtraKerningFactor (0.08f); }

void AfterLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 3.0f);
    g.setGradientFill (juce::ColourGradient (down ? pal::rustDeep : juce::Colour (0xff3a2a20), 0, r.getY(),
                                             down ? juce::Colour (0xff2a120a) : juce::Colour (0xff17110e), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (hi ? pal::rustLight : juce::Colour (0xff6a3a20)); g.drawRoundedRectangle (r, 3.0f, 1.2f);
}

void AfterLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (juce::Colour (0xff0f0b09)); g.fillRoundedRectangle (r, 3.0f);
    g.setColour (box.isMouseOver (true) ? pal::rustLight : juce::Colour (0xff6a3a20)); g.drawRoundedRectangle (r, 3.0f, 1.2f);
    juce::Path arrow; const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 5, ay - 3, ax + 5, ay - 3, ax, ay + 4);
    g.setColour (pal::signal); g.fillPath (arrow);
}

void AfterLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float thick = juce::jmax (2.5f, r * 0.11f);
    const float alpha = s.isEnabled() ? 1.0f : 0.3f;

    // tick marks stamped round the dial
    g.setColour (pal::paintDim.withAlpha (0.45f * alpha));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle) - juce::MathConstants<float>::halfPi;
        const float r1 = r + 1.0f, r0 = r - (i % 5 == 0 ? 5.0f : 3.0f);
        g.drawLine (c.x + std::cos (a) * r0, c.y + std::sin (a) * r0, c.x + std::cos (a) * r1, c.y + std::sin (a) * r1, i % 5 == 0 ? 1.4f : 0.9f);
    }

    const float ar = r - thick - 4.0f;
    juce::Path track; track.addCentredArc (c.x, c.y, ar, ar, 0, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff120e0c).withAlpha (alpha)); g.strokePath (track, juce::PathStrokeType (thick + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xff3a2418).withAlpha (alpha)); g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path val; val.addCentredArc (c.x, c.y, ar, ar, 0, startAngle, angle, true);
    const auto arcCol = (s.isMouseOverOrDragging() ? pal::signalHot : pal::signal).withAlpha (alpha);
    g.setColour (arcCol.withAlpha (0.25f * alpha));
    g.strokePath (val, juce::PathStrokeType (thick + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (arcCol);
    g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float br = ar - thick * 1.4f;
    if (br < 4.0f) return;
    const juce::Rectangle<float> body (c.x - br, c.y - br, br * 2, br * 2);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillEllipse (body.translated (1.5f, 2.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a5a4c).withAlpha (alpha), c.x - br * 0.5f, c.y - br * 0.7f,
                                             juce::Colour (0xff17120f), c.x + br * 0.6f, c.y + br, true));
    g.fillEllipse (body);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addEllipse (body);
        g.reduceClipRegion (clip);
        const float off = (float) (((juce::uint64) (juce::pointer_sized_int) &s >> 4) % 64);
        g.setOpacity (0.9f * alpha);
        g.drawImage (rustDisc, body.expanded (br * 0.4f).translated (-off * 0.3f, off * 0.2f), juce::RectanglePlacement::stretchToFit);
    }
    g.setColour (juce::Colour (0xff0f0b09).withAlpha (0.8f));
    for (int i = 0; i < 32; ++i)
    {
        const float a = (float) i / 32.0f * juce::MathConstants<float>::twoPi;
        g.drawLine (c.x + std::cos (a) * br * 0.87f, c.y + std::sin (a) * br * 0.87f,
                    c.x + std::cos (a) * br, c.y + std::sin (a) * br, 1.0f);
    }
    g.setColour (juce::Colour (0xff0b0807)); g.drawEllipse (body, 1.2f);

    juce::Path p; p.addRoundedRectangle (-1.8f, -br * 0.92f, 3.6f, br * 0.6f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c.x + 0.8f, c.y + 1.0f));
    g.setColour (pal::paint.withAlpha (alpha));
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
}

// toggles: an indicator lamp + stencilled label on a rusty plate
void AfterLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hi, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const bool warn = false;
    const auto lit = warn ? pal::hazard : pal::signalHot;
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 3.0f);
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xff3a1a10) : juce::Colour (0xff2a221d), 0, r.getY(),
                                             juce::Colour (0xff120d0b), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (on ? lit.withAlpha (0.8f) : (hi ? pal::rustLight : juce::Colour (0xff5a3420))); g.drawRoundedRectangle (r, 3.0f, 1.2f);

    const float d = r.getHeight() * 0.42f;
    auto led = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + r.getHeight() * 0.55f, r.getCentreY() });
    if (on)
    {
        juce::Path lp; lp.addEllipse (led);
        juce::DropShadow (lit.withAlpha (0.9f), 10, {}).drawForPath (g, lp);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff0c0), led.getCentreX(), led.getCentreY(), lit, led.getX(), led.getY(), true));
    }
    else g.setColour (juce::Colour (0xff2a1c16));
    g.fillEllipse (led);
    g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawEllipse (led, 1.0f);

    g.setColour (on ? pal::paint : pal::paintDim);
    g.setFont (tex::stencil (15.0f).withExtraKerningFactor (0.1f));
    g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (r.getHeight() * 1.0f).withTrimmedRight (6).toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
}

//==============================================================================
Selector::Selector (juce::RangedAudioParameter& p, juce::StringArray l, juce::StringArray s)
    : labels (std::move (l)), subs (std::move (s)),
      att (p, [this] (float v) { index = juce::roundToInt (v); repaint(); if (onChange) onChange(); })
{
    att.sendInitialUpdate();
}

int Selector::segmentAt (juce::Point<int> p) const
{
    if (labels.isEmpty()) return -1;
    return juce::jlimit (0, labels.size() - 1, p.x * labels.size() / juce::jmax (1, getWidth()));
}

void Selector::mouseDown (const juce::MouseEvent& e) { att.setValueAsCompleteGesture ((float) segmentAt (e.getPosition())); }
void Selector::mouseMove (const juce::MouseEvent& e) { const int h = segmentAt (e.getPosition()); if (h != hover) { hover = h; repaint(); } }

void Selector::paint (juce::Graphics& g)
{
    const int n = labels.size();
    const float sw = (float) getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto r = juce::Rectangle<float> ((float) i * sw, 0, sw, (float) getHeight()).reduced (3.0f, 2.0f);
        const bool on = i == index, hi = i == hover;
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.translated (1.5f, 2.0f), 3.0f);
        if (on)
        {
            juce::Path pth; pth.addRoundedRectangle (r, 3.0f);
            juce::DropShadow (pal::signalHot.withAlpha (0.45f), 12, {}).drawForPath (g, pth);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a2a14), r.getCentreX(), r.getY(), juce::Colour (0xff3a0e06), r.getCentreX(), r.getBottom(), false));
        }
        else
            g.setGradientFill (juce::ColourGradient (hi ? juce::Colour (0xff43332a) : juce::Colour (0xff332822), r.getCentreX(), r.getY(),
                                                     juce::Colour (0xff15100d), r.getCentreX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? pal::signalHot : (hi ? pal::rustLight : juce::Colour (0xff5a3420)));
        g.drawRoundedRectangle (r, 3.0f, on ? 1.6f : 1.1f);
        // little lamp in the corner
        auto lamp = juce::Rectangle<float> (6, 6).withCentre ({ r.getX() + 9, r.getY() + 9 });
        g.setColour (on ? juce::Colour (0xffffd0a0) : juce::Colour (0xff2a1c16)); g.fillEllipse (lamp);
        // grime
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawLine (r.getRight() - 18, r.getBottom() - 3, r.getRight() - 4, r.getBottom() - 10, 1.0f);

        const bool hasSub = subs.size() > i && subs[i].isNotEmpty();
        auto tr = r.reduced (6, 2);
        g.setColour (on ? juce::Colour (0xfffff0d8) : pal::paint.withAlpha (0.85f));
        g.setFont (tex::stencil (hasSub ? 19.0f : 18.0f).withExtraKerningFactor (0.08f));
        g.drawFittedText (labels[i], (hasSub ? tr.removeFromTop (tr.getHeight() * 0.62f) : tr).toNearestInt(),
                          hasSub ? juce::Justification::centredBottom : juce::Justification::centred, 1, 0.5f);
        if (hasSub)
        {
            g.setColour (on ? pal::paint : pal::paintDim.withAlpha (0.7f));
            g.setFont (tex::plainBold (9.5f).withExtraKerningFactor (0.12f));
            g.drawFittedText (subs[i], tr.toNearestInt(), juce::Justification::centredTop, 1, 0.7f);
        }
    }
}



//==============================================================================
void WetGauge::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    const auto c = r.getCentre();
    const float R = std::min (r.getWidth(), r.getHeight()) * 0.5f;
    const float halfPi = juce::MathConstants<float>::halfPi;
    const float a0 = -juce::MathConstants<float>::pi * 0.75f, a1 = juce::MathConstants<float>::pi * 0.75f;

    // iron bezel
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (r.translated (1.5f, 2.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a5a4c), c.x - R * 0.6f, c.y - R * 0.8f, juce::Colour (0xff1a1410), c.x + R, c.y + R, true));
    g.fillEllipse (r);
    g.setColour (pal::rust.withAlpha (0.55f)); g.drawEllipse (r.reduced (1.0f), 1.6f);

    // screen face
    const auto face = r.reduced (R * 0.14f);
    const float fr = face.getWidth() * 0.5f;
    g.setColour (pal::screen); g.fillEllipse (face);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x26ff6a3d), c.x, c.y, juce::Colours::transparentBlack, c.x, c.y - fr, true));
    g.fillEllipse (face);

    // scale: -48 dB on the left, 0 dB on the right
    for (int i = 0; i <= 8; ++i)
    {
        const float a = a0 + (float) i / 8.0f * (a1 - a0) - halfPi;
        const bool top = i >= 7;
        g.setColour (top ? pal::signalHot : pal::paintDim.withAlpha (0.7f));
        const float r0 = fr * (i % 2 == 0 ? 0.62f : 0.72f), r1 = fr * 0.88f;
        g.drawLine (c.x + std::cos (a) * r0, c.y + std::sin (a) * r0, c.x + std::cos (a) * r1, c.y + std::sin (a) * r1, i % 2 == 0 ? 1.6f : 1.0f);
    }

    // needle: the reverb + delay coming out
    const float t = juce::jlimit (0.0f, 1.0f, (wetDb + 48.0f) / 48.0f);
    const float ang = a0 + t * (a1 - a0);
    const bool working = wetDb > -40.0f;
    const auto col = wetDb > -6.0f ? pal::signalHot : pal::paint;
    juce::Path needle; needle.addRoundedRectangle (-1.6f, -fr * 0.80f, 3.2f, fr * 0.80f, 1.0f);
    juce::Path lit (needle); lit.applyTransform (juce::AffineTransform::rotation (ang).translated (c));
    if (working) juce::DropShadow (col.withAlpha (0.65f), 9, {}).drawForPath (g, lit);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (needle, juce::AffineTransform::rotation (ang).translated (c.x + 1.0f, c.y + 1.5f));
    g.setColour (col);
    g.fillPath (needle, juce::AffineTransform::rotation (ang).translated (c));

    // hub
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a7a68), c.x - 3.0f, c.y - 3.0f, juce::Colour (0xff1a1410), c.x + 5.0f, c.y + 5.0f, true));
    g.fillEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);
    g.setColour (juce::Colour (0xff0c0807)); g.drawEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f, 1.0f);

    g.setColour (pal::paintDim.withAlpha (0.85f)); g.setFont (tex::plainBold (8.0f).withExtraKerningFactor (0.15f));
    g.drawText ("WET", juce::Rectangle<float> (c.x - 20.0f, c.y + fr * 0.46f, 40.0f, 10.0f), juce::Justification::centred);

    // glass
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), c.x - fr * 0.4f, c.y - fr, juce::Colours::transparentWhite, c.x, c.y, false));
    g.fillEllipse (face);
    g.setColour (juce::Colours::black); g.drawEllipse (face, 1.6f);
}


//==============================================================================
static void screenBase (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (pal::screen); g.fillRoundedRectangle (r, 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x1cff6a3d), r.getCentreX(), r.getCentreY(), juce::Colours::transparentBlack, r.getX(), r.getY(), true));
    g.fillRoundedRectangle (r, 4.0f);
}
static void screenGlass (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX(), r.getCentreY(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (r, 4.0f, 2.0f);
    g.setColour (pal::rust.withAlpha (0.35f)); g.drawRoundedRectangle (r.reduced (2.0f), 3.0f, 1.0f);
}
static void lamp (juce::Graphics& g, juce::Rectangle<float> led, bool on, juce::Colour lit)
{
    if (on)
    {
        juce::Path lp; lp.addEllipse (led);
        juce::DropShadow (lit.withAlpha (0.9f), 8, {}).drawForPath (g, lp);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff0c0), led.getCentreX(), led.getCentreY(), lit, led.getX(), led.getY(), true));
    }
    else g.setColour (juce::Colour (0xff2a1c16));
    g.fillEllipse (led);
    g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawEllipse (led, 0.8f);
}
static juce::String timeText (double t, bool tenths)
{
    t = juce::jmax (0.0, t);
    const int m = (int) (t / 60.0);
    const double s = t - m * 60.0;
    return juce::String (m) + ":" + (tenths ? juce::String (s, 1).paddedLeft ('0', 4) : juce::String ((int) s).paddedLeft ('0', 2));
}

//==============================================================================
void IrScreen::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    screenBase (g, r);
    auto& e = proc.getEngine();
    const auto v = e.irView();
    const bool on = proc.apvts.getRawParameterValue ("rv_on")->load() > 0.5f;
    auto in = r.reduced (8.0f, 6.0f);
    auto head = in.removeFromTop (16.0f);
    auto foot = in.removeFromBottom (14.0f);

    g.setFont (tex::stencil (13.0f).withExtraKerningFactor (0.08f));
    g.setColour (on ? pal::paint : pal::paintDim);
    const auto missing = proc.getIrFile() != juce::File() && ! proc.getIrFile().existsAsFile()
                         && juce::roundToInt (proc.apvts.getRawParameterValue ("rv_space")->load()) == after::loadedIrIndex();
    g.drawFittedText (missing ? "IR MISSING: " + proc.getIrFile().getFileName().toUpperCase() : (v != nullptr ? v->name.toUpperCase() : juce::String ("...")),
                      head.toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
    if (v != nullptr)
    {
        g.setColour (pal::paintDim); g.setFont (tex::plainBold (9.0f));
        g.drawText (juce::String (v->seconds, 1) + " S", head, juce::Justification::centredRight);
        // the decay
        const auto plot = in.reduced (0.0f, 2.0f);
        g.setColour (juce::Colour (0xff2a1d16));
        for (int db : { -20, -40 })
        {
            const float y = plot.getY() + (float) -db / 60.0f * plot.getHeight();
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        }
        juce::Path fill, line;
        const int N = (int) v->outline.size();
        for (int i = 0; i < N; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) juce::jmax (1, N - 1);
            const float y = plot.getY() + juce::jlimit (0.0f, 1.0f, -v->outline[(size_t) i] / 60.0f) * plot.getHeight();
            if (i == 0) { fill.startNewSubPath (x, plot.getBottom()); fill.lineTo (x, y); line.startNewSubPath (x, y); }
            else { fill.lineTo (x, y); line.lineTo (x, y); }
        }
        fill.lineTo (plot.getRight(), plot.getBottom()); fill.closeSubPath();
        const auto col = on ? pal::signalHot : pal::paintDim;
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.45f), plot.getX(), plot.getY(), col.withAlpha (0.05f), plot.getX(), plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (col.withAlpha (0.9f)); g.strokePath (line, juce::PathStrokeType (1.3f));
    }
    // what's going in right now
    const bool sec = e.rvInSection.load();
    lamp (g, juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ foot.getX() + 4.0f, foot.getCentreY() }), sec, pal::signalHot);
    g.setFont (tex::plainBold (9.0f).withExtraKerningFactor (0.1f));
    g.setColour (sec ? pal::paint : pal::paintDim.withAlpha (0.8f));
    g.drawText ((sec ? "SECTION  //  SEND " : "SEND ") + juce::String (juce::roundToInt (e.revSendNow.load() * 100.0f)) + "%",
                foot.withTrimmedLeft (12.0f), juce::Justification::centredLeft);
    screenGlass (g, r);
}

//==============================================================================
void EchoScreen::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    screenBase (g, r);
    auto& e = proc.getEngine();
    const auto p = proc.currentParams();
    const double mainT = e.delaySeconds (p, -1);
    const bool on = p.dlOn;
    auto in = r.reduced (8.0f, 6.0f);
    auto head = in.removeFromTop (16.0f);
    auto foot = in.removeFromBottom (14.0f);

    g.setFont (tex::stencil (13.0f).withExtraKerningFactor (0.08f));
    g.setColour (on ? pal::paint : pal::paintDim);
    juce::String t = p.sync ? juce::String (after::divisions()[(size_t) juce::jlimit (0, (int) after::divisions().size() - 1, p.division)].name) + "  =  " : juce::String();
    t << juce::roundToInt (mainT * 1000.0) << " MS";
    g.drawFittedText (t, head.toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
    g.setColour (pal::paintDim); g.setFont (tex::plainBold (9.0f));
    g.drawText ("FB " + juce::String (juce::roundToInt (p.feedback * 100.0f)) + "%", head, juce::Justification::centredRight);

    const auto plot = in.reduced (0.0f, 2.0f);
    const int throwId = e.throwSectionId.load();
    const double throwT = e.lastThrowSeconds.load(), throwFb = e.lastThrowFb.load();
    const double span = juce::jmax (1.0, juce::jmax (mainT, throwId > 0 ? throwT : 0.0) * 6.5);
    const float cy = plot.getCentreY(), half = plot.getHeight() * 0.5f - 2.0f;
    g.setColour (juce::Colour (0xff2a1d16)); g.drawHorizontalLine ((int) cy, plot.getX(), plot.getRight());
    // the dry hit
    g.setColour (pal::paintDim.withAlpha (0.8f));
    g.fillRect (plot.getX(), cy - half, 2.0f, half * 2.0f);
    auto taps = [&] (double time, double fb, juce::Colour col, float dx)
    {
        double lvl = 1.0;
        for (int k = 1; k < 40; ++k)
        {
            const float x = plot.getX() + (float) (k * time / span) * plot.getWidth() + dx;
            if (x > plot.getRight() || lvl < 0.03) break;
            const float h = half * (float) lvl;
            const bool left = (k % 2) == 1;
            if (p.mode == after::kPingPong) g.setColour (col), g.fillRect (x - 1.5f, left ? cy - h : cy, 3.0f, h);
            else g.setColour (col), g.fillRect (x - 1.5f, cy - h, 3.0f, h * 2.0f);
            lvl *= fb;
        }
    };
    if (on)
    {
        taps (mainT, p.feedback, pal::paint.withAlpha (0.8f), 0.0f);
        if (throwId > 0) taps (throwT, throwFb, pal::hazard.withAlpha (0.85f), 2.5f);
    }
    if (p.mode == after::kPingPong)
    {
        g.setColour (pal::paintDim.withAlpha (0.5f)); g.setFont (tex::plainBold (8.0f));
        g.drawText ("L", juce::Rectangle<float> (plot.getRight() - 10.0f, plot.getY(), 10.0f, 10.0f), juce::Justification::centred);
        g.drawText ("R", juce::Rectangle<float> (plot.getRight() - 10.0f, plot.getBottom() - 10.0f, 10.0f, 10.0f), juce::Justification::centred);
    }
    const bool sec = e.dlInSection.load();
    lamp (g, juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ foot.getX() + 4.0f, foot.getCentreY() }), sec, pal::hazard);
    g.setFont (tex::plainBold (9.0f).withExtraKerningFactor (0.1f));
    g.setColour (sec ? pal::paint : pal::paintDim.withAlpha (0.8f));
    juce::String f;
    if (sec) f << "THROW " << juce::roundToInt (e.throwSendNow.load() * 100.0f) << "%";
    else f << "SEND " << juce::roundToInt (e.mainSendNow.load() * 100.0f) << "%";
    if (throwId > 0) f << "  //  THROW " << juce::roundToInt (throwT * 1000.0) << " MS";
    g.drawText (f, foot.withTrimmedLeft (12.0f), juce::Justification::centredLeft);
    screenGlass (g, r);
}

//==============================================================================
LanesView::LanesView (AfterlifeProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    setTooltip ("Drag in a lane to mark a section. Drag a section to move it, its edges to resize it, its top line to set "
                "how much goes in. Right-click for more. DELETE removes the selected section. Scroll to move along the song, "
                "Ctrl / Cmd + scroll to zoom.");
}

juce::Rectangle<float> LanesView::rulerRect() const
{
    return getLocalBounds().toFloat().removeFromTop ((float) kRuler).withTrimmedLeft ((float) kGutter);
}

juce::Rectangle<float> LanesView::laneRect (int lane) const
{
    auto area = getLocalBounds().toFloat().withTrimmedTop ((float) kRuler + 2.0f).withTrimmedLeft ((float) kGutter).withTrimmedBottom (2.0f);
    const float h = (area.getHeight() - (float) kGap) * 0.5f;
    return lane == 0 ? area.removeFromTop (h) : area.withTrimmedTop (h + (float) kGap);
}

double LanesView::xToTime (float x) const { return viewStart + (double) (x - (float) kGutter) / juce::jmax (1.0, (double) (getWidth() - kGutter)) * viewLen; }
float LanesView::timeToX (double t) const { return (float) kGutter + (float) ((t - viewStart) / viewLen * (double) (getWidth() - kGutter)); }
float LanesView::levelToY (int lane, float send) const { const auto r = laneRect (lane); return r.getBottom() - 4.0f - juce::jlimit (0.0f, 1.0f, send) * (r.getHeight() - 22.0f); }
float LanesView::yToLevel (int lane, float y) const { const auto r = laneRect (lane); return juce::jlimit (0.0f, 1.0f, (r.getBottom() - 4.0f - y) / juce::jmax (1.0f, r.getHeight() - 22.0f)); }

int LanesView::laneAt (juce::Point<float> p) const
{
    for (int l = 0; l < 2; ++l) if (laneRect (l).expanded (0.0f, (float) kGap * 0.5f).contains (p)) return l;
    return -1;
}

float LanesView::baseLevel (int lane) const
{
    return proc.apvts.getRawParameterValue (lane == after::kReverbLane ? "rv_send" : "dl_send")->load() * 0.01f;
}

double LanesView::ppqAt (double t) const
{
    if (ps.hasMusical && ps.bpm > 0) return ps.ppq + (t - ps.seconds) * ps.bpm / 60.0;
    return t * 2.0;   // no tempo from the DAW: 120 BPM from zero
}
double LanesView::timeAtPpq (double q) const
{
    if (ps.hasMusical && ps.bpm > 0) return ps.seconds + (q - ps.ppq) * 60.0 / ps.bpm;
    return q * 0.5;
}
double LanesView::gridQuarters() const
{
    switch (snap)
    {
        case 1: return 4.0 / (double) juce::jmax (1, ps.den);
        case 2: return 0.5;
        case 3: return 0.25;
        default: return 0.0;
    }
}
double LanesView::snapTime (double t) const
{
    const double step = gridQuarters();
    if (step <= 0) return juce::jmax (0.0, t);
    return juce::jmax (0.0, timeAtPpq (std::round (ppqAt (t) / step) * step));
}

juce::uint8 LanesView::binAt (int bin) const
{
    if (araw != nullptr && araw->bound) return juce::isPositiveAndBelow (bin, (int) araw->bins.size()) ? araw->bins[(size_t) bin] : (juce::uint8) 0;
    return proc.getWave().at (bin);
}

void LanesView::tick()
{
    ps = proc.getPlayState();
    araw = &proc.araWave();
    if (follow && drag == Drag::none)
    {
        const double inView = (ps.seconds - viewStart) / viewLen;
        if (ps.playing || inView < 0.0 || inView > 1.0) viewStart = juce::jmax (0.0, ps.seconds - viewLen * 0.25);
    }
    repaint();
}

void LanesView::zoomBy (double factor)
{
    const double anchor = (ps.seconds >= viewStart && ps.seconds <= viewStart + viewLen) ? ps.seconds : viewStart + viewLen * 0.5;
    const double newLen = juce::jlimit (0.5, 600.0, viewLen * factor);
    viewStart = juce::jmax (0.0, anchor - (anchor - viewStart) * newLen / viewLen);
    viewLen = newLen;
    repaint();
}

//------------------------------------------------------------------------------
void LanesView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    screenBase (g, r);
    const auto set = proc.getSections();
    paintRuler (g);
    for (int l = 0; l < 2; ++l) paintLane (g, l, set);

    // nothing heard yet
    const bool araBound = araw != nullptr && araw->bound;
    if (! araBound && proc.getWave().lastBin() < 0)
    {
        g.setColour (pal::paintDim.withAlpha (0.75f)); g.setFont (tex::stencil (15.0f).withExtraKerningFactor (0.08f));
        const auto a = laneRect (0).getUnion (laneRect (1));
        g.drawFittedText ("PLAY THE SONG ONCE: THE WAVEFORM IS DRAWN AS IT PLAYS", a.withSizeKeepingCentre (a.getWidth(), 30.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }

    // the playhead
    const float px = timeToX (ps.seconds);
    if (px >= (float) kGutter && px <= (float) getWidth())
    {
        const auto col = ps.playing ? juce::Colour (0xfffff0d8) : pal::paintDim;
        g.setColour (col.withAlpha (0.25f)); g.fillRect (px - 2.0f, (float) kRuler, 5.0f, (float) getHeight() - (float) kRuler);
        g.setColour (col); g.drawLine (px, 2.0f, px, (float) getHeight() - 2.0f, 1.4f);
        juce::Path tri; tri.addTriangle (px - 6.0f, 2.0f, px + 6.0f, 2.0f, px, 11.0f);
        g.fillPath (tri);
    }
    screenGlass (g, r);
}

void LanesView::paintRuler (juce::Graphics& g)
{
    const auto rr = rulerRect();
    g.setColour (juce::Colour (0xff17110e)); g.fillRect (rr);
    g.setColour (juce::Colour (0xff3a2418)); g.drawHorizontalLine ((int) rr.getBottom(), rr.getX(), rr.getRight());
    const double t0 = viewStart, t1 = viewStart + viewLen;
    const float W = (float) (getWidth() - kGutter);
    g.setFont (tex::plainBold (9.0f));
    if (ps.hasMusical && ps.bpm > 0)
    {
        const double qpb = (double) ps.num * 4.0 / (double) juce::jmax (1, ps.den), beatQ = 4.0 / (double) juce::jmax (1, ps.den);
        const double pxPerQ = W / viewLen * 60.0 / ps.bpm;
        int every = 1; while (every * qpb * pxPerQ < 46.0 && every < 4096) every *= 2;
        const int k0 = (int) std::floor ((ppqAt (t0) - ps.barPpq) / qpb) - 1, k1 = (int) std::ceil ((ppqAt (t1) - ps.barPpq) / qpb) + 1;
        for (int k = k0; k <= k1; ++k)
        {
            const double qb = ps.barPpq + k * qpb;
            const int barNum = (int) std::llround (qb / qpb) + 1;
            const float x = timeToX (timeAtPpq (qb));
            if (x >= (float) kGutter && x <= (float) getWidth())
            {
                const bool labelled = ((barNum - 1) % every) == 0;
                g.setColour (pal::paintDim.withAlpha (labelled ? 0.8f : 0.35f));
                g.drawVerticalLine ((int) x, rr.getY() + (labelled ? 8.0f : 14.0f), rr.getBottom());
                if (labelled) { g.setColour (pal::paint.withAlpha (0.85f)); g.drawText (juce::String (barNum), juce::Rectangle<float> (x + 3.0f, rr.getY() + 2.0f, 40.0f, 12.0f), juce::Justification::centredLeft); }
            }
            if (beatQ * pxPerQ >= 10.0)
                for (int j = 1; j < ps.num; ++j)
                {
                    const float bx = timeToX (timeAtPpq (qb + j * beatQ));
                    if (bx >= (float) kGutter && bx <= (float) getWidth()) { g.setColour (pal::paintDim.withAlpha (0.25f)); g.drawVerticalLine ((int) bx, rr.getBottom() - 5.0f, rr.getBottom()); }
                }
        }
    }
    else
    {
        double step = 0.1;
        for (double s : { 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0, 120.0, 300.0 }) { step = s; if (s * W / viewLen >= 56.0) break; }
        for (double t = std::ceil (t0 / step) * step; t <= t1; t += step)
        {
            const float x = timeToX (t);
            g.setColour (pal::paintDim.withAlpha (0.7f)); g.drawVerticalLine ((int) x, rr.getY() + 8.0f, rr.getBottom());
            g.setColour (pal::paint.withAlpha (0.8f));
            g.drawText (timeText (t, step < 1.0), juce::Rectangle<float> (x + 3.0f, rr.getY() + 2.0f, 50.0f, 12.0f), juce::Justification::centredLeft);
        }
    }
    // gutter corner: the zoom
    g.setColour (pal::paintDim.withAlpha (0.7f)); g.setFont (tex::plainBold (8.5f));
    g.drawText (juce::String (viewLen < 10 ? juce::String (viewLen, 1) : juce::String (juce::roundToInt (viewLen))) + " S VIEW",
                juce::Rectangle<float> (4.0f, 4.0f, (float) kGutter - 8.0f, 14.0f), juce::Justification::centredLeft);
}

void LanesView::paintLane (juce::Graphics& g, int lane, const after::SectionSet& set)
{
    const auto r = laneRect (lane);
    const auto col = lane == after::kReverbLane ? pal::signalHot : pal::hazard;
    g.setColour (juce::Colour (0xff120d0b)); g.fillRect (r);
    g.setColour (col.withAlpha (0.18f)); g.drawRect (r, 1.0f);

    // the gutter: name and level scale
    {
        const juce::Rectangle<float> gut (4.0f, r.getY(), (float) kGutter - 10.0f, r.getHeight());
        g.setColour (col); g.setFont (tex::stencil (14.0f).withExtraKerningFactor (0.04f));
        g.drawFittedText (lane == after::kReverbLane ? "REVERB" : "DELAY", gut.withHeight (18.0f).translated (0.0f, 2.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
        g.setFont (tex::plainBold (8.0f)); g.setColour (pal::paintDim.withAlpha (0.6f));
        for (int pct : { 100, 50, 0 })
        {
            const float y = levelToY (lane, (float) pct * 0.01f);
            g.drawText (juce::String (pct), juce::Rectangle<float> (gut.getRight() - 30.0f, y - 5.0f, 26.0f, 10.0f), juce::Justification::centredRight);
            g.drawHorizontalLine ((int) y, gut.getRight() - 2.0f, gut.getRight() + 4.0f);
        }
    }

    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (r.toNearestInt());

    // the waveform
    const float mid = r.getY() + 14.0f + (r.getHeight() - 18.0f) * 0.5f, half = (r.getHeight() - 22.0f) * 0.5f;
    const int x0 = (int) r.getX(), x1 = (int) r.getRight();
    for (int x = x0; x < x1; ++x)
    {
        const double ta = xToTime ((float) x), tb = xToTime ((float) x + 1.0f);
        if (tb < 0) continue;
        const int b0 = juce::jmax (0, (int) (ta / after::kBinSeconds)), b1 = juce::jmax (b0 + 1, (int) (tb / after::kBinSeconds));
        const int step = juce::jmax (1, (b1 - b0) / 48);
        juce::uint8 v = 0;
        for (int b = b0; b < b1; b += step) v = juce::jmax (v, binAt (b));
        if (v == 0) continue;
        const float h = juce::jmax (0.5f, std::pow (after::binHeight (v), 1.6f) * half);
        g.setColour (pal::paintDim.withAlpha (0.34f));
        g.fillRect ((float) x, mid - h, 1.0f, h * 2.0f);
    }

    // the faint grid
    if (gridQuarters() > 0 && gridQuarters() * 60.0 / juce::jmax (1.0, ps.bpm > 0 ? ps.bpm : 120.0) / viewLen * r.getWidth() >= 8.0)
    {
        const double step = gridQuarters();
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        for (double q = std::ceil (ppqAt (viewStart) / step) * step; q <= ppqAt (viewStart + viewLen); q += step)
            g.drawVerticalLine ((int) timeToX (timeAtPpq (q)), r.getY(), r.getBottom());
    }

    // the knob's level: what the lane does between sections
    const float base = baseLevel (lane), yb = levelToY (lane, base);
    {
        juce::Path l; l.startNewSubPath (r.getX(), yb); l.lineTo (r.getRight(), yb);
        juce::Path dashed; const float dl[2] = { 6.0f, 4.0f };
        juce::PathStrokeType (1.1f).createDashedStroke (dashed, l, dl, 2);
        g.setColour (col.withAlpha (0.45f)); g.fillPath (dashed);
        g.setFont (tex::plainBold (8.5f));
        g.drawText ("KNOB " + juce::String (juce::roundToInt (base * 100.0f)) + "%", juce::Rectangle<float> (r.getRight() - 74.0f, yb - 12.0f, 70.0f, 11.0f), juce::Justification::centredRight);
    }

    // the sections
    const bool lanesOn = proc.apvts.getRawParameterValue ("lanes")->load() > 0.5f;
    for (auto& s : set.lanes[(size_t) lane])
    {
        if (s.end < viewStart || s.start > viewStart + viewLen) continue;
        const float xs = timeToX (s.start), xe = timeToX (s.end);
        const double fEff = std::min ((double) s.fadeMs * 0.001, s.length() * 0.5);
        const float fp = (float) (fEff / viewLen * (double) (getWidth() - kGutter));
        const float ys = levelToY (lane, s.send);
        const bool sel = selLane == lane && selId == s.id;
        const auto c = lanesOn ? col : pal::paintDim;
        juce::Path area;
        area.startNewSubPath (xs, r.getBottom()); area.lineTo (xs, yb); area.lineTo (xs + fp, ys); area.lineTo (xe - fp, ys); area.lineTo (xe, yb); area.lineTo (xe, r.getBottom()); area.closeSubPath();
        g.setGradientFill (juce::ColourGradient (c.withAlpha (sel ? 0.38f : 0.24f), 0, juce::jmin (ys, yb), c.withAlpha (0.04f), 0, r.getBottom(), false));
        g.fillPath (area);
        g.setColour (c.withAlpha (sel ? 0.9f : 0.5f));
        g.drawVerticalLine ((int) xs, r.getY(), r.getBottom()); g.drawVerticalLine ((int) xe, r.getY(), r.getBottom());
        juce::Path top; top.startNewSubPath (xs, yb); top.lineTo (xs + fp, ys); top.lineTo (xe - fp, ys); top.lineTo (xe, yb);
        if (sel) { g.setColour (c.withAlpha (0.3f)); g.strokePath (top, juce::PathStrokeType (6.0f)); }
        g.setColour (c); g.strokePath (top, juce::PathStrokeType (sel ? 2.4f : 1.8f));
        if (sel)
        {
            g.setColour (juce::Colour (0xfffff0d8));
            g.fillRect (xs - 2.0f, r.getCentreY() - 9.0f, 4.0f, 18.0f);
            g.fillRect (xe - 2.0f, r.getCentreY() - 9.0f, 4.0f, 18.0f);
        }
        // its label
        juce::String label;
        if (lane == after::kReverbLane) label = s.send <= 0.001f ? juce::String ("KILL") : "SEND " + juce::String (juce::roundToInt (s.send * 100.0f)) + "%";
        else
        {
            label = s.division < 0 ? juce::String ("KNOB TIME") : juce::String (after::divisions()[(size_t) s.division].name);
            label << "  FB " << juce::roundToInt (s.feedback * 100.0f) << "%  " << (s.send <= 0.001f ? juce::String ("KILL") : juce::String (juce::roundToInt (s.send * 100.0f)) + "%");
        }
        if (xe - xs > 24.0f)
        {
            g.setColour (sel ? juce::Colour (0xfffff0d8) : pal::paint.withAlpha (0.85f));
            g.setFont (tex::plainBold (9.0f));
            g.drawFittedText (label, juce::Rectangle<float> (xs + 4.0f, r.getY() + 2.0f, xe - xs - 8.0f, 12.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        }
    }
    if (! lanesOn && ! set.lanes[(size_t) lane].empty())
    {
        g.setColour (pal::paintDim.withAlpha (0.8f)); g.setFont (tex::plainBold (9.0f));
        g.drawText ("SECTIONS OFF", r.reduced (6.0f, 2.0f).removeFromTop (12.0f), juce::Justification::centredRight);
    }
}

//------------------------------------------------------------------------------
LanesView::Hit LanesView::hitTest (juce::Point<float> p) const
{
    const int lane = laneAt (p);
    if (lane < 0) return {};
    const auto set = proc.getSections();
    for (auto& s : set.lanes[(size_t) lane])
    {
        const float xs = timeToX (s.start), xe = timeToX (s.end);
        if (p.x < xs - 5.0f || p.x > xe + 5.0f) continue;
        const bool narrow = xe - xs < 14.0f;
        if (! narrow && std::abs (p.x - xs) <= 5.0f) return { lane, s.id, Drag::resizeL };
        if (! narrow && std::abs (p.x - xe) <= 5.0f) return { lane, s.id, Drag::resizeR };
        if (p.x >= xs && p.x <= xe)
        {
            if (! narrow && std::abs (p.y - levelToY (lane, s.send)) <= 5.0f) return { lane, s.id, Drag::level };
            return { lane, s.id, Drag::move };
        }
        if (narrow) return { lane, s.id, p.x < xs ? Drag::resizeL : Drag::resizeR };
    }
    return { lane, 0, Drag::create };
}

void LanesView::mouseMove (const juce::MouseEvent& e)
{
    if (e.position.y < (float) kRuler) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); return; }
    switch (hitTest (e.position).what)
    {
        case Drag::resizeL: case Drag::resizeR: setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); break;
        case Drag::level:  setMouseCursor (juce::MouseCursor::UpDownResizeCursor); break;
        case Drag::move:   setMouseCursor (juce::MouseCursor::DraggingHandCursor); break;
        case Drag::create: setMouseCursor (juce::MouseCursor::CrosshairCursor); break;
        default:           setMouseCursor (juce::MouseCursor::NormalCursor); break;
    }
}

void LanesView::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto p = e.position;
    if (p.y < (float) kRuler && p.x > (float) kGutter)
    {
        drag = Drag::scroll; dragView0 = viewStart;
        setFollow (false);
        return;
    }
    const auto h = hitTest (p);
    if (e.mods.isPopupMenu()) { showMenu (e, h); return; }
    if (h.lane < 0) return;
    drag = h.what; dragLane = h.lane; dragId = h.id;
    dragT0 = xToTime (p.x);
    if (h.id != 0)
    {
        const auto set = proc.getSections();
        if (const auto* s = set.find (h.lane, h.id)) dragOrig = *s;
        select (h.lane, h.id);
    }
    else
    {
        dragT0 = snapTime (dragT0);
        select (-1, 0);
    }
}

void LanesView::mouseDrag (const juce::MouseEvent& e)
{
    const double t = xToTime (e.position.x);
    if (drag == Drag::scroll)
    {
        const double dt = (double) e.getDistanceFromDragStartX() / juce::jmax (1.0, (double) (getWidth() - kGutter)) * viewLen;
        viewStart = juce::jmax (0.0, dragView0 - dt);
        repaint();
        return;
    }
    if (drag == Drag::none || dragLane < 0) return;
    if (drag == Drag::create)
    {
        if (std::abs (e.getDistanceFromDragStartX()) < 4) return;
        after::Section s;
        const double b = snapTime (t);
        s.start = juce::jmin (dragT0, b); s.end = juce::jmax (dragT0, b);
        if (s.end - s.start < 0.02) s.end = s.start + 0.02;
        s.send = 1.0f;
        s.feedback = proc.apvts.getRawParameterValue ("dl_feedback")->load() * 0.01f;
        s.division = -1;
        const int id = proc.addSection (dragLane, s);
        if (id == 0) { drag = Drag::none; return; }
        dragId = id;
        const auto set = proc.getSections();
        if (const auto* made = set.find (dragLane, id)) dragOrig = *made;
        select (dragLane, id);
        drag = b >= dragT0 ? Drag::resizeR : Drag::resizeL;
        return;
    }
    const auto set = proc.getSections();
    const auto* now = set.find (dragLane, dragId);
    if (now == nullptr) { drag = Drag::none; return; }
    auto s = *now;
    const auto room = set.roomFor (dragLane, dragId, dragOrig.start + dragOrig.length() * 0.5);
    switch (drag)
    {
        case Drag::move:
        {
            const double len = dragOrig.length();
            double ns = snapTime (dragOrig.start + (t - dragT0));
            ns = juce::jlimit (room.getStart(), juce::jmax (room.getStart(), room.getEnd() - len), ns);
            s.start = ns; s.end = ns + len;
            break;
        }
        case Drag::resizeL: s.start = juce::jlimit (room.getStart(), s.end - 0.02, snapTime (t)); break;
        case Drag::resizeR: s.end = juce::jlimit (s.start + 0.02, room.getEnd(), snapTime (t)); break;
        case Drag::level:   s.send = yToLevel (dragLane, e.position.y); break;
        default: return;
    }
    proc.updateSection (dragLane, s);
}

void LanesView::mouseUp (const juce::MouseEvent&) { drag = Drag::none; }

void LanesView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto h = hitTest (e.position);
    if (h.lane < 0 || h.id != 0) return;
    // a section one beat long where you clicked
    after::Section s;
    const double beat = 60.0 / (ps.bpm > 0 ? ps.bpm : 120.0) * 4.0 / (double) juce::jmax (1, ps.den);
    s.start = snapTime (xToTime (e.position.x));
    s.end = s.start + beat;
    s.feedback = proc.apvts.getRawParameterValue ("dl_feedback")->load() * 0.01f;
    if (const int id = proc.addSection (h.lane, s)) select (h.lane, id);
}

void LanesView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const double anchor = xToTime (e.position.x);
        const double f = w.deltaY > 0 ? 0.8 : 1.25;
        const double newLen = juce::jlimit (0.5, 600.0, viewLen * f);
        viewStart = juce::jmax (0.0, anchor - (anchor - viewStart) * newLen / viewLen);
        viewLen = newLen;
    }
    else
    {
        const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? -w.deltaX : -w.deltaY;
        if (std::abs (d) < 1.0e-4f) return;
        viewStart = juce::jmax (0.0, viewStart + (double) d * viewLen * 0.5);
        setFollow (false);
    }
    repaint();
}

bool LanesView::keyPressed (const juce::KeyPress& k)
{
    if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && selId != 0)
    {
        proc.removeSection (selLane, selId);
        select (-1, 0);
        return true;
    }
    return false;
}

void LanesView::showMenu (const juce::MouseEvent& e, Hit h)
{
    if (h.lane < 0) return;
    juce::PopupMenu m;
    const int lane = h.lane;
    const double t = xToTime (e.position.x);
    juce::Component::SafePointer<LanesView> safe (this);
    if (h.id != 0)
    {
        select (lane, h.id);
        const auto set = proc.getSections();
        const auto* sp = set.find (lane, h.id);
        if (sp == nullptr) return;
        const auto s = *sp;
        auto apply = [safe, lane] (after::Section x) { if (safe != nullptr) safe->proc.updateSection (lane, x); };
        m.addSectionHeader (lane == after::kReverbLane ? "REVERB SECTION" : "DELAY SECTION");
        m.addItem ("Full send (100%)", [apply, s] { auto x = s; x.send = 1.0f; apply (x); });
        m.addItem ("Half (50%)", [apply, s] { auto x = s; x.send = 0.5f; apply (x); });
        m.addItem ("Kill (nothing goes in)", [apply, s] { auto x = s; x.send = 0.0f; apply (x); });
        if (lane == after::kDelayLane)
        {
            juce::PopupMenu tm;
            tm.addItem ("The TIME knob", true, s.division < 0, [apply, s] { auto x = s; x.division = -1; apply (x); });
            for (int i = 0; i < (int) after::divisions().size(); ++i)
                tm.addItem (after::divisions()[(size_t) i].name, true, s.division == i, [apply, s, i] { auto x = s; x.division = i; apply (x); });
            m.addSubMenu ("Throw time", tm);
            juce::PopupMenu fm;
            for (int f : { 0, 20, 40, 60, 80, 90 })
                fm.addItem (juce::String (f) + "%", true, juce::roundToInt (s.feedback * 100.0f) == f, [apply, s, f] { auto x = s; x.feedback = (float) f * 0.01f; apply (x); });
            m.addSubMenu ("Throw feedback", fm);
        }
        m.addSeparator();
        m.addItem (lane == after::kReverbLane ? "Copy to the DELAY lane" : "Copy to the REVERB lane", [safe, lane, s]
        {
            if (safe == nullptr) return;
            auto x = s;
            if (const int id = safe->proc.addSection (1 - lane, x)) safe->select (1 - lane, id);
        });
        m.addItem ("Delete section", [safe, lane, id = h.id] { if (safe == nullptr) return; safe->proc.removeSection (lane, id); safe->select (-1, 0); });
    }
    else
    {
        m.addSectionHeader (lane == after::kReverbLane ? "REVERB LANE" : "DELAY LANE");
        m.addItem ("Add a one-beat section here", [safe, lane, t]
        {
            if (safe == nullptr) return;
            after::Section s;
            const double beat = 60.0 / (safe->ps.bpm > 0 ? safe->ps.bpm : 120.0) * 4.0 / (double) juce::jmax (1, safe->ps.den);
            s.start = safe->snapTime (t); s.end = s.start + beat;
            s.feedback = safe->proc.apvts.getRawParameterValue ("dl_feedback")->load() * 0.01f;
            if (const int id = safe->proc.addSection (lane, s)) safe->select (lane, id);
        });
        m.addItem ("Clear this lane", ! proc.getSections().lanes[(size_t) lane].empty(), false, [safe, lane]
        {
            if (safe == nullptr) return;
            safe->proc.clearLane (lane);
            safe->select (-1, 0);
        });
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition());
}

//==============================================================================
Inspector::Inspector (AfterlifeProcessor& p, LanesView& l) : proc (p), lanes (l)
{
    setup (send, "SEND");
    send.slider.setRange (0.0, 100.0, 1.0);
    send.slider.textFromValueFunction = [] (double v) { return v < 0.5 ? juce::String ("KILL") : juce::String (juce::roundToInt (v)) + "%"; };
    send.slider.setTooltip ("How much goes into the effect while this section plays (KILL = none). The knob's SEND applies outside sections.");
    setup (fade, "FADE");
    fade.slider.setRange (0.0, 500.0, 1.0);
    fade.slider.setSkewFactorFromMidPoint (60.0);
    fade.slider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " ms"; };
    fade.slider.setTooltip ("How long the section takes to fade in at its start and out at its end, so nothing clicks.");
    setup (time, "TIME");
    time.slider.setRange (-1.0, (double) after::divisions().size() - 1.0, 1.0);
    time.slider.textFromValueFunction = [] (double v) { const int i = juce::roundToInt (v); return i < 0 ? juce::String ("KNOB") : juce::String (after::divisions()[(size_t) i].name); };
    time.slider.valueFromTextFunction = [] (const juce::String& s) { const int i = after::divisionNames().indexOf (s.trim()); return (double) i; };
    time.slider.setTooltip ("The throw's delay time. KNOB = the TIME knob. The throw's echoes keep this time as they fade, even after the section ends.");
    setup (feedback, "FEEDBACK");
    feedback.slider.setRange (0.0, 95.0, 1.0);
    feedback.slider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + "%"; };
    feedback.slider.setTooltip ("The throw's feedback: how many times it repeats as it fades.");
    for (auto* k : { &send, &fade, &time, &feedback }) k->slider.onValueChange = [this] { push(); };
    addAndMakeVisible (deleteButton);
    deleteButton.setTooltip ("Delete this section (or select it and press DELETE).");
    deleteButton.onClick = [this]
    {
        if (! has) return;
        proc.removeSection (lane, cur.id);
        lanes.select (-1, 0);
    };
    refresh();
}

void Inspector::setup (Knob& k, const juce::String& name)
{
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (tex::stencil (13.0f).withExtraKerningFactor (0.1f));
    k.label.setColour (juce::Label::textColourId, pal::paint);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    k.slider.setColour (juce::Slider::textBoxTextColourId, pal::paint.withAlpha (0.85f));
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff3a2418));
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x90000000));
    k.slider.setMouseDragSensitivity (200);
    addChildComponent (k.slider);
    addChildComponent (k.label);
}

void Inspector::refresh()
{
    lane = lanes.getSelectedLane();
    const auto set = proc.getSections();
    const auto* s = lane >= 0 ? set.find (lane, lanes.getSelectedId()) : nullptr;
    has = s != nullptr;
    if (has) cur = *s;
    const bool dl = has && lane == after::kDelayLane;
    for (auto* k : { &send, &fade }) { k->slider.setVisible (has); k->label.setVisible (has); }
    for (auto* k : { &time, &feedback }) { k->slider.setVisible (dl); k->label.setVisible (dl); }
    deleteButton.setVisible (has);
    if (has)
    {
        send.slider.setValue (cur.send * 100.0, juce::dontSendNotification);
        fade.slider.setValue (cur.fadeMs, juce::dontSendNotification);
        time.slider.setValue (cur.division, juce::dontSendNotification);
        feedback.slider.setValue (cur.feedback * 100.0, juce::dontSendNotification);
        const auto col = lane == after::kReverbLane ? pal::signal : pal::hazard;
        for (auto* k : { &send, &fade, &time, &feedback }) k->slider.setColour (juce::Slider::rotarySliderFillColourId, col);
    }
    resized();
    repaint();
}

void Inspector::push()
{
    if (! has) return;
    cur.send = (float) send.slider.getValue() * 0.01f;
    cur.fadeMs = (float) fade.slider.getValue();
    cur.division = juce::roundToInt (time.slider.getValue());
    cur.feedback = (float) feedback.slider.getValue() * 0.01f;
    proc.updateSection (lane, cur);
}

void Inspector::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (36);
    deleteButton.setBounds (r.removeFromBottom (24).reduced (30, 0));
    r.removeFromBottom (4);
    auto place = [] (Knob& k, juce::Rectangle<int> cell) { k.label.setBounds (cell.removeFromTop (16)); k.slider.setBounds (cell); };
    const int rowH = juce::jmin (84, r.getHeight() / 2);
    auto row1 = r.removeFromTop (rowH);
    place (send, row1.removeFromLeft (row1.getWidth() / 2));
    place (fade, row1);
    auto row2 = r.removeFromTop (rowH);
    place (time, row2.removeFromLeft (row2.getWidth() / 2));
    place (feedback, row2);
}

void Inspector::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto head = r.removeFromTop (34.0f).reduced (4.0f, 0.0f);
    if (! has)
    {
        g.setColour (pal::paintDim); g.setFont (tex::stencil (14.0f).withExtraKerningFactor (0.08f));
        g.drawText ("NO SECTION SELECTED", head.removeFromTop (18.0f), juce::Justification::centredLeft);
        g.setColour (pal::paintDim.withAlpha (0.8f)); g.setFont (tex::plainBold (10.5f));
        g.drawFittedText ("DRAG IN A LANE TO MARK A SECTION:\n\n"
                          "REVERB: MORE (OR NO) REVERB ON A WORD.\n\n"
                          "DELAY: A THROW ON THE END OF A LINE, WITH ITS OWN TIME AND FEEDBACK.\n\n"
                          "CLICK A SECTION TO EDIT IT HERE.",
                          r.reduced (4.0f, 0.0f).toNearestInt(), juce::Justification::topLeft, 12, 0.9f);
        return;
    }
    const auto col = lane == after::kReverbLane ? pal::signalHot : pal::hazard;
    g.setColour (col); g.setFont (tex::stencil (14.0f).withExtraKerningFactor (0.08f));
    g.drawText (lane == after::kReverbLane ? "REVERB SECTION" : "DELAY THROW", head.removeFromTop (18.0f), juce::Justification::centredLeft);
    g.setColour (pal::paintDim); g.setFont (tex::plainBold (9.5f));
    g.drawText (timeText (cur.start, true) + "  TO  " + timeText (cur.end, true) + "   (" + juce::String (cur.length(), 2) + " S)", head, juce::Justification::centredLeft);
}

//==============================================================================
AfterlifeEditor::AfterlifeEditor (AfterlifeProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      rvOnAtt (p.apvts, "rv_on", rvOn), dlOnAtt (p.apvts, "dl_on", dlOn), lanesOnAtt (p.apvts, "lanes", lanesOn),
      syncSel (*p.apvts.getParameter ("dl_sync"), { "FREE", "SYNC" }),
      modeSel (*p.apvts.getParameter ("dl_mode"), { "STEREO", "PING", "MONO" }),
      irScreen (p), echoScreen (p), lanes (p), inspector (p, lanes)
{
    setLookAndFeel (&lnf);

    // ---- reverb
    makeKnob ("rv_send", "SEND", "How much of the track goes into the reverb (outside sections; the REVERB lane's sections override it).");
    makeKnob ("rv_decay", "DECAY", "How long the tail takes to die away (60 dB). With LOADED IR, trims the IR's tail.");
    makeKnob ("rv_predelay", "PRE-DELAY", "A gap before the reverb starts, so the words stay clear in front of it.");
    makeKnob ("rv_width", "WIDTH", "Stereo width of the reverb. 0% = mono, 150% = extra wide.");
    makeKnob ("rv_lowcut", "LOW CUT", "Takes the mud out of the reverb.");
    makeKnob ("rv_highcut", "HIGH CUT", "Darkens the reverb.");
    makeKnob ("rv_duck", "DUCK", "Pushes the reverb down while the track is playing, so it blooms in the gaps.");
    makeKnob ("rv_level", "LEVEL", "The reverb's level in the mix.");
    // ---- delay
    dlDivKnob = &makeKnob ("dl_div", "TIME", "The delay time, in step with the song's tempo.");
    dlMsKnob = &makeKnob ("dl_ms", "TIME", "The delay time in milliseconds.");
    makeKnob ("dl_feedback", "FEEDBACK", "How many times the echo repeats.");
    makeKnob ("dl_tone", "TONE", "Darkens each repeat a little more.");
    makeKnob ("dl_lowcut", "LOW CUT", "Thins each repeat a little more.");
    makeKnob ("dl_dirt", "DIRT", "Tape grit: saturation, wow and flutter on the repeats.");
    makeKnob ("dl_duck", "DUCK", "Pushes the echoes down while the track is playing, so they answer in the gaps.");
    makeKnob ("dl_send", "SEND", "How much of the track goes into the delay (outside sections; the DELAY lane's sections override it).");
    makeKnob ("dl_level", "LEVEL", "The delay's level in the mix.");
    // ---- both
    makeKnob ("dry", "DRY", "The untouched track. Turn it to 0% on an FX bus.");
    makeKnob ("output", "OUTPUT", "Final output trim.");

    spaceBox.addItemList (after::spaceNames(), 1);
    spaceBox.setJustificationType (juce::Justification::centredLeft);
    spaceAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "rv_space", spaceBox);
    spaceBox.setTooltip ("The room the reverb is built from. LOADED IR uses an impulse response file you load.");
    addAndMakeVisible (spaceBox);
    loadIrButton.setTooltip ("Load your own impulse response (WAV, AIFF or FLAC, up to 16 seconds).");
    loadIrButton.onClick = [this] { chooseIr(); };
    addAndMakeVisible (loadIrButton);
    rvOn.setTooltip ("Reverb on / off."); dlOn.setTooltip ("Delay on / off.");
    addAndMakeVisible (rvOn); addAndMakeVisible (dlOn);
    syncSel.setTooltip ("SYNC: the delay time follows the song's tempo. FREE: set it in milliseconds.");
    modeSel.setTooltip ("STEREO: left echoes left, right echoes right. PING: the echoes bounce side to side. MONO: one echo in the middle.");
    addAndMakeVisible (syncSel); addAndMakeVisible (modeSel);
    addAndMakeVisible (irScreen); addAndMakeVisible (echoScreen);
    irScreen.setInterceptsMouseClicks (false, false); echoScreen.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (gauge); gauge.setInterceptsMouseClicks (false, false);

    // ---- lanes
    followButton.setToggleState (true, juce::dontSendNotification);
    followButton.setTooltip ("The lanes scroll with your DAW's playhead. Scrolling or dragging the ruler turns it off.");
    followButton.onClick = [this] { lanes.setFollow (followButton.getToggleState()); };
    lanes.onFollowChanged = [this] { followButton.setToggleState (lanes.isFollowing(), juce::dontSendNotification); };
    snapBox.addItemList ({ "SNAP OFF", "SNAP BEAT", "SNAP 1/8", "SNAP 1/16" }, 1);
    snapBox.setSelectedId (3, juce::dontSendNotification);
    snapBox.onChange = [this] { lanes.setSnap (snapBox.getSelectedId() - 1); };
    snapBox.setTooltip ("Section edges snap to the song's grid.");
    zoomOut.onClick = [this] { lanes.zoomBy (1.5); };
    zoomIn.onClick = [this] { lanes.zoomBy (1.0 / 1.5); };
    zoomOut.setTooltip ("Zoom out"); zoomIn.setTooltip ("Zoom in");
    lanesOn.setTooltip ("Sections on / off. Off: both effects follow their knobs all the way through (handy to compare).");
    forgetButton.setTooltip ("Clear the waveform drawn so far (it's drawn again as the song plays). Your sections stay.");
    forgetButton.onClick = [this] { proc.forgetWave(); };
    for (juce::Component* c : { (juce::Component*) &followButton, (juce::Component*) &snapBox, (juce::Component*) &zoomOut, (juce::Component*) &zoomIn,
                                (juce::Component*) &lanesOn, (juce::Component*) &forgetButton, (juce::Component*) &lanes, (juce::Component*) &inspector })
        addAndMakeVisible (c);
    lanes.onSelectionChanged = [this] { inspector.refresh(); };

    // ---- presets
    addAndMakeVisible (presetBox);
    presetBox.setJustificationType (juce::Justification::centredLeft);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id >= kUserIdBase) { if (id - kUserIdBase < userFiles.size()) proc.loadUserPreset (userFiles[id - kUserIdBase]); }
        else if (id - 1 != proc.getCurrentProgram() || proc.getCurrentPresetName() != proc.getProgramName (id - 1)) proc.setCurrentProgram (id - 1);
    };
    for (auto* b : { &prevButton, &nextButton, &saveButton, &deleteButton }) addAndMakeVisible (*b);
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { promptSave(); };
    deleteButton.onClick = [this] { confirmDelete(); };
    refreshPresetBox();

    setSize (1120, 800);
    timerCallback();   // shows the right TIME knob from the start
    startTimerHz (30);
}

AfterlifeEditor::~AfterlifeEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

AfterlifeEditor::Knob& AfterlifeEditor::makeKnob (const juce::String& id, const juce::String& text, const juce::String& tip)
{
    auto k = std::make_unique<Knob>();
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (tex::stencil (14.0f).withExtraKerningFactor (0.1f));
    k->label.setColour (juce::Label::textColourId, pal::paint);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 17);
    k->slider.setColour (juce::Slider::textBoxTextColourId, pal::paint.withAlpha (0.85f));
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff3a2418));
    k->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x90000000));
    k->slider.setMouseDragSensitivity (220);
    k->slider.setTooltip (tip);
    k->slider.setName (id);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    k->slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (proc.apvts.getParameter (id)->getDefaultValue()));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

void AfterlifeEditor::placeKnob (Knob& k, juce::Rectangle<int> cell)
{
    k.label.setBounds (cell.removeFromTop (18));
    k.slider.setBounds (cell);
}

void AfterlifeEditor::resized()
{
    plates.clear();
    gauge.setBounds (20, 14, 86, 86);
    badgeArea  = { 640, 14, 252, 86 };
    crestArea  = { 648, 19, 76, 76 };
    presetArea = { 904, 14, 200, 86 };
    presetBox.setBounds (904, 36, 200, 26);
    prevButton.setBounds (904, 70, 28, 24);
    nextButton.setBounds (936, 70, 28, 24);
    saveButton.setBounds (970, 70, 62, 24);
    deleteButton.setBounds (1040, 70, 64, 24);
    stripeArea = { 0, 110, getWidth(), 10 };

    auto knobAt = [this] (const char* id) -> Knob&
    {
        for (auto& k : knobs) if (k->slider.getName() == id) return *k;
        jassertfalse; return *knobs.front();
    };
    auto grid = [&] (std::initializer_list<const char*> ids, int x, int y)
    {
        int i = 0;
        for (auto* id : ids)
        {
            placeKnob (knobAt (id), { x + (i % 4) * 79, y + (i / 4) * 114, 79, 112 });
            ++i;
        }
    };

    plates.push_back ({ "REVERB", { 16, 130, 540, 252 }, true });
    spaceBox.setBounds (30, 156, 190, 26);
    loadIrButton.setBounds (30, 188, 104, 24);
    rvOn.setBounds (140, 188, 80, 24);
    irScreen.setBounds (30, 218, 190, 152);
    grid ({ "rv_send", "rv_decay", "rv_predelay", "rv_width", "rv_lowcut", "rv_highcut", "rv_duck", "rv_level" }, 232, 150);

    plates.push_back ({ "DELAY", { 566, 130, 538, 252 }, true });
    syncSel.setBounds (578, 154, 192, 30);
    modeSel.setBounds (578, 186, 192, 30);
    echoScreen.setBounds (580, 220, 190, 122);
    dlOn.setBounds (580, 348, 190, 24);
    grid ({ "dl_div", "dl_feedback", "dl_tone", "dl_lowcut", "dl_dirt", "dl_duck", "dl_send", "dl_level" }, 780, 150);
    placeKnob (*dlMsKnob, { 780, 150, 79, 112 });

    plates.push_back ({ "THE LANES", { 16, 392, 880, 392 } });
    followButton.setBounds (30, 420, 96, 24);
    snapBox.setBounds (134, 420, 110, 24);
    zoomOut.setBounds (252, 420, 28, 24);
    zoomIn.setBounds (284, 420, 28, 24);
    lanesOn.setBounds (320, 420, 130, 24);
    forgetButton.setBounds (458, 420, 112, 24);
    statusArea = { 580, 420, 304, 24 };
    lanes.setBounds (28, 452, 856, 320);

    plates.push_back ({ "SECTION", { 906, 392, 198, 252 } });
    inspector.setBounds (914, 416, 182, 222);
    plates.push_back ({ "OUTPUT", { 906, 652, 198, 132 } });
    placeKnob (knobAt ("dry"), { 914, 676, 90, 102 });
    placeKnob (knobAt ("output"), { 1006, 676, 90, 102 });

    buildBackdrop();
}

//==============================================================================
void AfterlifeEditor::buildBackdrop()
{
    const float scale = 2.0f;
    const int W = getWidth(), H = getHeight();
    if (W <= 0 || H <= 0) return;
    const int PW = (int) ((float) W * scale), PH = (int) ((float) H * scale);
    juce::Image base (juce::Image::RGB, W, H, false);

    std::vector<juce::Rectangle<float>> pr;
    for (auto& s : plates) pr.push_back (s.bounds.toFloat());
    pr.push_back (badgeArea.toFloat());
    const auto stripe = stripeArea.toFloat();

    {
        juce::Image::BitmapData bd (base, juce::Image::BitmapData::writeOnly);
        for (int py = 0; py < H; ++py)
        {
            const float y = (float) py;
            for (int px = 0; px < W; ++px)
            {
                const float x = (float) px;
                tex::RGB col;
                int inside = -1; float edge = 1e9f; bool shadow = false;
                for (int i = 0; i < (int) pr.size(); ++i)
                {
                    const auto& p = pr[(size_t) i];
                    if (p.contains (x, y))
                    {
                        inside = i;
                        edge = std::min ({ x - p.getX(), p.getRight() - x, y - p.getY(), p.getBottom() - y });
                        break;
                    }
                    if (p.translated (3.0f, 4.0f).contains (x, y)) shadow = true;
                }
                if (stripe.contains (x, y))
                {
                    const bool yellow = std::fmod (x + y * 1.0f + 1000.0f, 28.0f) < 14.0f;
                    const float wear = tex::fbm (x / 12.0f, y / 4.0f, 2024, 3);
                    col = yellow ? tex::rgb (pal::hazard) : tex::rgb (juce::Colour (0xff141010));
                    col = tex::mix (col, tex::rustAt (x, y, 0.3f, 99), tex::smoothstep (0.5f, 0.68f, wear));
                    col = tex::mul (col, 0.85f + 0.15f * tex::hash (px, py, 3));
                }
                else if (inside >= 0)
                {
                    const float d = tex::fbm (x / 30.0f, y / 30.0f, 211, 3);
                    tex::RGB steel = tex::mix (tex::rgb (juce::Colour (0xff1e1a17)), tex::rgb (juce::Colour (0xff332c27)), d);
                    const float u = std::fmod (x + y + 2000.0f, 16.0f) / 16.0f, v = std::fmod (x - y + 2000.0f, 16.0f) / 16.0f;
                    const float tread = (std::abs (u - 0.5f) < 0.08f && std::abs (v - 0.25f) < 0.18f) || (std::abs (v - 0.5f) < 0.08f && std::abs (u - 0.75f) < 0.18f) ? 1.0f : 0.0f;
                    steel = tex::mix (steel, tex::rgb (juce::Colour (0xff4a4038)), tread * 0.55f);
                    const float edgeRust = (1.0f - juce::jlimit (0.0f, 1.0f, edge / 14.0f)) * tex::smoothstep (0.35f, 0.6f, tex::vnoise (x / 6.0f, y / 6.0f, 17));
                    const float bloom = tex::smoothstep (0.6f, 0.75f, tex::fbm (x / 55.0f, y / 55.0f, 404, 3)) * 0.7f;
                    col = tex::mix (steel, tex::rustAt (x, y, 0.15f, 1337), juce::jlimit (0.0f, 1.0f, edgeRust + bloom));
                    col = tex::mul (col, 0.88f);
                }
                else
                {
                    col = tex::rustAt (x, y, 0.1f, 42);
                    if (shadow) col = tex::mul (col, 0.45f);
                }
                const float streak = tex::fbm (x / 9.0f, y / 240.0f, 909, 3);
                const float drip = tex::smoothstep (0.58f, 0.78f, streak) * (0.25f + 0.5f * y / (float) H);
                col = tex::mul (col, 1.0f - drip * 0.6f);
                const float runoff = tex::smoothstep (0.66f, 0.82f, tex::fbm (x / 5.0f, y / 120.0f, 31337, 2));
                col = tex::mix (col, tex::rgb (pal::rustDeep), runoff * 0.35f);
                const float oil = tex::smoothstep (0.7f, 0.8f, tex::fbm (x / 70.0f, y / 50.0f, 555, 3));
                col = tex::mul (col, 1.0f - oil * 0.45f);
                if (tex::hash (px, py, 5) > 0.988f) col = tex::mul (col, 0.45f);
                const float grime = 0.72f + 0.28f * tex::fbm (x / 260.0f, y / 260.0f, 77, 2);
                const float dx = (x / (float) W - 0.5f) * 2.0f, dy = (y / (float) H - 0.5f) * 2.0f;
                const float vig = 1.0f - 0.45f * std::min (1.0f, (dx * dx + dy * dy) * 0.6f);
                col = tex::mul (col, grime * vig);
                bd.setPixelColour (px, py, juce::Colour::fromFloatRGBA (juce::jlimit (0.0f, 1.0f, col.r), juce::jlimit (0.0f, 1.0f, col.g),
                                                                        juce::jlimit (0.0f, 1.0f, col.b), 1.0f));
            }
        }
    }

    backdrop = juce::Image (juce::Image::RGB, PW, PH, false);
    juce::Graphics g (backdrop);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (base, 0, 0, PW, PH, 0, 0, W, H);
    g.addTransform (juce::AffineTransform::scale (scale));
    juce::Random rnd (1991);

    auto rivet = [&] (float cx, float cy, float rad)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (cx - rad + 1, cy - rad + 1.5f, rad * 2, rad * 2);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a7a68), cx - rad * 0.4f, cy - rad * 0.5f, juce::Colour (0xff2a201a), cx + rad, cy + rad, true));
        g.fillEllipse (cx - rad, cy - rad, rad * 2, rad * 2);
        g.setColour (pal::rust.withAlpha (0.55f)); g.drawEllipse (cx - rad, cy - rad, rad * 2, rad * 2, 1.2f);
        g.setColour (pal::rustDeep.withAlpha (0.35f)); g.fillRect (cx - 0.8f, cy + rad, 1.6f, 3.0f + rnd.nextFloat() * 9.0f);
    };
    auto plateFrame = [&] (juce::Rectangle<float> p, bool hot)
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawLine (p.getX() + 2, p.getY() + 1, p.getRight() - 2, p.getY() + 1, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawRect (p, 1.0f);
        if (hot) { g.setColour (pal::signal.withAlpha (0.45f)); g.drawRect (p.reduced (2.0f), 1.5f); }
        rivet (p.getX() + 6, p.getY() + 6, 3.0f); rivet (p.getRight() - 6, p.getY() + 6, 3.0f);
        rivet (p.getX() + 6, p.getBottom() - 6, 3.0f); rivet (p.getRight() - 6, p.getBottom() - 6, 3.0f);
    };

    for (int i = 0; i < 360; ++i)
    {
        const float x = rnd.nextFloat() * (float) W, y = rnd.nextFloat() * (float) H;
        const float len = 6.0f + rnd.nextFloat() * 70.0f, ang = rnd.nextFloat() * juce::MathConstants<float>::pi;
        g.setColour ((rnd.nextBool() ? juce::Colour (0xffd0c0a8) : juce::Colours::black).withAlpha (0.04f + rnd.nextFloat() * 0.1f));
        g.drawLine (x, y, x + std::cos (ang) * len, y + std::sin (ang) * len, 0.5f + rnd.nextFloat() * 0.6f);
    }

    int seed = 10;
    for (auto& s : plates)
    {
        const auto p = s.bounds.toFloat();
        plateFrame (p, s.hot);
        auto title = tex::distressedText (s.title, s.bounds.getWidth() - 40, 18, tex::stencil (15.0f), s.hot ? pal::signalHot : pal::paint,
                                          juce::Justification::centredLeft, scale, 0.2f, seed++, 0.12f);
        g.drawImage (title, juce::Rectangle<float> (p.getX() + 14, p.getY() + 3, (float) s.bounds.getWidth() - 40, 18.0f));
    }
    // the delay plate's title in hazard yellow over the red one
    {
        auto t = tex::distressedText ("DELAY", 120, 18, tex::stencil (15.0f), pal::hazard, juce::Justification::centredLeft, scale, 0.2f, 77, 0.12f);
        g.setColour (juce::Colour (0xff1e1a17)); g.fillRect (580.0f, 134.0f, 70.0f, 15.0f);
        g.drawImage (t, juce::Rectangle<float> (580.0f, 133.0f, 120.0f, 18.0f));
    }
    // stamped dividers between the screens and the knobs
    for (float x : { 226.0f, 775.0f })
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawLine (x, 152.0f, x, 372.0f, 1.5f);
        g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawLine (x + 1.5f, 152.0f, x + 1.5f, 372.0f, 1.0f);
    }
    // screen wells
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    for (auto* c : { (juce::Component*) &irScreen, (juce::Component*) &echoScreen, (juce::Component*) &lanes })
        g.fillRoundedRectangle (c->getBounds().toFloat().expanded (3.0f), 5.0f);
    g.fillRoundedRectangle (inspector.getBounds().toFloat().expanded (3.0f), 5.0f);
    g.fillRoundedRectangle (statusArea.toFloat(), 4.0f);

    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawLine (0, stripe.getY(), (float) W, stripe.getY(), 1.5f);
    g.drawLine (0, stripe.getBottom(), (float) W, stripe.getBottom(), 1.5f);

    // CASTLE DANGER MUSIC COMPANY badge with the castle crest
    const auto badge = badgeArea.toFloat();
    plateFrame (badge, false);
    g.setColour (pal::paint.withAlpha (0.22f)); g.drawRect (badge.reduced (5.0f), 1.0f);
    drawCastleCrest (g, crestArea.toFloat());
    const float tx = (float) crestArea.getRight() + 8.0f, tw = badge.getRight() - tx - 10.0f;
    auto l1 = tex::distressedText ("CASTLE DANGER", (int) tw, 28, tex::stencil (25.0f), juce::Colour (0xffeadfc4), juce::Justification::centredLeft, scale, 0.24f, 4242, 0.08f);
    auto l2 = tex::distressedText ("MUSIC COMPANY", (int) tw, 20, tex::stencil (17.0f), pal::rustLight, juce::Justification::centredLeft, scale, 0.24f, 4343, 0.2f);
    auto l3 = tex::distressedText ("WE MAKE MUSIC", (int) tw, 14, tex::plainBold (9.0f), pal::paintDim, juce::Justification::centredLeft, scale, 0.15f, 4444, 0.3f);
    g.setColour (juce::Colours::black);
    g.setOpacity (0.7f); g.drawImage (l1, juce::Rectangle<float> (tx + 1.5f, badge.getY() + 13.0f, tw, 28.0f));
    g.setOpacity (1.0f); g.drawImage (l1, juce::Rectangle<float> (tx, badge.getY() + 11.0f, tw, 28.0f));
    g.setOpacity (0.7f); g.drawImage (l2, juce::Rectangle<float> (tx + 1.5f, badge.getY() + 40.0f, tw, 20.0f));
    g.setOpacity (1.0f); g.drawImage (l2, juce::Rectangle<float> (tx, badge.getY() + 38.0f, tw, 20.0f));
    g.drawImage (l3, juce::Rectangle<float> (tx + 1.0f, badge.getY() + 60.0f, tw, 14.0f));

    auto sub = tex::distressedText ("THE ECHOES NEVER DIE  //  V" + juce::String (JucePlugin_VersionString),
                                    360, 14, tex::plainBold (10.5f), pal::paintDim, juce::Justification::centredLeft, scale, 0.15f, 99, 0.14f);
    g.drawImage (sub, juce::Rectangle<float> (118.0f, 84.0f, 360.0f, 14.0f));

    auto pl = tex::distressedText ("PRESET", 120, 16, tex::stencil (14.0f), pal::paint, juce::Justification::centredLeft, scale, 0.2f, 7, 0.15f);
    g.drawImage (pl, juce::Rectangle<float> ((float) presetArea.getX() + 2, (float) presetArea.getY() + 2, 120.0f, 16.0f));

    // the logo: AFTER in rusty red, LIFE in cream (drawn live so it can flicker)
    {
        const int lw = 440, lh = 70;
        logoImage = juce::Image (juce::Image::ARGB, (int) (lw * scale), (int) (lh * scale), true);
        juce::Graphics lg (logoImage);
        lg.addTransform (juce::AffineTransform::scale (scale));
        auto f = tex::stencil (66.0f).withExtraKerningFactor (0.06f);
        {
            juce::GlyphArrangement all; all.addLineOfText (f, "AFTERLIFE", 0, 0);
            const float wAll = all.getBoundingBox (0, -1, true).getWidth();
            if (wAll > (float) lw - 4.0f) f = f.withHeight (66.0f * ((float) lw - 4.0f) / wAll);
        }
        lg.setFont (f);
        lg.setColour (pal::signal);
        lg.drawText ("AFTER", 0, 0, lw, lh, juce::Justification::centredLeft, false);
        juce::GlyphArrangement ga; ga.addLineOfText (f, "AFTER", 0, 0);
        const float split = ga.getBoundingBox (0, -1, true).getWidth();
        lg.setColour (pal::paint);
        lg.drawText ("LIFE", (int) split, 0, lw - (int) split, lh, juce::Justification::centredLeft, false);
        tex::distress (logoImage, scale, 0.3f, 666);
    }
}

void AfterlifeEditor::paint (juce::Graphics& g)
{
    if (backdrop.isValid()) g.drawImage (backdrop, getLocalBounds().toFloat());
    else g.fillAll (pal::ironDark);

    if (logoImage.isValid())
    {
        const float alpha = juce::jlimit (0.45f, 1.0f, 1.0f - flicker * 0.6f);
        const auto la = juce::Rectangle<float> (116.0f, 12.0f, 440.0f, 70.0f);
        g.setColour (juce::Colours::black);
        g.setOpacity (0.65f); g.drawImage (logoImage, la.translated (2.5f, 3.5f));
        g.setOpacity (alpha); g.drawImage (logoImage, la);
        g.setOpacity (1.0f);
    }

    // the lanes' status
    {
        auto s = statusArea.toFloat().reduced (8.0f, 0.0f);
        const auto ps = proc.getPlayState();
        const auto& aw = proc.araWave();
        juce::String right;
        bool hot = false;
        if (aw.bound) { right = "ARA  //  " + juce::String (aw.read) + " OF " + juce::String (aw.clips) + (aw.clips == 1 ? " CLIP READ" : " CLIPS READ"); hot = aw.read > 0; }
        else if (proc.getWave().lastBin() < 0) right = "WAVEFORM: PLAY THE SONG";
        else right = ps.playing ? "DRAWING AS IT PLAYS" : "WAVEFORM DRAWN";
        lamp (g, juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ s.getX() + 4.0f, s.getCentreY() }), ps.playing, pal::signalHot);
        g.setFont (tex::stencil (13.0f).withExtraKerningFactor (0.06f));
        g.setColour (ps.playing ? pal::paint : pal::paintDim);
        juce::String left = timeText (ps.seconds, true);
        if (ps.bpm > 0) left << "  " << juce::String (ps.bpm, ps.bpm == std::floor (ps.bpm) ? 0 : 1) << " BPM";
        g.drawText (left, s.withTrimmedLeft (14.0f), juce::Justification::centredLeft);
        g.setFont (tex::plainBold (9.0f).withExtraKerningFactor (0.08f));
        g.setColour (hot ? pal::hazard : pal::paintDim);
        g.drawText (right, s, juce::Justification::centredRight);
    }
}

void AfterlifeEditor::timerCallback()
{
    auto& e = proc.getEngine();
    const float wet = e.wetPeak.exchange (0.0f);
    e.inPeak.store (0.0f);
    wetMeter = std::max (wet, wetMeter * 0.88f);
    gauge.set (juce::Decibels::gainToDecibels (wetMeter, -60.0f));
    gauge.repaint();
    irScreen.repaint();
    echoScreen.repaint();
    lanes.tick();

    const bool sync = proc.apvts.getRawParameterValue ("dl_sync")->load() > 0.5f;
    if (dlDivKnob->slider.isVisible() != sync || dlMsKnob->slider.isVisible() == sync)
    {
        dlDivKnob->slider.setVisible (sync); dlDivKnob->label.setVisible (sync);
        dlMsKnob->slider.setVisible (! sync); dlMsKnob->label.setVisible (! sync);
    }

    // the logo flickers with the tail
    auto& rnd = juce::Random::getSystemRandom();
    const float before = flicker;
    const float work = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (wetMeter, -60.0f) + 40.0f) / 40.0f);
    flicker = (rnd.nextFloat() < 0.015f + work * 0.08f) ? rnd.nextFloat() : flicker * 0.5f;
    if (std::abs (flicker - before) > 0.01f) repaint (116, 10, 446, 76);
    repaint (statusArea);

    if (proc.sectionsVersion.load() != lastSectionsVersion) { lastSectionsVersion = proc.sectionsVersion.load(); inspector.refresh(); }
    if (proc.presetVersion.load() != lastPresetVersion) refreshPresetBox();
}

void AfterlifeEditor::chooseIr()
{
    chooser = std::make_unique<juce::FileChooser> ("Load an impulse response", proc.getIrFile().existsAsFile() ? proc.getIrFile().getParentDirectory() : juce::File(),
                                                   "*.wav;*.aif;*.aiff;*.flac");
    juce::Component::SafePointer<AfterlifeEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (f == juce::File()) return;
        juce::String err;
        if (! safe->proc.loadIrFile (f, err)) safe->showMessage ("COULDN'T LOAD THE IR", err);
    });
}

//==============================================================================
void AfterlifeEditor::refreshPresetBox()
{
    lastPresetVersion = proc.presetVersion.load();
    userFiles = AfterlifeProcessor::getUserPresets();
    const auto current = proc.getCurrentPresetName();
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("Factory");
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.addSeparator();
    presetBox.addSectionHeading ("User Presets");
    if (userFiles.isEmpty()) { presetBox.addItem ("(none saved yet)", 999); presetBox.setItemEnabled (999, false); }
    for (int i = 0; i < userFiles.size(); ++i) presetBox.addItem (userFiles[i].getFileNameWithoutExtension(), kUserIdBase + i);
    int sel = 0;
    for (int i = 0; i < userFiles.size(); ++i)
        if (userFiles[i].getFileNameWithoutExtension() == juce::File::createLegalFileName (current)) sel = kUserIdBase + i;
    if (sel == 0 && proc.getProgramName (proc.getCurrentProgram()) == current) sel = proc.getCurrentProgram() + 1;
    if (sel != 0) presetBox.setSelectedId (sel, juce::dontSendNotification);
    else presetBox.setText (current, juce::dontSendNotification);
    deleteButton.setEnabled (sel >= kUserIdBase);
}

void AfterlifeEditor::stepPreset (int delta)
{
    std::vector<int> ids;
    for (int i = 0; i < proc.getNumPrograms(); ++i) ids.push_back (i + 1);
    for (int i = 0; i < userFiles.size(); ++i) ids.push_back (kUserIdBase + i);
    auto it = std::find (ids.begin(), ids.end(), presetBox.getSelectedId());
    const int pos = it == ids.end() ? 0 : (int) (it - ids.begin());
    const int n = (int) ids.size();
    presetBox.setSelectedId (ids[(size_t) (((pos + delta) % n + n) % n)], juce::sendNotificationSync);
}

void AfterlifeEditor::showDialog (const juce::String& title, const juce::String& message, bool withTextField, const juce::String& initialText,
                                  juce::StringArray buttons, std::function<void (int, const juce::String&)> onResult)
{
    juce::Component::SafePointer<AfterlifeEditor> safe (this);
    dialog = std::make_unique<AfterDialog> (title, message, withTextField, initialText, buttons,
        [safe, onResult] (int index, const juce::String& text)
        {
            juce::MessageManager::callAsync ([safe, onResult, index, text]
            {
                if (safe == nullptr) return;
                safe->dialog.reset();
                if (onResult) onResult (index, text);
            });
        });
    addAndMakeVisible (*dialog);
    dialog->setBounds (getLocalBounds());
}

void AfterlifeEditor::showMessage (const juce::String& title, const juce::String& text) { showDialog (title, text, false, {}, { "OK" }, {}); }

void AfterlifeEditor::promptSave()
{
    const bool onUser = presetBox.getSelectedId() >= kUserIdBase;
    juce::Component::SafePointer<AfterlifeEditor> safe (this);
    auto doSave = [safe] (const juce::String& name)
    {
        if (safe == nullptr) return;
        if (! safe->proc.saveUserPreset (name))
            safe->showMessage ("COULDN'T SAVE", "Afterlife couldn't write the preset file to:\n" + AfterlifeProcessor::getUserPresetFolder().getFullPathName());
        else safe->refreshPresetBox();
    };
    showDialog ("SAVE PRESET", "Name your preset (the knobs only: sections stay with the song):", true, onUser ? proc.getCurrentPresetName() : juce::String(), { "Save", "Cancel" },
        [safe, doSave] (int button, const juce::String& text)
        {
            if (button != 0 || safe == nullptr) return;
            const auto name = text.trim();
            if (name.isEmpty()) { safe->showMessage ("SAVE PRESET", "Please enter a name for the preset."); return; }
            for (int i = 0; i < safe->proc.getNumPrograms(); ++i)
                if (safe->proc.getProgramName (i).equalsIgnoreCase (name))
                {
                    safe->showMessage ("SAVE PRESET", "\"" + name + "\" is a factory preset name. Please choose another name.");
                    return;
                }
            if (AfterlifeProcessor::fileForPresetName (name).existsAsFile())
            {
                safe->showDialog ("REPLACE PRESET?", "A preset named \"" + name + "\" already exists. Replace it with the current settings?",
                                  false, {}, { "Replace", "Cancel" },
                                  [doSave, name] (int b, const juce::String&) { if (b == 0) doSave (name); });
                return;
            }
            doSave (name);
        });
}

void AfterlifeEditor::confirmDelete()
{
    const int id = presetBox.getSelectedId();
    if (id < kUserIdBase || id - kUserIdBase >= userFiles.size()) return;
    const auto file = userFiles[id - kUserIdBase];
    juce::Component::SafePointer<AfterlifeEditor> safe (this);
    showDialog ("DELETE PRESET?", "Delete \"" + file.getFileNameWithoutExtension() + "\"? This can't be undone.", false, {},
                { "Delete", "Cancel" }, [safe, file] (int b, const juce::String&)
                {
                    if (b != 0 || safe == nullptr) return;
                    file.deleteFile();
                    safe->proc.setCurrentProgram (0);
                    safe->refreshPresetBox();
                });
}


//==============================================================================
AfterDialog::AfterDialog (const juce::String& t, const juce::String& m, bool withTextField, const juce::String& initialText,
                        juce::StringArray names, std::function<void (int, const juce::String&)> cb)
    : title (t), message (m), hasField (withTextField), onClose (std::move (cb))
{
    setWantsKeyboardFocus (true);
    if (hasField)
    {
        field.setText (initialText, false);
        field.setInputRestrictions (60);
        field.setSelectAllWhenFocused (true);
        field.setFont (tex::plainBold (15.0f));
        field.setIndents (8, 6);
        field.onReturnKey = [this] { finish (0); };
        field.onEscapeKey = [this] { finish ((int) buttons.size() - 1); };
        addAndMakeVisible (field);
    }
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (names[i]));
        b->onClick = [this, i] { finish (i); };
        addAndMakeVisible (b);
    }
}

void AfterDialog::parentHierarchyChanged()
{
    if (isShowing())
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<AfterDialog> (this)]
        {
            if (sp == nullptr) return;
            if (sp->hasField) sp->field.grabKeyboardFocus(); else sp->grabKeyboardFocus();
        });
}

void AfterDialog::finish (int index)
{
    if (onClose == nullptr) return;
    auto cb = std::move (onClose);
    onClose = nullptr;
    cb (index, field.getText());
}

bool AfterDialog::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::returnKey) { finish (0); return true; }
    if (k == juce::KeyPress::escapeKey) { finish ((int) buttons.size() - 1); return true; }
    return true;
}

void AfterDialog::resized()
{
    panel = getLocalBounds().withSizeKeepingCentre (400, hasField ? 196 : 166);
    auto r = panel.reduced (22);
    r.removeFromTop (30);
    r.removeFromTop (hasField ? 22 : 50);
    if (hasField) { field.setBounds (r.removeFromTop (32)); r.removeFromTop (16); }
    auto row = r.removeFromBottom (28);
    for (int i = (int) buttons.size() - 1; i >= 0; --i) { buttons[i]->setBounds (row.removeFromRight (96)); row.removeFromRight (8); }
}

void AfterDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.72f));
    auto p = panel.toFloat();
    g.setColour (juce::Colour (0xff1a1411)); g.fillRect (p);
    g.setGradientFill (juce::ColourGradient (pal::rustDeep.withAlpha (0.5f), p.getX(), p.getY(), juce::Colours::transparentBlack, p.getX(), p.getY() + p.getHeight() * 0.6f, false));
    g.fillRect (p);
    g.setColour (pal::rust); g.drawRect (p, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRect (p.reduced (2.0f), 1.0f);
    for (auto pt : { juce::Point<float> (p.getX() + 8, p.getY() + 8), { p.getRight() - 8, p.getY() + 8 }, { p.getX() + 8, p.getBottom() - 8 }, { p.getRight() - 8, p.getBottom() - 8 } })
    {
        g.setColour (juce::Colour (0xff6a5a4c)); g.fillEllipse (pt.x - 3, pt.y - 3, 6, 6);
        g.setColour (pal::rust.withAlpha (0.6f)); g.drawEllipse (pt.x - 3, pt.y - 3, 6, 6, 1.0f);
    }
    auto r = panel.reduced (22);
    g.setColour (pal::signalHot); g.setFont (tex::stencil (19.0f).withExtraKerningFactor (0.1f));
    g.drawText (title, r.removeFromTop (30), juce::Justification::centredLeft);
    g.setColour (pal::paint); g.setFont (tex::plainBold (13.0f));
    g.drawFittedText (message, r.removeFromTop (hasField ? 22 : 50), juce::Justification::topLeft, 3);
}
