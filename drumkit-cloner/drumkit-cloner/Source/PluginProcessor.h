#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "PitchDetect.h"
#include "Analyzer.h"

namespace dkc
{
    constexpr int numPads = 64;
    constexpr int padsPerBank = 16;
    constexpr int numBanks = 4;
    constexpr int firstMidiNote = 36;   // C1 = pad 1 (convencion Ableton)
    constexpr int numSteps = 32;        // patron: 2 compases a 1/16

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

    // --- Corrector de pitch ---
    dkc::PitchResult analysePitch (int pad) const;
    // Afina el pad a una nota MIDI concreta (o a la semitono mas cercano si snapToNearest)
    dkc::PitchResult tunePad (int pad, float targetMidi, bool snapToNearest);

    // --- Patron MIDI ---
    int  getStepVel (int pad, int step) const;
    void setStepVel (int pad, int step, int vel);
    void clearPattern();
    bool writeMidiFile (const juce::File& file) const;
    void setSequencerPlaying (bool shouldPlay) { seqPlaying.store (shouldPlay); }
    bool isSequencerPlaying() const { return seqPlaying.load(); }
    int  getCurrentStep() const { return currentStep.load(); }

    std::atomic<int> accentIndex { 0 };

    // --- Clonar un kit desde una cancion / loop ---
    bool cloneFromFile (const juce::File& file);
    bool recloneSource();                       // repite el analisis con otra sensibilidad
    bool isCloning() const { return cloneProgress.load() >= 0.0f; }
    float getCloneProgress() const { return cloneProgress.load(); }
    std::shared_ptr<const dkc::AnalysisResult> getAnalysis() const { return analysis; }
    juce::String getCloneStatus() const { return cloneStatus; }
    int getPadFamily (int pad) const;
    std::atomic<float> sensitivity { 0.5f };

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    struct CloneJob;
    void startJob (const juce::File& file, std::shared_ptr<juce::AudioBuffer<float>> existing,
                   double rate, const juce::String& name);
    void finishJob (std::shared_ptr<juce::AudioBuffer<float>> buf, double sr, const juce::String& name,
                    std::shared_ptr<const dkc::AnalysisResult> result, bool ok);
    void installClone (const dkc::AnalysisResult& res);
    void setParam (const juce::String& id, float plainValue);
    std::shared_ptr<juce::AudioBuffer<float>> readAudioFile (const juce::File& file, double& rate, double maxSeconds);

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
        const juce::AudioBuffer<float>* alt = nullptr;
        Biquad hp, lp;
        float lastHp = -1.0f, lastLp = -1.0f;
    };

    struct Snap
    {
        float gain = 1, panL = 1, panR = 1, hp = 20, lp = 20000, cleanAmt = 1;
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
        const juce::AudioBuffer<float>* alt = nullptr;   // golpe real (el principal es la mediana limpia)
        double rate = 44100.0;
        int family = -1;
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

    std::atomic<float>* bpmParam = nullptr;
    std::atomic<uint8_t> pattern[dkc::numPads * dkc::numSteps];
    std::atomic<bool> seqPlaying { false };
    std::atomic<int> currentStep { -1 };
    bool wasPlaying = false;
    int seqStep = 0;
    double nextStepIn = 0.0;

    mutable juce::SpinLock slotLock;
    // Los buffers no se liberan hasta cerrar el plugin: asi el hilo de audio
    // nunca lee memoria liberada (simple y seguro para la Fase 1).
    std::vector<std::shared_ptr<juce::AudioBuffer<float>>> keepAlive;
    juce::AudioFormatManager formatManager;

    std::unique_ptr<CloneJob> job;
    std::atomic<float> cloneProgress { -1.0f };
    std::shared_ptr<juce::AudioBuffer<float>> sourceBuffer;
    double sourceRate = 44100.0;
    juce::String sourceName, cloneStatus;
    std::shared_ptr<const dkc::AnalysisResult> analysis;

    JUCE_DECLARE_WEAK_REFERENCEABLE (DrumkitProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumkitProcessor)
};
