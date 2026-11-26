#include "PluginEditor.h"
#include <cmath>

namespace
{
    const juce::Colour bgTop       { 0xff161920 };
    const juce::Colour bgBottom    { 0xff0b0d11 };
    const juce::Colour panelColour { 0xff171a20 };
    const juce::Colour panelDark   { 0xff0e1014 };
    const juce::Colour lineColour  { 0xff272b35 };
    const juce::Colour textColour  { 0xffd5d9e0 };

    constexpr int numAccents = 6;
    const juce::Colour accents[numAccents] = {
        juce::Colour (0xffe8582b), juce::Colour (0xff22c1e6), juce::Colour (0xff3ddc84),
        juce::Colour (0xffe84fb4), juce::Colour (0xff8f6bff), juce::Colour (0xfff5c518) };
    const char* accentNames[numAccents] = { "Naranja", "Cian", "Verde", "Magenta", "Violeta", "Amarillo" };

    juce::Colour familyColour (int f)
    {
        static const juce::Colour c[8] = {
            juce::Colour (0xffe8582b), juce::Colour (0xffe9a62b), juce::Colour (0xffe84f8c), juce::Colour (0xff2bb5d4),
            juce::Colour (0xff3ddc9c), juce::Colour (0xff8f6bff), juce::Colour (0xff7ed957), juce::Colour (0xffb8c0cc) };
        return c[juce::jlimit (0, 7, f)];
    }

    void drawPeaks (juce::Graphics& g, const juce::AudioBuffer<float>& data,
                    juce::Rectangle<float> r, juce::Colour c)
    {
        const int n = data.getNumSamples();
        const int w = (int) r.getWidth();
        if (w <= 0 || n <= 0) return;

        const float* d = data.getReadPointer (0);
        const float midY = r.getCentreY();
        const float h = r.getHeight() * 0.5f;

        g.setColour (c);
        for (int x = 0; x < w; ++x)
        {
            const int s0 = (int) ((juce::int64) x * n / w);
            const int s1 = juce::jmax (s0 + 1, (int) ((juce::int64) (x + 1) * n / w));
            const int stride = juce::jmax (1, (s1 - s0) / 16);

            float peak = 0.0f;
            for (int s = s0; s < s1 && s < n; s += stride)
                peak = juce::jmax (peak, std::abs (d[s]));
            peak = juce::jmin (1.0f, peak);

            g.drawVerticalLine ((int) r.getX() + x, midY - peak * h, midY + peak * h);
        }
    }

    void drawPanel (juce::Graphics& g, juce::Rectangle<int> r)
    {
        const auto f = r.toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1f27), f.getX(), f.getY(),
                                                 juce::Colour (0xff14171d), f.getX(), f.getBottom(), false));
        g.fillRoundedRectangle (f, 12.0f);
        g.setColour (lineColour);
        g.drawRoundedRectangle (f.reduced (0.5f), 12.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawHorizontalLine ((int) f.getY() + 1, f.getX() + 12.0f, f.getRight() - 12.0f);
    }

    void drawLabel (juce::Graphics& g, const juce::String& t, int x, int y, int w, juce::Justification j)
    {
        g.setColour (textColour.withAlpha (0.55f));
        g.setFont (11.0f);
        g.drawText (t, x, y, w, 14, j);
    }
}

//==============================================================================
DkcLookAndFeel::DkcLookAndFeel()
{
    setColourScheme (juce::LookAndFeel_V4::getMidnightColourScheme());
    setColour (juce::ResizableWindow::backgroundColourId, bgBottom);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff252a33));
    setColour (juce::TextButton::textColourOffId, textColour);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff101216));
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1b1e25));
    setColour (juce::ComboBox::outlineColourId, lineColour);
    setColour (juce::ComboBox::textColourId, textColour);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxTextColourId, textColour);
    setColour (juce::Slider::backgroundColourId, lineColour);
    setColour (juce::Label::textColourId, textColour);
    setAccent (accents[0]);
}

void DkcLookAndFeel::setAccent (juce::Colour c)
{
    accent = c;
    setColour (juce::TextButton::buttonOnColourId, c);
    setColour (juce::Slider::rotarySliderFillColourId, c);
    setColour (juce::Slider::thumbColourId, c);
    setColour (juce::Slider::trackColourId, c);
}

void DkcLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                       float pos, float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (8.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float arcR = radius - 2.0f;

    const juce::PathStrokeType stroke (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (lineColour);
    g.strokePath (track, stroke);

    juce::Path value;
    value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (accent.withAlpha (0.25f));                                   // brillo
    g.strokePath (value, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (value, stroke);

    const float bodyR = radius - 9.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff343a46), c.x, c.y - bodyR,
                                             juce::Colour (0xff15181e), c.x, c.y + bodyR, false));
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    juce::Path pointer;
    pointer.startNewSubPath (0.0f, -bodyR + 3.0f);
    pointer.lineTo (0.0f, -bodyR * 0.45f);
    g.setColour (juce::Colours::white);
    g.strokePath (pointer, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void DkcLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                       float sliderPos, float minPos, float maxPos,
                                       juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
        return;
    }

    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (lineColour);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, (float) w, 4.0f, 2.0f);
    g.setColour (accent);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, sliderPos - (float) x, 4.0f, 2.0f);
    g.setColour (juce::Colour (0xffeef0f4));
    g.fillEllipse (sliderPos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
    g.setColour (accent);
    g.drawEllipse (sliderPos - 7.0f, cy - 7.0f, 14.0f, 14.0f, 2.0f);
}

//==============================================================================
PadComponent::PadComponent (DrumkitEditor& e, DrumkitProcessor& p) : editor (e), proc (p)
{
    startTimerHz (30);
}

void PadComponent::timerCallback()
{
    const float before = lit;
    const float f = proc.consumePadFlash (padIndex);
    if (f > 0.0f) lit = 1.0f; else lit *= 0.80f;
    if (lit < 0.02f) lit = 0.0f;
    if (lit > 0.0f || before > 0.0f) repaint();
}

void PadComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (3.0f);
    const auto* data = proc.getSampleData (padIndex);
    const bool loaded = data != nullptr;
    const bool selected = editor.getSelectedPad() == padIndex;

    const int fam = proc.getPadFamily (padIndex);
    const auto base = loaded ? (fam >= 0 ? familyColour (fam) : editor.getAccent())
                             : juce::Colour (0xff4a505c);

    g.setGradientFill (juce::ColourGradient (base.withAlpha (loaded ? 0.42f : 0.10f), b.getX(), b.getY(),
                                             base.darker (0.7f).withAlpha (loaded ? 0.12f : 0.04f), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle (b, 10.0f);

    if (loaded)
        drawPeaks (g, *data, b.withTrimmedTop (26.0f).withTrimmedBottom (28.0f).reduced (10.0f, 0.0f),
                   base.brighter (0.4f).withAlpha (0.9f));

    if (heldNote >= 0 || lit > 0.0f)       // se enciende al sonar (tambien desde el patron)
    {
        const float a = juce::jmax (heldNote >= 0 ? 0.35f : 0.0f, 0.5f * lit);
        g.setColour (base.withAlpha (a));
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (base.withAlpha (0.6f * lit));
        g.drawRoundedRectangle (b.expanded (1.5f), 11.0f, 2.0f);
    }

    g.setColour (dragOver ? juce::Colours::white : (selected ? base : lineColour));
    g.drawRoundedRectangle (b, 10.0f, (selected || dragOver) ? 2.5f : 1.0f);

    g.setColour (textColour.withAlpha (0.5f));
    g.setFont (11.0f);
    g.drawText (juce::MidiMessage::getMidiNoteName (dkc::firstMidiNote + padIndex, true, true, 3),
                b.reduced (10.0f, 8.0f).toNearestInt(), juce::Justification::topRight);

    g.setColour (textColour.withAlpha (loaded ? 0.95f : 0.30f));
    if (loaded)
    {
        g.setFont (13.0f);
        g.drawFittedText (proc.getPadName (padIndex), b.reduced (10.0f, 6.0f).toNearestInt().removeFromBottom (22),
                          juce::Justification::centredLeft, 1);
    }
    else
    {
        g.setFont (30.0f);
        g.drawText ("+", b.toNearestInt(), juce::Justification::centred);
    }
}

void PadComponent::mouseDown (const juce::MouseEvent&)
{
    editor.selectPad (padIndex);
    heldNote = dkc::firstMidiNote + padIndex;
    proc.keyboardState.noteOn (1, heldNote, 0.8f);
    repaint();
}

void PadComponent::mouseUp (const juce::MouseEvent&)
{
    if (heldNote >= 0)
        proc.keyboardState.noteOff (1, heldNote, 0.0f);
    heldNote = -1;
    repaint();
}

void PadComponent::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    for (int i = 0; i < files.size() && padIndex + i < dkc::numPads; ++i)
        proc.loadSample (padIndex + i, juce::File (files[i]));

    editor.selectPad (padIndex);
    repaint();
}

//==============================================================================
SampleEditor::SampleEditor (DrumkitEditor& e, DrumkitProcessor& p) : editor (e), proc (p)
{
    startTimerHz (15);
}

float SampleEditor::val (const char* id) const
{
    if (auto* v = proc.apvts.getRawParameterValue (dkc::pid (editor.getSelectedPad(), id)))
        return v->load();
    return 0.0f;
}

juce::Rectangle<float> SampleEditor::getWaveRect() const
{
    return getLocalBounds().toFloat().withTrimmedTop (24.0f).reduced (10.0f, 8.0f);
}

void SampleEditor::timerCallback()
{
    const int pad = editor.getSelectedPad();
    const double sig = val ("start") * 7.0 + val ("end") * 13.0 + val ("attack") * 0.31 + val ("fade") * 0.113
                     + val ("comp") * 17.0 + val ("gain") * 0.71 + pad * 1000.0
                     + (proc.getSampleData (pad) != nullptr ? 5000.0 : 0.0);
    if (std::abs (sig - lastSig) > 1.0e-6)
    {
        lastSig = sig;
        repaint();
    }
}

int SampleEditor::hitHandle (float x) const
{
    const auto wave = getWaveRect();
    const float xs = wave.getX() + val ("start") * wave.getWidth();
    const float xe = wave.getX() + val ("end") * wave.getWidth();
    const float ds = std::abs (x - xs), de = std::abs (x - xe);
    if (juce::jmin (ds, de) > 9.0f) return 0;
    return ds <= de ? 1 : 2;
}

void SampleEditor::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (hitHandle ((float) e.x) != 0 ? juce::MouseCursor::LeftRightResizeCursor
                                                 : juce::MouseCursor::NormalCursor);
}

void SampleEditor::mouseDown (const juce::MouseEvent& e)
{
    if (proc.getSampleData (editor.getSelectedPad()) == nullptr) return;

    dragging = hitHandle ((float) e.x);
    if (dragging == 0) return;

    dragParam = proc.apvts.getParameter (dkc::pid (editor.getSelectedPad(), dragging == 1 ? "start" : "end"));
    if (dragParam != nullptr) dragParam->beginChangeGesture();
}

void SampleEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == 0 || dragParam == nullptr) return;

    const auto wave = getWaveRect();
    const float frac = juce::jlimit (0.0f, 1.0f, ((float) e.x - wave.getX()) / wave.getWidth());
    const float v = dragging == 1 ? juce::jmin (frac, val ("end") - 0.005f)
                                  : juce::jmax (frac, val ("start") + 0.005f);
    dragParam->setValueNotifyingHost (dragParam->convertTo0to1 (juce::jlimit (0.0f, 1.0f, v)));
    repaint();
}

void SampleEditor::mouseUp (const juce::MouseEvent&)
{
    if (dragParam != nullptr) dragParam->endChangeGesture();
    dragParam = nullptr;
    dragging = 0;
}

void SampleEditor::paint (juce::Graphics& g)
{
    const auto accent = editor.getAccent();
    const int pad = editor.getSelectedPad();
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (panelDark);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (lineColour);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    const auto* data = proc.getSampleData (pad);
    if (data == nullptr)
    {
        g.setColour (textColour.withAlpha (0.35f));
        g.setFont (13.0f);
        g.drawText ("Arrastra un sample sobre un pad para editarlo aqui", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const double sr = proc.getSampleRateOf (pad);
    const int n = data->getNumSamples();
    const auto wave = getWaveRect();
    const float midY = wave.getCentreY();
    const float hh = wave.getHeight() * 0.5f;
    const int x0 = (int) wave.getX(), w = (int) wave.getWidth();

    const float s = val ("start"), e = val ("end"), attack = val ("attack"), fade = val ("fade");
    const float compAmt = val ("comp");
    const float gain = juce::Decibels::decibelsToGain (val ("gain"), -60.0f);
    const auto cp = dkc::makeComp (compAmt);
    const int startIdx = (int) (s * (float) n);
    const int endIdx = juce::jmax (startIdx + 1, (int) (e * (float) n));

    g.setColour (lineColour);
    g.drawHorizontalLine ((int) midY, wave.getX(), wave.getRight());

    const float* d = data->getReadPointer (0);
    for (int x = 0; x < w; ++x)
    {
        const int s0 = (int) ((juce::int64) x * n / w);
        const int s1 = juce::jmax (s0 + 1, (int) ((juce::int64) (x + 1) * n / w));
        const int stride = juce::jmax (1, (s1 - s0) / 16);
        const float frac = ((float) x + 0.5f) / (float) w;
        const bool inside = frac >= s && frac <= e;

        float pkOrig = 0.0f, pkProc = 0.0f;
        for (int i = s0; i < s1 && i < n; i += stride)
        {
            const float a = std::abs (d[i]);
            pkOrig = juce::jmax (pkOrig, a);

            if (inside)   // asi suena despues de cortar, comprimir, fades y ganancia
            {
                float env = 1.0f;
                const double t = (double) (i - startIdx) / sr;
                if (attack > 0.0f) env = juce::jmin (1.0f, (float) (t * 1000.0 / (double) attack));
                const double remaining = (double) (endIdx - i) / sr * 1000.0;
                if (fade > 0.0f && remaining < (double) fade) env *= (float) (remaining / (double) fade);
                pkProc = juce::jmax (pkProc, std::abs (dkc::compShape (a, cp)) * env * gain);
            }
        }

        pkOrig = juce::jmin (1.0f, pkOrig);
        g.setColour (juce::Colour (0xff5a6070).withAlpha (inside ? 0.55f : 0.30f));
        g.drawVerticalLine (x0 + x, midY - pkOrig * hh, midY + pkOrig * hh);

        if (inside)
        {
            pkProc = juce::jmin (1.0f, pkProc);
            g.setColour (accent);
            g.drawVerticalLine (x0 + x, midY - pkProc * hh, midY + pkProc * hh);
        }
    }

    const float xs = wave.getX() + s * wave.getWidth();
    const float xe = wave.getX() + e * wave.getWidth();

    // Lo que queda fuera del corte se oscurece
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (wave.getX(), wave.getY(), xs - wave.getX(), wave.getHeight());
    g.fillRect (xe, wave.getY(), wave.getRight() - xe, wave.getHeight());

    // Umbral del compresor
    if (compAmt > 0.001f)
    {
        const float dash[] = { 4.0f, 4.0f };
        g.setColour (accent.withAlpha (0.6f));
        for (float sign : { -1.0f, 1.0f })
        {
            const float yy = midY + sign * cp.thr * hh;
            g.drawDashedLine (juce::Line<float> (xs, yy, xe, yy), dash, 2, 1.0f);
        }
        g.setFont (10.0f);
        g.drawText ("THR " + juce::String (juce::Decibels::gainToDecibels (cp.thr), 0) + " dB",
                    (int) xs + 6, (int) (midY - cp.thr * hh) - 12, 70, 12, juce::Justification::left);
    }

    // Curvas de fade
    const float selW = xe - xs;
    const float aW = juce::jmin (selW, (float) ((double) attack / 1000.0 * sr / (double) n) * wave.getWidth());
    const float fW = juce::jmin (selW, (float) ((double) fade / 1000.0 * sr / (double) n) * wave.getWidth());
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    if (aW > 2.0f)
    {
        g.drawLine (xs, midY + hh * 0.9f, xs + aW, midY - hh * 0.9f, 1.2f);
        g.drawLine (xs, midY - hh * 0.9f, xs + aW, midY + hh * 0.9f, 1.2f);
    }
    if (fW > 2.0f)
    {
        g.drawLine (xe - fW, midY - hh * 0.9f, xe, midY + hh * 0.9f, 1.2f);
        g.drawLine (xe - fW, midY + hh * 0.9f, xe, midY - hh * 0.9f, 1.2f);
    }

    // Asas de corte START / END
    for (float hx : { xs, xe })
    {
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillRect (hx - 1.0f, wave.getY(), 2.0f, wave.getHeight());
        g.setColour (accent);
        g.fillRoundedRectangle (hx - 5.0f, wave.getY() - 4.0f, 10.0f, 16.0f, 3.0f);
    }

    auto ms = [&] (float frac) { return juce::String ((int) std::lround ((double) frac * (double) n / sr * 1000.0)) + " ms"; };

    g.setColour (textColour.withAlpha (0.75f));
    g.setFont (11.0f);
    g.drawText ("SAMPLE  " + juce::String ((double) n / sr, 2) + " s", 12, 5, 140, 16, juce::Justification::left);
    g.drawText ("corte  " + ms (s) + "  >  " + ms (e) + "   (" + ms (e - s) + ")",
                getWidth() - 12 - 230, 5, 230, 16, juce::Justification::right);
}

//==============================================================================
PatternGrid::PatternGrid (DrumkitEditor& e, DrumkitProcessor& p) : editor (e), proc (p)
{
    startTimerHz (30);
}

void PatternGrid::timerCallback()
{
    const int cs = proc.getCurrentStep();
    if (cs != shownStep)
    {
        shownStep = cs;
        if (isVisible()) repaint();
    }
}

bool PatternGrid::cellAt (juce::Point<int> pos, int& pad, int& step) const
{
    const float rowH = (float) getHeight() / 16.0f;
    const float cw = (float) (getWidth() - gutter) / (float) dkc::numSteps;
    if (pos.x < gutter || pos.y < 0 || pos.y >= getHeight()) return false;

    const int row = juce::jlimit (0, 15, (int) ((float) pos.y / rowH));
    step = (int) ((float) (pos.x - gutter) / cw);
    pad = bank * dkc::padsPerBank + (15 - row);
    return step >= 0 && step < dkc::numSteps;
}

void PatternGrid::applyAt (juce::Point<int> pos)
{
    int pad = 0, step = 0;
    if (! cellAt (pos, pad, step)) return;
    if (pad == lastPad && step == lastStep) return;
    lastPad = pad;
    lastStep = step;

    if (proc.getStepVel (pad, step) != paintValue)
    {
        proc.setStepVel (pad, step, paintValue);
        if (paintValue == 0) proc.setStepRatchet (pad, step, 1);
        if (paintValue > 0)   // sonido de prueba al escribir una nota
        {
            proc.keyboardState.noteOn (1, dkc::firstMidiNote + pad, 0.7f);
            proc.keyboardState.noteOff (1, dkc::firstMidiNote + pad, 0.0f);
        }
        repaint();
    }
}

void PatternGrid::mouseDown (const juce::MouseEvent& e)
{
    int pad = 0, step = 0;
    if (! cellAt (e.getPosition(), pad, step)) return;

    const int v = proc.getStepVel (pad, step);

    if (e.mods.isRightButtonDown())     // click derecho: cambia la fuerza 127 > 100 > 64 > 32
    {
        if (v > 0)
        {
            proc.setStepVel (pad, step, v >= 120 ? 100 : (v >= 90 ? 64 : (v >= 50 ? 32 : 127)));
            repaint();
        }
        return;
    }

    if (e.mods.isShiftDown())           // shift + click: roll (1 > 2 > 3 > 4 golpes en el paso)
    {
        if (v > 0)
        {
            proc.setStepRatchet (pad, step, proc.getStepRatchet (pad, step) % 4 + 1);
            repaint();
        }
        return;
    }

    paintValue = v > 0 ? 0 : 100;       // click: pone o quita; arrastrar pinta
    lastPad = lastStep = -1;
    applyAt (e.getPosition());
}

void PatternGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (! e.mods.isRightButtonDown() && ! e.mods.isShiftDown())
        applyAt (e.getPosition());
}

void PatternGrid::paint (juce::Graphics& g)
{
    g.setColour (panelDark);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 12.0f);
    g.setColour (lineColour);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 12.0f, 1.0f);

    const float rowH = (float) getHeight() / 16.0f;
    const float cw = (float) (getWidth() - gutter) / (float) dkc::numSteps;
    const auto accent = editor.getAccent();

    for (int row = 0; row < 16; ++row)
    {
        const int pad = bank * dkc::padsPerBank + (15 - row);
        const float y = (float) row * rowH;
        const int fam = proc.getPadFamily (pad);
        const auto rowColour = fam >= 0 ? familyColour (fam) : accent;
        const auto name = proc.getPadName (pad);
        const bool loaded = proc.getSampleData (pad) != nullptr;

        // Nombre del instrumento + nota (como las teclas de un piano roll)
        g.setColour (rowColour.withAlpha (loaded ? 0.9f : 0.25f));
        g.fillRoundedRectangle (6.0f, y + 3.0f, 3.0f, rowH - 6.0f, 1.5f);
        g.setColour (textColour.withAlpha (loaded ? 0.95f : 0.35f));
        g.setFont (11.0f);
        g.drawFittedText (name.isNotEmpty() ? name : juce::String ("-"),
                          juce::Rectangle<int> (14, (int) y, gutter - 18, (int) rowH).withTrimmedBottom (9),
                          juce::Justification::centredLeft, 1);
        g.setColour (textColour.withAlpha (0.4f));
        g.setFont (9.0f);
        g.drawText (juce::MidiMessage::getMidiNoteName (dkc::firstMidiNote + pad, true, true, 3),
                    14, (int) (y + rowH * 0.5f), gutter - 18, (int) (rowH * 0.5f), juce::Justification::centredLeft);

        for (int s = 0; s < dkc::numSteps; ++s)
        {
            const juce::Rectangle<float> cell ((float) gutter + (float) s * cw + 1.0f, y + 1.0f, cw - 2.0f, rowH - 2.0f);
            const int vel = proc.getStepVel (pad, s);

            if (vel > 0)
                g.setColour (rowColour.withAlpha (0.30f + 0.70f * (float) vel / 127.0f));
            else
                g.setColour (((s / 4) % 2 == 0) ? juce::Colour (0xff1d2128) : juce::Colour (0xff171a20));

            g.fillRoundedRectangle (cell, 3.0f);

            if (vel > 0)   // rolls: marcas dentro de la celda
            {
                const int r = proc.getStepRatchet (pad, s);
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                for (int k = 1; k < r; ++k)
                    g.drawVerticalLine ((int) (cell.getX() + cell.getWidth() * (float) k / (float) r),
                                        cell.getY() + 2.0f, cell.getBottom() - 2.0f);
            }
        }
    }

    const int cs = proc.getCurrentStep();
    if (cs >= 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.13f));
        g.fillRect ((float) gutter + (float) cs * cw, 0.0f, cw, (float) getHeight());
    }

    g.setColour (lineColour.brighter (0.3f));
    g.drawVerticalLine (gutter + (int) (16.0f * cw), 0.0f, (float) getHeight());   // separador de compas
}

//==============================================================================
SourcePanel::SourcePanel (DrumkitEditor& e, DrumkitProcessor& p) : editor (e), proc (p)
{
    startTimerHz (20);
}

void SourcePanel::timerCallback()
{
    const bool c = proc.isCloning();
    if (c || wasCloning) repaint();
    wasCloning = c;
}

void SourcePanel::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    if (files.size() > 0)
        proc.cloneFromFile (juce::File (files[0]));
    repaint();
}

void SourcePanel::paint (juce::Graphics& g)
{
    const auto accent = editor.getAccent();
    drawPanel (g, getLocalBounds());
    if (dragOver)
    {
        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.5f), 12.0f, 2.0f);
    }

    const auto analysis = proc.getAnalysis();
    const bool hasData = analysis != nullptr && analysis->numSamples > 0;
    const int w = getWidth();

    g.setColour (accent);
    g.setFont (12.0f);
    g.drawText ("SOURCE", 16, 10, 60, 18, juce::Justification::centredLeft);

    juce::String info;
    if (! proc.getCloneStatus().isEmpty())
        info = proc.getCloneStatus();
    else if (hasData)
    {
        const int secs = (int) ((double) analysis->numSamples / analysis->sampleRate);
        info = analysis->name + "     " + juce::String (secs / 60) + ":" + juce::String (secs % 60).paddedLeft ('0', 2)
               + "     " + juce::String ((int) analysis->hits.size()) + " hits";
    }
    else
        info = "Arrastra aqui una cancion, un loop o un one-shot";

    g.setColour (textColour);
    g.setFont (13.0f);
    g.drawText (info, 76, 10, w - 76 - 470, 18, juce::Justification::centredLeft);

    drawLabel (g, "SENSITIVITY", w - 16 - 84 - 8 - 96 - 8 - 170 - 88, 14, 80, juce::Justification::centredRight);

    const juce::Rectangle<int> wr (16, 40, w - 32, 62);
    g.setColour (panelDark);
    g.fillRoundedRectangle (wr.toFloat(), 8.0f);

    if (hasData)
    {
        const float midY = (float) wr.getCentreY() + 6.0f;
        const float hh = (float) wr.getHeight() * 0.38f;
        g.setColour (juce::Colour (0xff4a5060));
        for (int x = 0; x < wr.getWidth(); ++x)
        {
            const size_t idx = (size_t) ((juce::int64) x * (juce::int64) analysis->peaks.size() / wr.getWidth());
            const float pk = analysis->peaks[juce::jmin (idx, analysis->peaks.size() - 1)];
            g.drawVerticalLine (wr.getX() + x, midY - pk * hh, midY + pk * hh);
        }

        for (const auto& h : analysis->hits)
        {
            const int x = wr.getX() + (int) ((juce::int64) h.start * wr.getWidth() / analysis->numSamples);
            g.setColour (familyColour (h.family).withAlpha (0.25f));
            g.drawVerticalLine (x, (float) wr.getY() + 4.0f, (float) wr.getBottom() - 4.0f);
            g.setColour (familyColour (h.family));
            g.drawVerticalLine (x, (float) wr.getY() + 4.0f, (float) wr.getY() + 16.0f);
        }

        int lx = 16;
        g.setFont (11.0f);
        for (int f = 0; f < dkc::numFamilies; ++f)
        {
            g.setColour (familyColour (f));
            g.fillEllipse ((float) lx, (float) wr.getBottom() + 8.0f, 8.0f, 8.0f);
            g.setColour (textColour.withAlpha (0.7f));
            g.drawText (juce::String (dkc::familyLabel (f)) + " " + juce::String (analysis->familyCount[f]),
                        lx + 12, wr.getBottom() + 4, 100, 16, juce::Justification::centredLeft);
            lx += 118;
        }
    }
    else
    {
        g.setColour (textColour.withAlpha (0.3f));
        g.setFont (13.0f);
        g.drawText ("Suelta un archivo de audio aqui (wav, mp3, flac, aiff...) y el plugin crea el kit", wr, juce::Justification::centred);
    }

    if (proc.isCloning())
    {
        const float pr = juce::jlimit (0.0f, 1.0f, proc.getCloneProgress());
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (wr.toFloat(), 8.0f);
        g.setColour (accent);
        g.fillRoundedRectangle ((float) wr.getX() + 20.0f, (float) wr.getCentreY() + 6.0f,
                                (float) (wr.getWidth() - 40) * pr, 6.0f, 3.0f);
        g.setColour (textColour);
        g.setFont (13.0f);
        g.drawText ("ANALIZANDO...  " + juce::String ((int) (pr * 100.0f)) + "%",
                    wr.withTrimmedBottom (20), juce::Justification::centred);
    }
}

//==============================================================================
void MidiDragSource::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff252a33));
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (lineColour);
    g.drawRoundedRectangle (b, 8.0f, 1.0f);
    g.setColour (textColour);
    g.setFont (12.0f);
    g.drawText ("ARRASTRA MIDI AL DAW", getLocalBounds(), juce::Justification::centred);
}

void MidiDragSource::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 8) return;

    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("Drumkit Cloner Pattern.mid");

    if (proc.writeMidiFile (tmp))
    {
        dragging = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ tmp.getFullPathName() }, false, this,
                                                                    [this] { dragging = false; });
    }
}

//==============================================================================
DrumkitEditor::DrumkitEditor (DrumkitProcessor& p)
    : AudioProcessorEditor (p), proc (p), sourcePanel (*this, p), sampleEditor (*this, p),
      patternGrid (*this, p), midiDrag (p)
{
    setLookAndFeel (&laf);

    for (int i = 0; i < dkc::padsPerBank; ++i)
        addAndMakeVisible (pads.add (new PadComponent (*this, proc)));
    addAndMakeVisible (sourcePanel);
    addAndMakeVisible (sampleEditor);
    addChildComponent (patternGrid);
    addAndMakeVisible (midiDrag);

    static const char* bankNames[] = { "A", "B", "C", "D" };
    for (int b = 0; b < dkc::numBanks; ++b)
    {
        bankButtons[b].setButtonText (bankNames[b]);
        bankButtons[b].setClickingTogglesState (true);
        bankButtons[b].setRadioGroupId (1001);
        bankButtons[b].onClick = [this, b] { setBank (b); };
        addAndMakeVisible (bankButtons[b]);
    }
    bankButtons[0].setToggleState (true, juce::dontSendNotification);

    static const char* names[]  = { "CLEAN", "GAIN", "PAN", "TUNE", "COMP", "ATTACK", "FADE", "HP", "LP" };
    static const char* suffix[] = { "", " dB", "", " st", "", " ms", " ms", " Hz", " Hz" };
    for (int i = 0; i < numKnobs; ++i)
    {
        knobs[i].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knobs[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
        knobs[i].setName (names[i]);
        knobs[i].setTextValueSuffix (suffix[i]);
        knobs[i].setNumDecimalPlacesToDisplay (i >= 7 ? 0 : (i == 3 || i == 4 || i == 0 ? 2 : 1));
        addAndMakeVisible (knobs[i]);
    }

    chokeBox.addItemList (dkc::chokeChoices(), 1);
    outBox.addItemList (dkc::outputChoices(), 1);
    for (int i = 0; i < numAccents; ++i)
        accentBox.addItem (accentNames[i], i + 1);
    accentBox.setSelectedId (juce::jlimit (0, numAccents - 1, proc.accentIndex.load()) + 1, juce::dontSendNotification);
    accentBox.onChange = [this] { applyAccent (accentBox.getSelectedId() - 1); };

    for (int n = 24; n <= 83; ++n)
        noteBox.addItem (juce::MidiMessage::getMidiNoteName (n, true, true, 3), n + 1);
    noteBox.setSelectedId (41 + 1, juce::dontSendNotification);   // F1

    for (int i = 0; i < dkc::numStyles; ++i)
        styleBox.addItem (dkc::styleName (i), i + 1);
    styleBox.setSelectedId (1, juce::dontSendNotification);

    muteButton.setClickingTogglesState (true);
    soloButton.setClickingTogglesState (true);

    for (auto* c : { (juce::Component*) &chokeBox, (juce::Component*) &outBox, (juce::Component*) &accentBox,
                     (juce::Component*) &noteBox, (juce::Component*) &styleBox,
                     (juce::Component*) &muteButton, (juce::Component*) &soloButton,
                     (juce::Component*) &detectLabel, (juce::Component*) &detectButton, (juce::Component*) &tuneButton,
                     (juce::Component*) &snapButton, (juce::Component*) &tuneAllButton, (juce::Component*) &editButton,
                     (juce::Component*) &playButton, (juce::Component*) &clearButton, (juce::Component*) &saveMidiButton,
                     (juce::Component*) &saveWavButton, (juce::Component*) &generateButton, (juce::Component*) &hatsButton,
                     (juce::Component*) &bpmSlider, (juce::Component*) &sensSlider, (juce::Component*) &intensitySlider,
                     (juce::Component*) &humTimeSlider, (juce::Component*) &humVelSlider, (juce::Component*) &swingSlider,
                     (juce::Component*) &recloneButton, (juce::Component*) &browseButton })
        addAndMakeVisible (c);

    // --- Corrector de pitch ---
    detectLabel.setJustificationType (juce::Justification::centredLeft);
    setPitchMessage ("Pitch: pulsa DETECT");

    detectButton.onClick = [this]
    {
        const auto r = proc.analysePitch (selectedPad);
        if (! r.valid) { setPitchMessage ("Sin tono claro (sample sin pitch)"); return; }
        const int nearest = (int) std::lround (r.midiNote);
        const int cents = (int) std::lround ((r.midiNote - (float) nearest) * 100.0f);
        setPitchMessage ("Detectado: " + juce::MidiMessage::getMidiNoteName (nearest, true, true, 3)
                         + "  " + (cents >= 0 ? "+" : "") + juce::String (cents) + " cents");
    };

    tuneButton.onClick = [this]
    {
        const auto r = proc.tunePad (selectedPad, (float) (noteBox.getSelectedId() - 1), false);
        setPitchMessage (r.valid ? "Afinado a " + noteBox.getText() : "Sin tono claro (sample sin pitch)");
    };

    snapButton.onClick = [this]
    {
        const auto r = proc.tunePad (selectedPad, 0.0f, true);
        setPitchMessage (r.valid ? "Ajustado a la nota mas cercana" : "Sin tono claro (sample sin pitch)");
    };

    tuneAllButton.onClick = [this]
    {
        int ok = 0, total = 0;
        for (int i = 0; i < dkc::numPads; ++i)
        {
            if (proc.getSampleData (i) == nullptr) continue;
            ++total;
            if (proc.tunePad (i, (float) (noteBox.getSelectedId() - 1), false).valid) ++ok;
        }
        setPitchMessage (juce::String (ok) + " de " + juce::String (total) + " pads afinados a " + noteBox.getText());
    };

    // --- Patron: transporte, generador, humanizacion y exportacion ---
    editButton.setClickingTogglesState (true);
    editButton.onClick = [this] { showEditMode (editButton.getToggleState()); };

    playButton.setClickingTogglesState (true);
    playButton.onClick = [this]
    {
        const bool on = playButton.getToggleState();
        playButton.setButtonText (on ? "STOP" : "PLAY");
        proc.setSequencerPlaying (on);
    };

    clearButton.onClick = [this] { proc.clearPattern(); patternGrid.repaint(); };
    generateButton.onClick = [this] { generate (false); };
    hatsButton.onClick = [this] { generate (true); };

    saveMidiButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Guardar patron MIDI",
            juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Drumkit Cloner pattern.mid"),
            "*.mid");

        auto& pr = proc;
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [&pr] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != juce::File())
                                      pr.writeMidiFile (f.withFileExtension ("mid"));
                              });
    };

    saveWavButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Exportar el loop a WAV",
            juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Drumkit Cloner loop.wav"),
            "*.wav");

        auto& pr = proc;
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [&pr] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != juce::File())
                                      pr.renderPatternToWav (f.withFileExtension ("wav"));
                              });
    };

    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 50, 22);
    bpmSlider.setNumDecimalPlacesToDisplay (0);
    bpmAtt = std::make_unique<SliderAtt> (proc.apvts, "bpm", bpmSlider);

    for (auto* s : { &intensitySlider, &humTimeSlider, &humVelSlider, &swingSlider })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    }
    intensitySlider.setRange (0.0, 1.0, 0.01);
    intensitySlider.setValue (0.5, juce::dontSendNotification);
    humTimeAtt = std::make_unique<SliderAtt> (proc.apvts, "hum_time", humTimeSlider);
    humVelAtt = std::make_unique<SliderAtt> (proc.apvts, "hum_vel", humVelSlider);
    swingAtt = std::make_unique<SliderAtt> (proc.apvts, "swing", swingSlider);

    // --- Clonar desde una cancion ---
    sensSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    sensSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    sensSlider.setRange (0.0, 1.0, 0.01);
    sensSlider.setValue (proc.sensitivity.load(), juce::dontSendNotification);
    sensSlider.onValueChange = [this] { proc.sensitivity.store ((float) sensSlider.getValue()); };

    recloneButton.onClick = [this] { proc.recloneSource(); };

    browseButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Elige una cancion, un loop o un sample",
            juce::File::getSpecialLocation (juce::File::userMusicDirectory),
            "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.aac;*.ogg;*.caf");

        auto& pr = proc;
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [&pr] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile())
                                      pr.cloneFromFile (f);
                              });
    };

    setSize (1180, 920);
    proc.addChangeListener (this);
    applyAccent (juce::jlimit (0, numAccents - 1, proc.accentIndex.load()));
    setBank (0);
}

DrumkitEditor::~DrumkitEditor()
{
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void DrumkitEditor::generate (bool hatsOnly)
{
    proc.generatePattern (styleBox.getSelectedId() - 1, (float) intensitySlider.getValue(), hatsOnly);
    editButton.setToggleState (true, juce::dontSendNotification);
    showEditMode (true);
    patternGrid.repaint();
}

void DrumkitEditor::applyAccent (int idx)
{
    idx = juce::jlimit (0, numAccents - 1, idx);
    proc.accentIndex.store (idx);
    laf.setAccent (accents[idx]);
    sendLookAndFeelChange();
    repaint();
}

void DrumkitEditor::showEditMode (bool on)
{
    patternGrid.setVisible (on);
    for (auto* p : pads) p->setVisible (! on);
}

void DrumkitEditor::setPitchMessage (const juce::String& text)
{
    detectLabel.setText (text, juce::dontSendNotification);
}

void DrumkitEditor::setBank (int bank)
{
    for (int i = 0; i < dkc::padsPerBank; ++i)
        pads[i]->setPad (bank * dkc::padsPerBank + i);

    patternGrid.setBank (bank);
    selectPad (bank * dkc::padsPerBank + (selectedPad % dkc::padsPerBank));
}

void DrumkitEditor::selectPad (int pad)
{
    selectedPad = juce::jlimit (0, dkc::numPads - 1, pad);
    rebuildAttachments();
    setPitchMessage ("Pitch: pulsa DETECT");
    repaint();
}

void DrumkitEditor::rebuildAttachments()
{
    static const char* ids[] = { "clean", "gain", "pan", "tune", "comp", "attack", "fade", "hp", "lp" };

    for (int i = 0; i < numKnobs; ++i)
    {
        knobAtts[i].reset();
        knobAtts[i] = std::make_unique<SliderAtt> (proc.apvts, dkc::pid (selectedPad, ids[i]), knobs[i]);
    }

    chokeAtt.reset(); outAtt.reset(); muteAtt.reset(); soloAtt.reset();
    chokeAtt = std::make_unique<ComboAtt>  (proc.apvts, dkc::pid (selectedPad, "choke"), chokeBox);
    outAtt   = std::make_unique<ComboAtt>  (proc.apvts, dkc::pid (selectedPad, "out"),   outBox);
    muteAtt  = std::make_unique<ButtonAtt> (proc.apvts, dkc::pid (selectedPad, "mute"),  muteButton);
    soloAtt  = std::make_unique<ButtonAtt> (proc.apvts, dkc::pid (selectedPad, "solo"),  soloButton);
}

//==============================================================================
void DrumkitEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f, bgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    const auto accent = laf.getAccent();

    // Barra superior con un hilo de color
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.0f), 0.0f, 0.0f, accent, (float) getWidth() * 0.5f, 0.0f, false));
    g.fillRect (0, 0, getWidth() / 2, 2);
    g.setGradientFill (juce::ColourGradient (accent, (float) getWidth() * 0.5f, 0.0f, accent.withAlpha (0.0f), (float) getWidth(), 0.0f, false));
    g.fillRect (getWidth() / 2, 0, getWidth() - getWidth() / 2, 2);

    g.setGradientFill (juce::ColourGradient (accent.brighter (0.2f), 16.0f, 18.0f, accent.darker (0.5f), 34.0f, 36.0f, false));
    g.fillRoundedRectangle (16.0f, 16.0f, 22.0f, 22.0f, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    for (int i = 0; i < 4; ++i)                               // icono: barras de una onda
    {
        const float hgt = i == 1 ? 12.0f : (i == 2 ? 8.0f : 5.0f);
        g.fillRoundedRectangle (21.0f + (float) i * 4.0f, 27.0f - hgt * 0.5f, 2.0f, hgt, 1.0f);
    }

    g.setColour (textColour);
    g.setFont (17.0f);
    g.drawText ("DRUMKIT CLONER", 48, 14, 300, 26, juce::Justification::centredLeft);
    g.setColour (textColour.withAlpha (0.4f));
    g.setFont (11.0f);
    g.drawText ("v0.4", 190, 20, 60, 16, juce::Justification::centredLeft);
    drawLabel (g, "COLOR", accentBox.getX() - 52, accentBox.getY() + 11, 48, juce::Justification::centredRight);

    drawPanel (g, rightPanel);
    drawPanel (g, bottomPanel);

    const auto name = proc.getPadName (selectedPad);
    g.setColour (accent);
    g.setFont (14.0f);
    g.drawText ("PAD " + juce::String (selectedPad + 1) + (name.isNotEmpty() ? "  ·  " + name : juce::String()),
                waveArea.getX(), waveArea.getY() - 26, waveArea.getWidth(), 22, juce::Justification::centredLeft);

    for (int i = 0; i < numKnobs; ++i)
        drawLabel (g, knobs[i].getName(), knobs[i].getX(), knobs[i].getY() - 14, knobs[i].getWidth(), juce::Justification::centred);

    drawLabel (g, "CHOKE", chokeBox.getX(), chokeBox.getY() - 15, chokeBox.getWidth(), juce::Justification::left);
    drawLabel (g, "OUT", outBox.getX(), outBox.getY() - 15, outBox.getWidth(), juce::Justification::left);
    drawLabel (g, "CORRECTOR DE PITCH  ·  nota destino", noteBox.getX(), detectLabel.getY() - 16, 320, juce::Justification::left);

    g.setColour (accent);
    g.setFont (11.0f);
    g.drawText ("PATTERN", bottomPanel.getX() + 16, bottomPanel.getY() + 7, 100, 14, juce::Justification::left);
    g.setColour (textColour.withAlpha (0.5f));
    g.setFont (11.0f);
    g.drawText ("click: nota  ·  arrastrar: pintar  ·  shift+click: roll  ·  click derecho: fuerza",
                bottomPanel.getX() + 90, bottomPanel.getY() + 7, 520, 14, juce::Justification::left);
    for (const auto& l : labelList)
        g.drawText (l.first, l.second, juce::Justification::centredLeft);
}

void DrumkitEditor::resized()
{
    auto r = getLocalBounds().reduced (16);

    auto top = r.removeFromTop (40);
    accentBox.setBounds (top.removeFromRight (110).withSizeKeepingCentre (110, 28));

    r.removeFromTop (10);
    sourceBounds = r.removeFromTop (132);
    r.removeFromTop (10);
    bottomPanel = r.removeFromBottom (136);
    r.removeFromBottom (10);

    sourcePanel.setBounds (sourceBounds);
    {
        auto row = sourceBounds.withHeight (34).reduced (16, 3);
        browseButton.setBounds (row.removeFromRight (84));
        row.removeFromRight (8);
        recloneButton.setBounds (row.removeFromRight (96));
        row.removeFromRight (8);
        sensSlider.setBounds (row.removeFromRight (170));
    }

    // Bancos (A abajo, como un Drum Rack)
    auto bankCol = r.removeFromLeft (48);
    const int rowH = r.getHeight() / 4;
    for (int i = 0; i < dkc::numBanks; ++i)
        bankButtons[dkc::numBanks - 1 - i].setBounds (bankCol.removeFromTop (rowH).withSizeKeepingCentre (44, 44));

    r.removeFromLeft (10);
    gridArea = r.removeFromLeft (552);
    const int cellW = gridArea.getWidth() / 4;

    for (int i = 0; i < dkc::padsPerBank; ++i)
    {
        const int col = i % 4, row = i / 4;
        pads[i]->setBounds (gridArea.getX() + col * cellW, gridArea.getY() + (3 - row) * rowH, cellW, rowH);
    }
    patternGrid.setBounds (gridArea);

    r.removeFromLeft (20);
    rightPanel = r;

    auto right = rightPanel.reduced (16, 12);
    right.removeFromTop (26);
    waveArea = right.removeFromTop (140);
    sampleEditor.setBounds (waveArea);
    right.removeFromTop (8);

    const int kw = right.getWidth() / 5;
    for (int rr = 0; rr < 2; ++rr)
    {
        auto row = right.removeFromTop (104);
        const int count = rr == 0 ? 5 : 4;
        for (int c = 0; c < count; ++c)
            knobs[rr * 5 + c].setBounds (row.removeFromLeft (kw).withTrimmedTop (16).reduced (2));
    }

    right.removeFromTop (22);
    auto ctrl = right.removeFromTop (28);
    chokeBox.setBounds (ctrl.removeFromLeft (90));
    ctrl.removeFromLeft (10);
    outBox.setBounds (ctrl.removeFromLeft (150));
    ctrl.removeFromLeft (14);
    muteButton.setBounds (ctrl.removeFromLeft (44));
    ctrl.removeFromLeft (6);
    soloButton.setBounds (ctrl.removeFromLeft (44));

    right.removeFromTop (26);
    detectLabel.setBounds (right.removeFromTop (22));
    right.removeFromTop (6);
    auto prow = right.removeFromTop (30);
    noteBox.setBounds (prow.removeFromLeft (84));
    prow.removeFromLeft (6);
    detectButton.setBounds (prow.removeFromLeft (74));
    prow.removeFromLeft (6);
    tuneButton.setBounds (prow.removeFromLeft (64));
    prow.removeFromLeft (6);
    snapButton.setBounds (prow.removeFromLeft (64));
    prow.removeFromLeft (6);
    tuneAllButton.setBounds (prow.removeFromLeft (84));

    // Panel inferior: dos filas
    labelList.clear();
    auto area = bottomPanel.withTrimmedTop (26);
    auto row1 = area.removeFromTop (34).reduced (16, 0);
    area.removeFromTop (12);
    auto row2 = area.removeFromTop (34).reduced (16, 0);

    auto take = [] (juce::Rectangle<int>& row, int w, int gap = 8)
    {
        auto piece = row.removeFromLeft (w);
        row.removeFromLeft (gap);
        return piece;
    };

    editButton.setBounds (take (row1, 76));
    playButton.setBounds (take (row1, 76, 22));
    labelList.push_back ({ "BPM", take (row1, 34, 0) });
    bpmSlider.setBounds (take (row1, 210, 30));
    styleBox.setBounds (take (row1, 120));
    generateButton.setBounds (take (row1, 100));
    hatsButton.setBounds (take (row1, 92, 26));
    labelList.push_back ({ "INTENSITY", take (row1, 68, 0) });
    intensitySlider.setBounds (take (row1, 130));

    labelList.push_back ({ "HUMANIZE", take (row2, 76, 6) });
    labelList.push_back ({ "TIMING", take (row2, 44, 0) });
    humTimeSlider.setBounds (take (row2, 100, 14));
    labelList.push_back ({ "VELO", take (row2, 32, 0) });
    humVelSlider.setBounds (take (row2, 100, 14));
    labelList.push_back ({ "SWING", take (row2, 40, 0) });
    swingSlider.setBounds (take (row2, 100, 26));
    clearButton.setBounds (take (row2, 70));
    saveMidiButton.setBounds (take (row2, 100));
    midiDrag.setBounds (take (row2, 176));
    saveWavButton.setBounds (take (row2, 100));
}
