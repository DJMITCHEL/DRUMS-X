#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace dkc
{
    constexpr int numPads = 64;
    constexpr int padsPerBank = 16;
    constexpr int numBanks = 4;
    constexpr int firstMidiNote = 36;   // C1 = pad 1 (convencion Ableton)

    inline juce::String pid (int pad, const juce::String& name)
    {
        return "p" + juce::String (pad + 1).paddedLeft ('0', 2) + "_" + name;
    }

    inline juce::StringArray chokeChoices() { return { "Off", "1", "2", "3", "4" }; }

    inline juce::StringArray familyNames()
    {
        return { "Kick", "Snare", "Clap", "Hat", "Open Hat", "Tom", "Perc", "Cymbal" };
    }

    inline juce::StringArray outputChoices()
    {
        juce::StringArray s { "Main" };
        for (auto& f : familyNames())
            s.add ("Main + " + f);
        return s;
    }
}

class DrumkitProcessor : public juce::AudioProcessor,
                         public juce::ChangeBroadcaster
{
public:
    DrumkitProcessor();
    ~DrumkitProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Drumkit Cloner"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- API para la interfaz (hilo de mensajes) ---
    bool loadSample (int pad, const juce::File& file);
    const juce::AudioBuffer<float>* getSampleData (int pad) const;
    juce::String getPadName (int pad) const;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    static BusesProperties makeBuses();
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float z1[2] {}, z2[2] {};

        void set (bool highPass, double sr, float freq);
        float process (int ch, float x)
        {
            const float y = b0 * x + z1[ch];
            z1[ch] = b1 * x - a1 * y + z2[ch];
            z2[ch] = b2 * x - a2 * y;
            return y;
        }
        void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }
    };

    struct Voice
    {
        bool active = false;
        const juce::AudioBuffer<float>* data = nullptr;
        double srcRate = 44100.0, pos = 0.0;
        juce::int64 age = 0;
        float vel = 1.0f;
        Biquad hp, lp;
        float lastHp = -1.0f, lastLp = -1.0f;
    };

    struct Snap
    {
        float gain = 1, panL = 1, panR = 1, hp = 20, lp = 20000;
        double tuneRatio = 1.0;
        int attackSamples = 0, fadeSamples = 0, out = 0, choke = 0;
        bool useHp = false, useLp = false, mute = false, solo = false;
    };

    struct PadParams
    {
        std::atomic<float> *gain, *pan, *tune, *attack, *fade, *hp, *lp,
                           *clean, *choke, *mute, *solo, *out;
    };

    struct Slot
    {
        const juce::AudioBuffer<float>* data = nullptr;
        double rate = 44100.0;
        juce::String name, path;
    };

    void updateSnapshots();
    void trigger (int pad, float velocity);
    void stopAll();
    void render (juce::AudioBuffer<float>& buffer, int start, int end);

    std::array<PadParams, dkc::numPads> params {};
    std::array<Slot, dkc::numPads> slots;
    std::array<Voice, dkc::numPads> voices;
    std::array<Snap, dkc::numPads> snaps;
    bool anySolo = false;
    double hostRate = 44100.0;

    mutable juce::SpinLock slotLock;
    // Los buffers no se liberan hasta cerrar el plugin: asi el hilo de audio
    // nunca lee memoria liberada (simple y seguro para la Fase 1).
    std::vector<std::shared_ptr<juce::AudioBuffer<float>>> keepAlive;
    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumkitProcessor)
};
