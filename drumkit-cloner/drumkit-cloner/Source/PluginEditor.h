#pragma once

#include "PluginProcessor.h"

class DrumkitEditor;

//==============================================================================
// Un pad de la rejilla: click = tocar, arrastrar un archivo encima = cargarlo
class PadComponent : public juce::Component,
                     public juce::FileDragAndDropTarget
{
public:
    PadComponent (DrumkitEditor& e, DrumkitProcessor& p) : editor (e), proc (p) {}

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
    DrumkitEditor& editor;
    DrumkitProcessor& proc;
    int padIndex = 0;
    int heldNote = -1;
    bool dragOver = false;
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

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    void setBank (int bank);
    void rebuildAttachments();

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    static constexpr int numKnobs = 8;

    DrumkitProcessor& proc;
    int selectedPad = 0;

    juce::OwnedArray<PadComponent> pads;
    juce::TextButton bankButtons[dkc::numBanks];
    juce::Slider knobs[numKnobs];
    juce::ComboBox chokeBox, outBox;
    juce::ToggleButton muteButton { "M" }, soloButton { "S" };
    juce::Rectangle<int> waveArea;

    // Se declaran DESPUES de los controles para destruirse antes que ellos
    std::unique_ptr<SliderAtt> knobAtts[numKnobs];
    std::unique_ptr<ComboAtt> chokeAtt, outAtt;
    std::unique_ptr<ButtonAtt> muteAtt, soloAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumkitEditor)
};
