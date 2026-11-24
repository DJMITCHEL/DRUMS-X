#include "PluginEditor.h"

namespace
{
    const juce::Colour bgColour   { 0xff14161b };
    const juce::Colour panelColour{ 0xff0d0f13 };
    const juce::Colour accent     { 0xffe8582b };
}

//==============================================================================
void PadComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    const bool loaded   = proc.getSampleData (padIndex) != nullptr;
    const bool selected = editor.getSelectedPad() == padIndex;

    const auto base = loaded ? juce::Colour::fromHSV ((float) (padIndex % 8) / 8.0f, 0.65f, 0.85f, 1.0f)
                             : juce::Colours::grey;

    g.setColour (base.withAlpha (loaded ? 0.30f : 0.10f));
    g.fillRoundedRectangle (b, 8.0f);

    if (heldNote >= 0)
    {
        g.setColour (base.withAlpha (0.35f));
        g.fillRoundedRectangle (b, 8.0f);
    }

    g.setColour (dragOver ? juce::Colours::white : (selected ? accent : base.withAlpha (0.5f)));
    g.drawRoundedRectangle (b, 8.0f, (selected || dragOver) ? 2.5f : 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (11.0f);
    g.drawText (juce::MidiMessage::getMidiNoteName (dkc::firstMidiNote + padIndex, true, true, 3),
                getLocalBounds().reduced (8, 6), juce::Justification::topRight);

    g.setColour (juce::Colours::white.withAlpha (loaded ? 0.9f : 0.35f));
    g.setFont (loaded ? 14.0f : 28.0f);
    g.drawFittedText (loaded ? proc.getPadName (padIndex) : juce::String ("+"),
                      getLocalBounds().reduced (10), juce::Justification::centred, 2);
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

    // Varios archivos: se reparten en pads consecutivos
    for (int i = 0; i < files.size() && padIndex + i < dkc::numPads; ++i)
        proc.loadSample (padIndex + i, juce::File (files[i]));

    editor.selectPad (padIndex);
    repaint();
}

//==============================================================================
DrumkitEditor::DrumkitEditor (DrumkitProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    for (int i = 0; i < dkc::padsPerBank; ++i)
        addAndMakeVisible (pads.add (new PadComponent (*this, proc)));

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
        knobs[i].setNumDecimalPlacesToDisplay (i >= 6 ? 0 : 1);
        knobs[i].setColour (juce::Slider::rotarySliderFillColourId, accent);
        addAndMakeVisible (knobs[i]);
    }

    chokeBox.addItemList (dkc::chokeChoices(), 1);
    outBox.addItemList (dkc::outputChoices(), 1);
    addAndMakeVisible (chokeBox);
    addAndMakeVisible (outBox);
    addAndMakeVisible (muteButton);
    addAndMakeVisible (soloButton);

    setSize (960, 560);
    proc.addChangeListener (this);
    setBank (0);
}

DrumkitEditor::~DrumkitEditor()
{
    proc.removeChangeListener (this);
}

void DrumkitEditor::setBank (int bank)
{
    for (int i = 0; i < dkc::padsPerBank; ++i)
        pads[i]->setPad (bank * dkc::padsPerBank + i);

    selectPad (bank * dkc::padsPerBank + (selectedPad % dkc::padsPerBank));
}

void DrumkitEditor::selectPad (int pad)
{
    selectedPad = juce::jlimit (0, dkc::numPads - 1, pad);
    rebuildAttachments();
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

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText ("DRUMKIT CLONER  ·  Fase 1: sampler de 64 pads", 14, 8, 600, 24, juce::Justification::left);

    // Panel derecho: titulo del pad
    g.setColour (accent);
    g.setFont (14.0f);
    const auto name = proc.getPadName (selectedPad);
    g.drawText ("PAD " + juce::String (selectedPad + 1) + (name.isNotEmpty() ? "  ·  " + name : juce::String()),
                waveArea.getX(), waveArea.getY() - 26, waveArea.getWidth(), 22, juce::Justification::left);

    // Forma de onda
    g.setColour (panelColour);
    g.fillRoundedRectangle (waveArea.toFloat(), 6.0f);

    if (auto* data = proc.getSampleData (selectedPad))
    {
        const int n = data->getNumSamples();
        const float* d = data->getReadPointer (0);
        const int w = waveArea.getWidth();
        const float midY = (float) waveArea.getCentreY();
        const float h = (float) waveArea.getHeight() * 0.45f;

        g.setColour (accent);
        for (int x = 0; x < w; ++x)
        {
            const int s0 = (int) ((juce::int64) x * n / w);
            const int s1 = juce::jmax (s0 + 1, (int) ((juce::int64) (x + 1) * n / w));
            const int stride = juce::jmax (1, (s1 - s0) / 32);

            float peak = 0.0f;
            for (int s = s0; s < s1 && s < n; s += stride)
                peak = juce::jmax (peak, std::abs (d[s]));

            g.drawVerticalLine (waveArea.getX() + x, midY - peak * h, midY + peak * h);
        }
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (13.0f);
        g.drawText ("Arrastra un sample (o varios) sobre un pad", waveArea, juce::Justification::centred);
    }

    // Etiquetas de los knobs y combos
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.setFont (11.0f);
    for (int i = 0; i < numKnobs; ++i)
        g.drawText (knobs[i].getName(), knobs[i].getX(), knobs[i].getY() - 16,
                    knobs[i].getWidth(), 14, juce::Justification::centred);

    g.drawText ("CHOKE", chokeBox.getX(), chokeBox.getY() - 15, chokeBox.getWidth(), 13, juce::Justification::left);
    g.drawText ("OUT",   outBox.getX(),   outBox.getY()   - 15, outBox.getWidth(),   13, juce::Justification::left);
}

void DrumkitEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (36);

    auto bankCol = area.removeFromLeft (50);
    for (int b = 0; b < dkc::numBanks; ++b)
        bankButtons[b].setBounds (bankCol.removeFromTop (60).reduced (4));

    area.removeFromLeft (10);
    auto grid = area.removeFromLeft (4 * 118);

    // Como un Drum Rack: la fila 1 (pads 1-4) abajo
    for (int i = 0; i < dkc::padsPerBank; ++i)
    {
        const int col = i % 4, row = i / 4;
        pads[i]->setBounds (grid.getX() + col * 118, grid.getY() + (3 - row) * 118, 114, 114);
    }

    area.removeFromLeft (14);
    auto right = area;
    right.removeFromTop (28);
    waveArea = right.removeFromTop (120);
    right.removeFromTop (10);

    const int kw = right.getWidth() / 4;
    for (int r = 0; r < 2; ++r)
    {
        auto row = right.removeFromTop (130);
        for (int c = 0; c < 4; ++c)
            knobs[r * 4 + c].setBounds (row.removeFromLeft (kw).withTrimmedTop (18).reduced (4));
    }

    right.removeFromTop (20);
    auto ctrl = right.removeFromTop (28);
    chokeBox.setBounds (ctrl.removeFromLeft (80));
    ctrl.removeFromLeft (10);
    outBox.setBounds (ctrl.removeFromLeft (130));
    ctrl.removeFromLeft (14);
    muteButton.setBounds (ctrl.removeFromLeft (46));
    soloButton.setBounds (ctrl.removeFromLeft (46));
}
