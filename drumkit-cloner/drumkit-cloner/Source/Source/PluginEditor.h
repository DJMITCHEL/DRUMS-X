#pragma once

#include "PluginProcessor.h"

class DrumkitEditor;

//==============================================================================
// Aspecto: knobs con arco de color, botones planos. El color de acento es cambiable.
class DkcLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DkcLookAndFeel();
    void setAccent (juce::Colour c);
    juce::Colour getAccent() const { return accent; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
private:
    juce::Colour accent;
};

//==============================================================================
// Un pad: click = tocar, arrastrar archivo encima = cargarlo. Se ilumina al sonar.
class PadComponent : public juce::Component,
                     public juce::FileDragAndDropTarget,
                     private juce::Timer
{
public:
    PadComponent (DrumkitEditor& e, DrumkitProcessor& p);

    void setPad (int idx) { padIndex = idx; repaint(); }
    int getPad() const { return padIndex; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true;  repaint(); }
    void fileDragExit (const juce::StringArray&) override            { dragOver = false; repaint(); }

private:
    void timerCallback() override;

    DrumkitEditor& editor;
    DrumkitProcessor& proc;
    int padIndex = 0;
    int heldNote = -1;
    float lit = 0.0f;
    bool dragOver = false;
};

//==============================================================================
// Editor visual del sample: onda original (fantasma) + onda final despues de cortar,
// fades, compresion y ganancia. Arrastra las asas START / END para cortar.
class SampleEditor : public juce::Component, private juce::Timer
{
public:
    SampleEditor (DrumkitEditor& e, DrumkitProcessor& p);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    float val (const char* id) const;
    juce::Rectangle<float> getWaveRect() const;
    int hitHandle (float x) const;
    void timerCallback() override;

    DrumkitEditor& editor;
    DrumkitProcessor& proc;
    int dragging = 0;                       // 0 nada, 1 start, 2 end
    juce::RangedAudioParameter* dragParam = nullptr;
    double lastSig = -1.0;
};

//==============================================================================
// Rejilla del patron MIDI (16 pads del banco x 32 pasos) con el nombre de cada instrumento
class PatternGrid : public juce::Component, private juce::Timer
{
public:
    PatternGrid (DrumkitEditor& e, DrumkitProcessor& p);

    void setBank (int b) { bank = b; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { lastPad = lastStep = -1; }

private:
    bool cellAt (juce::Point<int> pos, int& pad, int& step) const;
    void applyAt (juce::Point<int> pos);
    void timerCallback() override;

    DrumkitEditor& editor;
    DrumkitProcessor& proc;
    int bank = 0, paintValue = 0, lastPad = -1, lastStep = -1, shownStep = -2;
    static constexpr int gutter = 96;
};

//==============================================================================
// Panel SOURCE: onda de la cancion con los golpes detectados. Arrastra aqui un archivo.
class SourcePanel : public juce::Component,
                    public juce::FileDragAndDropTarget,
                    private juce::Timer
{
public:
    SourcePanel (DrumkitEditor& e, DrumkitProcessor& p);

    void paint (juce::Graphics&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true;  repaint(); }
    void fileDragExit (const juce::StringArray&) override            { dragOver = false; repaint(); }

private:
    void timerCallback() override;

    DrumkitEditor& editor;
    DrumkitProcessor& proc;
    bool dragOver = false, wasCloning = false;
};

//==============================================================================
// Boton "arrastra el MIDI a tu DAW"
class MidiDragSource : public juce::Component
{
public:
    explicit MidiDragSource (DrumkitProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
private:
    DrumkitProcessor& proc;
    bool dragging = false;
};

//==============================================================================
class DrumkitEditor : public juce::AudioProcessorEditor,
                      private juce::ChangeListener
{
public:
    explicit DrumkitEditor (DrumkitProcessor&);
    ~DrumkitEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void selectPad (int pad);
    int getSelectedPad() const { return selectedPad; }
    juce::Colour getAccent() const { return laf.getAccent(); }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    void setBank (int bank);
    void rebuildAttachments();
    void applyAccent (int idx);
    void showEditMode (bool on);
    void setPitchMessage (const juce::String& text);
    void generate (bool hatsOnly);

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    static constexpr int numKnobs = 9;

    DkcLookAndFeel laf;                       // primero: se destruye el ultimo
    DrumkitProcessor& proc;
    int selectedPad = 0;

    juce::OwnedArray<PadComponent> pads;
    SourcePanel sourcePanel;
    SampleEditor sampleEditor;
    PatternGrid patternGrid;
    MidiDragSource midiDrag;

    juce::TextButton bankButtons[dkc::numBanks];
    juce::Slider knobs[numKnobs];
    juce::ComboBox chokeBox, outBox, accentBox, noteBox, styleBox;
    juce::TextButton muteButton { "M" }, soloButton { "S" };

    juce::Label detectLabel;
    juce::TextButton detectButton { "DETECT" }, tuneButton { "TUNE" },
                     snapButton { "SNAP" }, tuneAllButton { "TUNE ALL" };

    juce::TextButton editButton { "EDIT" }, playButton { "PLAY" }, clearButton { "CLEAR" },
                     saveMidiButton { "SAVE .MID" }, saveWavButton { "SAVE .WAV" },
                     generateButton { "GENERATE" }, hatsButton { "HI-HATS" },
                     recloneButton { "RE-CLONE" }, browseButton { "BROWSE" };
    juce::Slider bpmSlider, sensSlider, intensitySlider, humTimeSlider, humVelSlider, swingSlider;

    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> waveArea, rightPanel, bottomPanel, gridArea, sourceBounds;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> labelList;

    // Despues de los controles: se destruyen antes que ellos
    std::unique_ptr<SliderAtt> knobAtts[numKnobs];
    std::unique_ptr<ComboAtt> chokeAtt, outAtt;
    std::unique_ptr<ButtonAtt> muteAtt, soloAtt;
    std::unique_ptr<SliderAtt> bpmAtt, humTimeAtt, humVelAtt, swingAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumkitEditor)
};
