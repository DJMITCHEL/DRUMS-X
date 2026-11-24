#include "PluginEditor.h"
#include <cmath>

namespace
{
    const juce::Colour bgColour    { 0xff101216 };
    const juce::Colour panelColour { 0xff171a20 };
    const juce::Colour panelDark   { 0xff0e1014 };
    const juce::Colour lineColour  { 0xff262a33 };
    const juce::Colour textColour  { 0xffd5d9e0 };

    constexpr int numAccents = 6;
    const juce::Colour accents[numAccents] = {
        juce::Colour (0xffe8582b), juce::Colour (0xff22c1e6), juce::Colour (0xff3ddc84),
        juce::Colour (0xffe84fb4), juce::Colour (0xff8f6bff), juce::Colour (0xfff5c518) };
    const char* accentNames[numAccents] = { "Naranja", "Cian", "Verde", "Magenta", "Violeta", "Amarillo" };

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
        g.setColour (panelColour);
        g.fillRoundedRectangle (r.toFloat(), 12.0f);
        g.setColour (lineColour);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 12.0f, 1.0f);
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
    setColour (juce::ResizableWindow::backgroundColourId, bgColour);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff232730));
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
    g.setColour (accent);
    g.strokePath (value, stroke);

    const float bodyR = radius - 9.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c313c), c.x, c.y - bodyR,
                                             juce::Colour (0xff181b21), c.x, c.y + bodyR, false));
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    juce::Path pointer;
    pointer.startNewSubPath (0.0f, -bodyR + 3.0f);
    pointer.lineTo (0.0f, -bodyR * 0.45f);
    g.setColour (juce::Colours::white);
    g.strokePath (pointer, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

//==============================================================================
void PadComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (3.0f);
    const auto* data = proc.getSampleData (padIndex);
    const bool loaded = data != nullptr;
    const bool selected = editor.getSelectedPad() == padIndex;

    const auto base = loaded ? editor.getAccent().withRotatedHue (((float) (padIndex % 4) - 1.5f) * 0.03f)
                             : juce::Colour (0xff4a505c);

    g.setGradientFill (juce::ColourGradient (base.withAlpha (loaded ? 0.40f : 0.10f), b.getX(), b.getY(),
                                             base.darker (0.6f).withAlpha (loaded ? 0.12f : 0.04f), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle (b, 10.0f);

    if (loaded)
        drawPeaks (g, *data, b.withTrimmedTop (26.0f).withTrimmedBottom (28.0f).reduced (10.0f, 0.0f),
                   base.brighter (0.4f).withAlpha (0.9f));

    if (heldNote >= 0)
    {
        g.setColour (base.withAlpha (0.35f));
        g.fillRoundedRectangle (b, 10.0f);
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

    paintValue = v > 0 ? 0 : 100;       // click: pone o quita; arrastrar pinta
    lastPad = lastStep = -1;
    applyAt (e.getPosition());
}

void PatternGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (! e.mods.isRightButtonDown())
        applyAt (e.getPosition());
}

void PatternGrid::paint (juce::Graphics& g)
{
    g.setColour (panelDark);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 12.0f);

    const float rowH = (float) getHeight() / 16.0f;
    const float cw = (float) (getWidth() - gutter) / (float) dkc::numSteps;
    const auto accent = editor.getAccent();

    for (int row = 0; row < 16; ++row)
    {
        const int pad = bank * dkc::padsPerBank + (15 - row);
        const float y = (float) row * rowH;

        g.setColour (textColour.withAlpha (proc.getSampleData (pad) != nullptr ? 0.8f : 0.35f));
        g.setFont (10.0f);
        g.drawText (juce::MidiMessage::getMidiNoteName (dkc::firstMidiNote + pad, true, true, 3),
                    8, (int) y, gutter - 10, (int) rowH, juce::Justification::centredLeft);

        for (int s = 0; s < dkc::numSteps; ++s)
        {
            const juce::Rectangle<float> cell ((float) gutter + (float) s * cw + 1.0f, y + 1.0f, cw - 2.0f, rowH - 2.0f);
            const int vel = proc.getStepVel (pad, s);

            if (vel > 0)
                g.setColour (accent.withAlpha (0.30f + 0.70f * (float) vel / 127.0f));
            else
                g.setColour (((s / 4) % 2 == 0) ? juce::Colour (0xff1d2128) : juce::Colour (0xff171a20));

            g.fillRoundedRectangle (cell, 3.0f);
        }
    }

    const int cs = proc.getCurrentStep();
    if (cs >= 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.13f));
        g.fillRect ((float) gutter + (float) cs * cw, 0.0f, cw, (float) getHeight());
    }

    g.setColour (lineColour);
    g.drawVerticalLine (gutter + (int) (16.0f * cw), 0.0f, (float) getHeight());   // separador de compas
}

//==============================================================================
void MidiDragSource::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff232730));
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
    : AudioProcessorEditor (p), proc (p), patternGrid (*this, p), midiDrag (p)
{
    setLookAndFeel (&laf);

    for (int i = 0; i < dkc::padsPerBank; ++i)
        addAndMakeVisible (pads.add (new PadComponent (*this, proc)));
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

    static const char* names[]  = { "CLEAN", "GAIN", "PAN", "TUNE", "ATTACK", "FADE", "HP", "LP" };
    static const char* suffix[] = { "", " dB", "", " st", " ms", " ms", " Hz", " Hz" };
    for (int i = 0; i < numKnobs; ++i)
    {
        knobs[i].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knobs[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
        knobs[i].setName (names[i]);
        knobs[i].setTextValueSuffix (suffix[i]);
        knobs[i].setNumDecimalPlacesToDisplay (i >= 6 ? 0 : (i == 3 ? 2 : 1));
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

    muteButton.setClickingTogglesState (true);
    soloButton.setClickingTogglesState (true);

    for (auto* c : { (juce::Component*) &chokeBox, (juce::Component*) &outBox, (juce::Component*) &accentBox,
                     (juce::Component*) &noteBox, (juce::Component*) &muteButton, (juce::Component*) &soloButton,
                     (juce::Component*) &detectLabel, (juce::Component*) &detectButton, (juce::Component*) &tuneButton,
                     (juce::Component*) &snapButton, (juce::Component*) &tuneAllButton, (juce::Component*) &editButton,
                     (juce::Component*) &playButton, (juce::Component*) &clearButton, (juce::Component*) &saveMidiButton,
                     (juce::Component*) &bpmSlider })
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

    // --- Patron MIDI ---
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

    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 56, 22);
    bpmSlider.setNumDecimalPlacesToDisplay (0);
    bpmAtt = std::make_unique<SliderAtt> (proc.apvts, "bpm", bpmSlider);

    setSize (1180, 730);
    proc.addChangeListener (this);
    applyAccent (juce::jlimit (0, numAccents - 1, proc.accentIndex.load()));
    setBank (0);
}

DrumkitEditor::~DrumkitEditor()
{
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
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
    static const char* ids[] = { "clean", "gain", "pan", "tune", "attack", "fade", "hp", "lp" };

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
    g.fillAll (bgColour);

    const auto accent = laf.getAccent();

    // Barra superior
    g.setColour (accent);
    g.fillRoundedRectangle (16.0f, 20.0f, 14.0f, 14.0f, 3.0f);
    g.setColour (textColour);
    g.setFont (16.0f);
    g.drawText ("DRUMKIT CLONER", 40, 14, 300, 26, juce::Justification::centredLeft);
    drawLabel (g, "COLOR", accentBox.getX() - 52, accentBox.getY() + 11, 48, juce::Justification::centredRight);

    // Paneles
    drawPanel (g, rightPanel);
    drawPanel (g, bottomPanel);

    // Panel derecho
    const auto name = proc.getPadName (selectedPad);
    g.setColour (accent);
    g.setFont (14.0f);
    g.drawText ("PAD " + juce::String (selectedPad + 1) + (name.isNotEmpty() ? "  ·  " + name : juce::String()),
                waveArea.getX(), waveArea.getY() - 26, waveArea.getWidth(), 22, juce::Justification::centredLeft);

    g.setColour (panelDark);
    g.fillRoundedRectangle (waveArea.toFloat(), 8.0f);

    if (auto* data = proc.getSampleData (selectedPad))
        drawPeaks (g, *data, waveArea.toFloat().reduced (8.0f, 10.0f), accent);
    else
    {
        g.setColour (textColour.withAlpha (0.35f));
        g.setFont (13.0f);
        g.drawText ("Arrastra un sample sobre un pad", waveArea, juce::Justification::centred);
    }

    for (int i = 0; i < numKnobs; ++i)
        drawLabel (g, knobs[i].getName(), knobs[i].getX(), knobs[i].getY() - 14, knobs[i].getWidth(), juce::Justification::centred);

    drawLabel (g, "CHOKE", chokeBox.getX(), chokeBox.getY() - 15, chokeBox.getWidth(), juce::Justification::left);
    drawLabel (g, "OUT", outBox.getX(), outBox.getY() - 15, outBox.getWidth(), juce::Justification::left);
    drawLabel (g, "CORRECTOR DE PITCH  ·  nota destino", noteBox.getX(), detectLabel.getY() - 16, 320, juce::Justification::left);

    // Panel inferior
    drawLabel (g, "MIDI PATTERN  ·  2 compases", bottomPanel.getX() + 16, bottomPanel.getY() + 6, 200, juce::Justification::left);
    drawLabel (g, "BPM", bpmSlider.getX() - 28, bpmSlider.getY() + 4, 26, juce::Justification::left);
}

void DrumkitEditor::resized()
{
    auto r = getLocalBounds().reduced (16);

    auto top = r.removeFromTop (40);
    accentBox.setBounds (top.removeFromRight (110).withSizeKeepingCentre (110, 28));

    r.removeFromTop (10);
    bottomPanel = r.removeFromBottom (78);
    r.removeFromBottom (10);

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
    waveArea = right.removeFromTop (130);
    right.removeFromTop (8);

    const int kw = right.getWidth() / 4;
    for (int rr = 0; rr < 2; ++rr)
    {
        auto row = right.removeFromTop (120);
        for (int c = 0; c < 4; ++c)
            knobs[rr * 4 + c].setBounds (row.removeFromLeft (kw).withTrimmedTop (16).reduced (2));
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

    // Panel inferior
    auto bar = bottomPanel.withTrimmedTop (18).reduced (16, 8);
    editButton.setBounds (bar.removeFromLeft (76).withSizeKeepingCentre (76, 34));
    bar.removeFromLeft (8);
    playButton.setBounds (bar.removeFromLeft (76).withSizeKeepingCentre (76, 34));
    bar.removeFromLeft (44);
    bpmSlider.setBounds (bar.removeFromLeft (240).withSizeKeepingCentre (240, 30));
    bar.removeFromLeft (20);
    clearButton.setBounds (bar.removeFromLeft (70).withSizeKeepingCentre (70, 34));
    bar.removeFromLeft (8);
    saveMidiButton.setBounds (bar.removeFromLeft (100).withSizeKeepingCentre (100, 34));
    bar.removeFromLeft (8);
    midiDrag.setBounds (bar.removeFromLeft (180).withSizeKeepingCentre (180, 34));
}
