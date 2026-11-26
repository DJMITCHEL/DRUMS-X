#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <random>
#include <set>

namespace
{
    bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buf, double rate)
    {
        file.deleteFile();
        std::unique_ptr<juce::FileOutputStream> os (file.createOutputStream());
        if (os == nullptr) return false;

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (os.get(), rate, (unsigned int) buf.getNumChannels(), 24, {}, 0));
        if (writer == nullptr) return false;

        os.release();   // el writer se encarga del stream
        return writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    }
}

//==============================================================================
// Hilo de analisis: lee la cancion, detecta, clasifica, agrupa y aisla
struct DrumkitProcessor::CloneJob : public juce::Thread
{
    CloneJob (DrumkitProcessor& p, juce::File f, std::shared_ptr<juce::AudioBuffer<float>> existing,
              double rate, juce::String srcName, float sens)
        : juce::Thread ("Drumkit Cloner analysis"), owner (p), file (std::move (f)),
          buffer (std::move (existing)), sampleRate (rate), name (std::move (srcName)), sensitivity (sens) {}

    ~CloneJob() override { stopThread (20000); }

    void run() override
    {
        bool ok = true;

        if (buffer == nullptr)
        {
            buffer = owner.readAudioFile (file, sampleRate, 8.0 * 60.0);   // maximo 8 minutos
            name = file.getFileNameWithoutExtension();
        }
        if (threadShouldExit()) return;

        std::shared_ptr<dkc::AnalysisResult> result;
        if (buffer == nullptr)
        {
            ok = false;
        }
        else
        {
            result = std::make_shared<dkc::AnalysisResult> (
                dkc::analyse (*buffer, sampleRate, sensitivity, &owner.cloneProgress,
                              [this] { return threadShouldExit(); }));
            result->name = name;
            if (threadShouldExit()) return;
        }

        juce::WeakReference<DrumkitProcessor> weak (&owner);
        auto buf = buffer;
        const double sr = sampleRate;
        const juce::String nm = name;

        juce::MessageManager::callAsync ([weak, buf, sr, nm, result, ok]
        {
            if (weak != nullptr)
                weak->finishJob (buf, sr, nm, result, ok);
        });
    }

    DrumkitProcessor& owner;
    juce::File file;
    std::shared_ptr<juce::AudioBuffer<float>> buffer;
    double sampleRate;
    juce::String name;
    float sensitivity;
};

//==============================================================================
void DrumkitProcessor::Biquad::set (bool highPass, double sr, float freq)
{
    freq = juce::jlimit (10.0f, (float) (sr * 0.45), freq);
    const double w0 = juce::MathConstants<double>::twoPi * freq / sr;
    const double c = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * 0.70710678);   // Q = 0.707 -> 12 dB/oct

    double nb0, nb1, nb2;
    if (highPass) { nb0 = (1.0 + c) * 0.5; nb1 = -(1.0 + c); nb2 = nb0; }
    else          { nb0 = (1.0 - c) * 0.5; nb1 =  (1.0 - c); nb2 = nb0; }

    const double a0 = 1.0 + alpha;
    b0 = (float) (nb0 / a0);
    b1 = (float) (nb1 / a0);
    b2 = (float) (nb2 / a0);
    a1 = (float) ((-2.0 * c) / a0);
    a2 = (float) ((1.0 - alpha) / a0);
}

//==============================================================================
juce::AudioProcessor::BusesProperties DrumkitProcessor::makeBuses()
{
    auto b = BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true);
    for (auto& name : dkc::familyNames())
        b = b.withOutput (name, juce::AudioChannelSet::stereo(), false);
    return b;
}

juce::AudioProcessorValueTreeState::ParameterLayout DrumkitProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&layout] (const String& id, const String& name,
                               float lo, float hi, float def, float skew = 1.0f)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.0f, skew), def));
    };

    addFloat ("bpm", "BPM", 40.0f, 240.0f, 90.0f);
    addFloat ("hum_time", "Humanize Timing", 0.0f, 1.0f, 0.0f);
    addFloat ("hum_vel", "Humanize Velocity", 0.0f, 1.0f, 0.0f);
    addFloat ("swing", "Swing", 0.0f, 1.0f, 0.0f);

    for (int p = 0; p < dkc::numPads; ++p)
    {
        const String n = "Pad " + String (p + 1) + " ";

        addFloat (dkc::pid (p, "gain"),   n + "Gain",   -60.0f, 12.0f, 0.0f);
        addFloat (dkc::pid (p, "pan"),    n + "Pan",    -1.0f,  1.0f,  0.0f);
        addFloat (dkc::pid (p, "tune"),   n + "Tune",   -24.0f, 24.0f, 0.0f);
        addFloat (dkc::pid (p, "attack"), n + "Attack", 0.0f,   500.0f,  0.0f, 0.4f);
        addFloat (dkc::pid (p, "fade"),   n + "Fade",   0.0f,   2000.0f, 0.0f, 0.4f);
        addFloat (dkc::pid (p, "hp"),     n + "HP",     20.0f,  20000.0f, 20.0f,    0.3f);
        addFloat (dkc::pid (p, "lp"),     n + "LP",     20.0f,  20000.0f, 20000.0f, 0.3f);
        addFloat (dkc::pid (p, "clean"),  n + "Clean",  0.0f,   1.0f,  0.0f);
        addFloat (dkc::pid (p, "comp"),   n + "Comp",   0.0f,   1.0f,  0.0f);
        addFloat (dkc::pid (p, "start"),  n + "Start",  0.0f,   1.0f,  0.0f);
        addFloat (dkc::pid (p, "end"),    n + "End",    0.0f,   1.0f,  1.0f);

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { dkc::pid (p, "choke"), 1 }, n + "Choke", dkc::chokeChoices(), 0));
        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { dkc::pid (p, "out"), 1 }, n + "Output", dkc::outputChoices(), 0));
        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { dkc::pid (p, "mute"), 1 }, n + "Mute", false));
        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { dkc::pid (p, "solo"), 1 }, n + "Solo", false));
    }
    return layout;
}

//==============================================================================
DrumkitProcessor::DrumkitProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "STATE", createLayout())
{
    formatManager.registerBasicFormats();

    for (auto& v : pattern) v.store (0);
    for (auto& v : ratchet) v.store (1);
    for (auto& f : padFlash) f.store (0.0f);

    bpmParam = apvts.getRawParameterValue ("bpm");
    humTimeParam = apvts.getRawParameterValue ("hum_time");
    humVelParam = apvts.getRawParameterValue ("hum_vel");
    swingParam = apvts.getRawParameterValue ("swing");

    for (int p = 0; p < dkc::numPads; ++p)
    {
        auto get = [this, p] (const char* id) { return apvts.getRawParameterValue (dkc::pid (p, id)); };
        auto& pp = params[(size_t) p];
        pp.gain = get ("gain");     pp.pan = get ("pan");     pp.tune = get ("tune");
        pp.attack = get ("attack"); pp.fade = get ("fade");   pp.hp = get ("hp");
        pp.lp = get ("lp");         pp.clean = get ("clean"); pp.choke = get ("choke");
        pp.mute = get ("mute");     pp.solo = get ("solo");   pp.out = get ("out");
        pp.start = get ("start");   pp.end = get ("end");     pp.comp = get ("comp");
    }
}

DrumkitProcessor::~DrumkitProcessor()
{
    job.reset();   // para el hilo de analisis antes de destruir el resto
}

bool DrumkitProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        const auto& s = layouts.outputBuses.getReference (i);
        if (! s.isDisabled() && s != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void DrumkitProcessor::prepareToPlay (double sampleRate, int)
{
    hostRate = sampleRate;
    stopAll (engine);
    numPending = 0;
}

juce::AudioProcessorEditor* DrumkitProcessor::createEditor()
{
    return new DrumkitEditor (*this);
}

//==============================================================================
std::shared_ptr<juce::AudioBuffer<float>> DrumkitProcessor::readAudioFile (const juce::File& file, double& rate, double maxSeconds)
{
    if (! file.existsAsFile()) return nullptr;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return nullptr;

    rate = reader->sampleRate;
    const juce::int64 maxLen = (juce::int64) (reader->sampleRate * maxSeconds);
    const int numSamples = (int) juce::jmin (reader->lengthInSamples, maxLen);
    const int numChannels = (int) juce::jmin (2u, reader->numChannels);

    auto buf = std::make_shared<juce::AudioBuffer<float>> (numChannels, numSamples);
    if (! reader->read (buf.get(), 0, numSamples, 0, true, true))
        return nullptr;
    return buf;
}

bool DrumkitProcessor::loadSample (int pad, const juce::File& file)
{
    if (pad < 0 || pad >= dkc::numPads) return false;

    double rate = 44100.0;
    auto buf = readAudioFile (file, rate, 30.0);   // maximo 30 s por pad
    if (buf == nullptr) return false;

    // Si existe "NOMBRE_raw.wav" al lado (kits clonados), se carga como golpe real
    std::shared_ptr<juce::AudioBuffer<float>> alt;
    const auto rawFile = file.getSiblingFile (file.getFileNameWithoutExtension() + "_raw.wav");
    if (rawFile.existsAsFile())
    {
        double r2 = rate;
        alt = readAudioFile (rawFile, r2, 30.0);
        if (alt != nullptr && (alt->getNumSamples() != buf->getNumSamples()
                               || alt->getNumChannels() != buf->getNumChannels()))
            alt = nullptr;
    }

    {
        const juce::SpinLock::ScopedLockType sl (slotLock);
        keepAlive.push_back (buf);
        if (alt != nullptr) keepAlive.push_back (alt);

        auto& s = slots[(size_t) pad];
        s.data = buf.get();
        s.alt = alt != nullptr ? alt.get() : nullptr;
        s.rate = rate;
        s.family = -1;
        s.name = file.getFileNameWithoutExtension();
        s.path = file.getFullPathName();
    }

    // Un sample nuevo empieza con el recorte completo
    setParam (dkc::pid (pad, "start"), 0.0f);
    setParam (dkc::pid (pad, "end"), 1.0f);

    sendChangeMessage();
    return true;
}

const juce::AudioBuffer<float>* DrumkitProcessor::getSampleData (int pad) const
{
    if (pad < 0 || pad >= dkc::numPads) return nullptr;
    const juce::SpinLock::ScopedLockType sl (slotLock);
    return slots[(size_t) pad].data;
}

double DrumkitProcessor::getSampleRateOf (int pad) const
{
    if (pad < 0 || pad >= dkc::numPads) return 44100.0;
    const juce::SpinLock::ScopedLockType sl (slotLock);
    return slots[(size_t) pad].rate;
}

juce::String DrumkitProcessor::getPadName (int pad) const
{
    if (pad < 0 || pad >= dkc::numPads) return {};
    const juce::SpinLock::ScopedLockType sl (slotLock);
    return slots[(size_t) pad].name;
}

int DrumkitProcessor::getPadFamily (int pad) const
{
    if (pad < 0 || pad >= dkc::numPads) return -1;
    const juce::SpinLock::ScopedLockType sl (slotLock);
    return slots[(size_t) pad].family;
}

// Nombre para mostrar en el piano roll / MIDI: nombre del sample, o la nota si el pad esta vacio
juce::String DrumkitProcessor::padLabel (int pad) const
{
    const auto n = getPadName (pad);
    return n.isNotEmpty() ? n : juce::MidiMessage::getMidiNoteName (dkc::firstMidiNote + pad, true, true, 3);
}

//==============================================================================
// MOTOR DE VOCES (se usa en tiempo real y tambien para exportar el loop a WAV)
//==============================================================================
void DrumkitProcessor::updateSnapshots (Engine& e, double sr) const
{
    e.anySolo = false;

    for (int p = 0; p < dkc::numPads; ++p)
    {
        const auto& pp = params[(size_t) p];
        auto& s = e.snaps[(size_t) p];

        s.gain = juce::Decibels::decibelsToGain (pp.gain->load(), -60.0f);

        const float pan = pp.pan->load();
        s.panL = juce::jmin (1.0f, 1.0f - pan);
        s.panR = juce::jmin (1.0f, 1.0f + pan);

        s.tuneRatio = std::pow (2.0, (double) pp.tune->load() / 12.0);
        s.attackSamples = (int) (pp.attack->load() * 0.001 * sr);
        s.fadeSamples   = (int) (pp.fade->load()   * 0.001 * sr);

        s.hp = pp.hp->load();
        s.lp = pp.lp->load();
        s.useHp = s.hp > 20.5f;
        s.useLp = s.lp < 19500.0f;

        const float comp = pp.comp->load();
        s.useComp = comp > 0.001f;
        s.comp = dkc::makeComp (comp);

        s.startFrac = juce::jlimit (0.0f, 0.998f, pp.start->load());
        s.endFrac = juce::jlimit (s.startFrac + 0.002f, 1.0f, pp.end->load());

        s.cleanAmt = juce::jlimit (0.0f, 1.0f, pp.clean->load());
        s.mute  = pp.mute->load() > 0.5f;
        s.solo  = pp.solo->load() > 0.5f;
        s.out   = (int) std::lround (pp.out->load());
        s.choke = (int) std::lround (pp.choke->load());

        e.anySolo = e.anySolo || s.solo;
    }
}

void DrumkitProcessor::stopAll (Engine& e)
{
    for (auto& v : e.voices) v.active = false;
}

void DrumkitProcessor::triggerVoice (Engine& e, int pad, float velocity, bool blocking)
{
    if (pad < 0 || pad >= dkc::numPads) return;

    const juce::AudioBuffer<float>* data = nullptr;
    const juce::AudioBuffer<float>* alt = nullptr;
    double rate = 44100.0;

    auto readSlot = [&]
    {
        data = slots[(size_t) pad].data;
        alt = slots[(size_t) pad].alt;
        rate = slots[(size_t) pad].rate;
    };

    if (blocking)
    {
        const juce::SpinLock::ScopedLockType sl (slotLock);
        readSlot();
    }
    else
    {
        const juce::SpinLock::ScopedTryLockType tl (slotLock);
        if (! tl.isLocked()) return;
        readSlot();
    }
    if (data == nullptr) return;

    const auto& s = e.snaps[(size_t) pad];
    const int len = data->getNumSamples();
    const int startIdx = juce::jlimit (0, len - 2, (int) (s.startFrac * (float) len));
    const int endIdx = juce::jlimit (startIdx + 2, len, (int) (s.endFrac * (float) len));

    auto& v = e.voices[(size_t) pad];
    v.data = data;
    v.srcRate = rate;
    v.alt = (alt != nullptr && alt->getNumSamples() == len && alt->getNumChannels() >= 1) ? alt : nullptr;
    v.pos = (double) startIdx;
    v.endIdx = endIdx;
    v.age = 0;
    v.vel = velocity;
    v.active = true;
    v.hp.reset();
    v.lp.reset();
    v.lastHp = v.lastLp = -1.0f;

    if (&e == &engine)
        padFlash[(size_t) pad].store (velocity);

    if (s.choke > 0)
        for (int j = 0; j < dkc::numPads; ++j)
            if (j != pad && e.snaps[(size_t) j].choke == s.choke)
                e.voices[(size_t) j].active = false;
}

void DrumkitProcessor::renderEngine (Engine& e, double sr, float* mainL, float* mainR,
                                     float* const* sepL, float* const* sepR, int start, int end) const
{
    if (end <= start) return;

    for (int p = 0; p < dkc::numPads; ++p)
    {
        auto& v = e.voices[(size_t) p];
        if (! v.active || v.data == nullptr) continue;

        const auto& s = e.snaps[(size_t) p];
        const bool audible = ! s.mute && (! e.anySolo || s.solo);
        if (! audible) { v.active = false; continue; }

        float* oL = nullptr;
        float* oR = nullptr;
        if (sepL != nullptr && s.out > 0 && s.out <= 8)   // salida separada (siempre suena tambien en Main)
        {
            oL = sepL[s.out - 1];
            oR = sepR[s.out - 1];
        }

        if (s.useHp && v.lastHp != s.hp) { v.hp.set (true,  sr, s.hp); v.lastHp = s.hp; }
        if (s.useLp && v.lastLp != s.lp) { v.lp.set (false, sr, s.lp); v.lastLp = s.lp; }

        const float* L = v.data->getReadPointer (0);
        const float* R = v.data->getNumChannels() > 1 ? v.data->getReadPointer (1) : L;
        const bool useAlt = v.alt != nullptr && s.cleanAmt < 0.999f;
        const float* AL = useAlt ? v.alt->getReadPointer (0) : nullptr;
        const float* AR = useAlt ? (v.alt->getNumChannels() > 1 ? v.alt->getReadPointer (1) : AL) : nullptr;
        const double step = v.srcRate / sr * s.tuneRatio;

        for (int i = start; i < end; ++i)
        {
            const int i0 = (int) v.pos;
            if (i0 >= v.endIdx - 1) { v.active = false; break; }

            const float frac = (float) (v.pos - (double) i0);
            float l = L[i0] + (L[i0 + 1] - L[i0]) * frac;
            float r = R[i0] + (R[i0 + 1] - R[i0]) * frac;

            if (useAlt)   // Clean: 0 = golpe real, 1 = mediana limpia
            {
                const float al = AL[i0] + (AL[i0 + 1] - AL[i0]) * frac;
                const float ar = AR[i0] + (AR[i0 + 1] - AR[i0]) * frac;
                l = al + (l - al) * s.cleanAmt;
                r = ar + (r - ar) * s.cleanAmt;
            }

            if (s.useHp) { l = v.hp.process (0, l); r = v.hp.process (1, r); }
            if (s.useLp) { l = v.lp.process (0, l); r = v.lp.process (1, r); }

            if (s.useComp)
            {
                l = dkc::compShape (l, s.comp);
                r = dkc::compShape (r, s.comp);
            }

            float g = v.vel * s.gain;

            if (s.attackSamples > 0 && v.age < s.attackSamples)
                g *= (float) v.age / (float) s.attackSamples;

            if (s.fadeSamples > 0)
            {
                const double remaining = ((double) (v.endIdx - 1) - v.pos) / step;
                if (remaining < (double) s.fadeSamples)
                    g *= (float) (remaining / (double) s.fadeSamples);
            }

            const float outL = l * g * s.panL;
            const float outR = r * g * s.panR;

            mainL[i] += outL;
            mainR[i] += outR;
            if (oL != nullptr) { oL[i] += outL; oR[i] += outR; }

            v.pos += step;
            ++v.age;
        }
    }
}

//==============================================================================
// Secuenciador en tiempo real: ratchets (rolls), swing y humanizacion
void DrumkitProcessor::fireStep (int step, double t, double stepLen)
{
    const float swing = swingParam->load();
    const float ht = humTimeParam->load();
    const float hv = humVelParam->load();
    const double jMax = (double) ht * 0.025 * hostRate;   // hasta 25 ms de variacion

    for (int p = 0; p < dkc::numPads; ++p)
    {
        const int v = pattern[p * dkc::numSteps + step].load();
        if (v <= 0) continue;
        const int r = juce::jlimit (1, 4, (int) ratchet[p * dkc::numSteps + step].load());

        for (int j = 0; j < r; ++j)
        {
            if (numPending >= maxPending) return;

            double due = t + stepLen * (double) j / (double) r;
            if (step % 2 == 1) due += (double) swing * 0.33 * stepLen;
            due += jMax * (1.0 + (double) (rng.nextFloat() * 2.0f - 1.0f));   // siempre hacia delante: sin latencia negativa

            int vel = (int) ((float) v * (1.0f - 0.10f * (float) j));
            vel += (int) ((rng.nextFloat() * 2.0f - 1.0f) * hv * 30.0f);

            auto& pe = pending[numPending++];
            pe.pad = p;
            pe.vel = juce::jlimit (1, 127, vel);
            pe.due = due;
        }
    }
}

void DrumkitProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    auto mainBus = getBusBuffer (buffer, false, 0);
    if (mainBus.getNumChannels() < 1) return;
    float* mainL = mainBus.getWritePointer (0);
    float* mainR = mainBus.getWritePointer (mainBus.getNumChannels() > 1 ? 1 : 0);

    float* sepL[8] = {};
    float* sepR[8] = {};
    for (int i = 0; i < 8; ++i)
    {
        if (i + 1 >= getBusCount (false)) break;
        if (auto* bus = getBus (false, i + 1))
        {
            if (bus->isEnabled() && bus->getNumberOfChannels() >= 2)
            {
                auto bb = getBusBuffer (buffer, false, i + 1);
                sepL[i] = bb.getWritePointer (0);
                sepR[i] = bb.getWritePointer (1);
            }
        }
    }

    // --- Secuenciador interno ---
    const bool playing = seqPlaying.load();
    if (playing && ! wasPlaying) { seqStep = 0; nextStepIn = 0.0; numPending = 0; }
    if (! playing && wasPlaying) { currentStep.store (-1); numPending = 0; }
    wasPlaying = playing;

    if (playing)
    {
        const double bpm = juce::jlimit (40.0, 240.0, (double) bpmParam->load());
        const double stepLen = 60.0 / bpm / 4.0 * hostRate;
        double t = nextStepIn;

        while (t < (double) numSamples)
        {
            currentStep.store (seqStep);
            fireStep (seqStep, t, stepLen);
            seqStep = (seqStep + 1) % dkc::numSteps;
            t += stepLen;
        }
        nextStepIn = t - (double) numSamples;
    }

    // Saca del buffer de pendientes las notas que tocan en este bloque
    {
        int w = 0;
        for (int i = 0; i < numPending; ++i)
        {
            auto pe = pending[i];
            if (pe.due < (double) numSamples)
                midi.addEvent (juce::MidiMessage::noteOn (1, dkc::firstMidiNote + pe.pad, (juce::uint8) pe.vel),
                               juce::jmax (0, (int) pe.due));
            else
            {
                pe.due -= (double) numSamples;
                pending[w++] = pe;
            }
        }
        numPending = w;
    }

    // Mezcla los clicks de los pads de la interfaz con el MIDI del DAW
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    updateSnapshots (engine, hostRate);

    int pos = 0;
    for (const auto meta : midi)
    {
        const int evPos = juce::jlimit (pos, numSamples, meta.samplePosition);
        renderEngine (engine, hostRate, mainL, mainR, sepL, sepR, pos, evPos);
        pos = evPos;

        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            triggerVoice (engine, msg.getNoteNumber() - dkc::firstMidiNote, msg.getFloatVelocity(), false);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            stopAll (engine);
    }
    renderEngine (engine, hostRate, mainL, mainR, sepL, sepR, pos, numSamples);
}

//==============================================================================
void DrumkitProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();

    auto old = state.getChildWithName ("SAMPLES");
    if (old.isValid()) state.removeChild (old, nullptr);

    juce::ValueTree samples ("SAMPLES");
    {
        const juce::SpinLock::ScopedLockType sl (slotLock);
        for (int p = 0; p < dkc::numPads; ++p)
            if (slots[(size_t) p].path.isNotEmpty())
            {
                samples.setProperty ("pad" + juce::String (p), slots[(size_t) p].path, nullptr);
                samples.setProperty ("fam" + juce::String (p), slots[(size_t) p].family, nullptr);
            }
    }
    state.appendChild (samples, nullptr);

    auto oldExtra = state.getChildWithName ("EXTRA");
    if (oldExtra.isValid()) state.removeChild (oldExtra, nullptr);

    juce::ValueTree extra ("EXTRA");
    const size_t n = (size_t) (dkc::numPads * dkc::numSteps);
    juce::MemoryBlock mbVel (n, true), mbRat (n, true);
    auto* bv = static_cast<juce::uint8*> (mbVel.getData());
    auto* br = static_cast<juce::uint8*> (mbRat.getData());
    for (size_t i = 0; i < n; ++i)
    {
        bv[i] = pattern[i].load();
        br[i] = ratchet[i].load();
    }
    extra.setProperty ("pattern", mbVel.toBase64Encoding(), nullptr);
    extra.setProperty ("ratchet", mbRat.toBase64Encoding(), nullptr);
    extra.setProperty ("accent", accentIndex.load(), nullptr);
    extra.setProperty ("sens", sensitivity.load(), nullptr);
    state.appendChild (extra, nullptr);

    juce::MemoryOutputStream mos (dest, false);
    state.writeToStream (mos);
}

void DrumkitProcessor::setStateInformation (const void* data, int size)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) size);
    if (! tree.isValid() || ! tree.hasType (apvts.state.getType()))
        return;

    apvts.replaceState (tree);

    auto extra = tree.getChildWithName ("EXTRA");
    if (extra.isValid())
    {
        const size_t n = (size_t) (dkc::numPads * dkc::numSteps);
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (extra.getProperty ("pattern").toString()) && mb.getSize() == n)
        {
            auto* bytes = static_cast<const juce::uint8*> (mb.getData());
            for (size_t i = 0; i < n; ++i) pattern[i].store (bytes[i]);
        }
        juce::MemoryBlock mr;
        if (mr.fromBase64Encoding (extra.getProperty ("ratchet").toString()) && mr.getSize() == n)
        {
            auto* bytes = static_cast<const juce::uint8*> (mr.getData());
            for (size_t i = 0; i < n; ++i) ratchet[i].store (bytes[i] == 0 ? 1 : bytes[i]);
        }
        accentIndex.store ((int) extra.getProperty ("accent", 0));
        sensitivity.store ((float) (double) extra.getProperty ("sens", 0.5));
    }

    auto samples = tree.getChildWithName ("SAMPLES");
    for (int p = 0; p < dkc::numPads; ++p)
    {
        const auto path = samples.getProperty ("pad" + juce::String (p)).toString();
        if (path.isNotEmpty() && loadSample (p, juce::File (path)))
        {
            const juce::SpinLock::ScopedLockType sl (slotLock);
            slots[(size_t) p].family = (int) samples.getProperty ("fam" + juce::String (p), -1);
        }
    }

    // loadSample reinicia start/end: restaurar los valores guardados del proyecto
    apvts.replaceState (tree);
}

//==============================================================================
dkc::PitchResult DrumkitProcessor::analysePitch (int pad) const
{
    if (pad < 0 || pad >= dkc::numPads) return {};

    const juce::AudioBuffer<float>* data = nullptr;
    double rate = 44100.0;
    {
        const juce::SpinLock::ScopedLockType sl (slotLock);
        data = slots[(size_t) pad].data;
        rate = slots[(size_t) pad].rate;
    }
    if (data == nullptr) return {};
    return dkc::detectPitch (*data, rate);
}

dkc::PitchResult DrumkitProcessor::tunePad (int pad, float targetMidi, bool snapToNearest)
{
    auto r = analysePitch (pad);
    if (! r.valid) return r;

    const float target = snapToNearest ? std::round (r.midiNote) : targetMidi;
    setParam (dkc::pid (pad, "tune"), juce::jlimit (-24.0f, 24.0f, target - r.midiNote));
    return r;
}

void DrumkitProcessor::setParam (const juce::String& id, float plainValue)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
        prm->endChangeGesture();
    }
}

//==============================================================================
// PATRON
//==============================================================================
int DrumkitProcessor::getStepVel (int pad, int step) const
{
    if (pad < 0 || pad >= dkc::numPads || step < 0 || step >= dkc::numSteps) return 0;
    return pattern[pad * dkc::numSteps + step].load();
}

void DrumkitProcessor::setStepVel (int pad, int step, int vel)
{
    if (pad < 0 || pad >= dkc::numPads || step < 0 || step >= dkc::numSteps) return;
    pattern[pad * dkc::numSteps + step].store ((juce::uint8) juce::jlimit (0, 127, vel));
}

int DrumkitProcessor::getStepRatchet (int pad, int step) const
{
    if (pad < 0 || pad >= dkc::numPads || step < 0 || step >= dkc::numSteps) return 1;
    return juce::jlimit (1, 4, (int) ratchet[pad * dkc::numSteps + step].load());
}

void DrumkitProcessor::setStepRatchet (int pad, int step, int r)
{
    if (pad < 0 || pad >= dkc::numPads || step < 0 || step >= dkc::numSteps) return;
    ratchet[pad * dkc::numSteps + step].store ((juce::uint8) juce::jlimit (1, 4, r));
}

void DrumkitProcessor::clearPattern()
{
    for (auto& v : pattern) v.store (0);
    for (auto& v : ratchet) v.store (1);
}

void DrumkitProcessor::generatePattern (int style, float intensity, bool hatsOnly)
{
    // Busca en el kit cargado el pad de cada familia (si no hay, usa los pads por defecto)
    dkc::GenPads gp;
    {
        const juce::SpinLock::ScopedLockType sl (slotLock);
        auto find = [&] (int family, int& target)
        {
            for (int p = 0; p < dkc::numPads; ++p)
                if (slots[(size_t) p].data != nullptr && slots[(size_t) p].family == family) { target = p; return; }
        };
        find (dkc::Kick, gp.kick);
        find (dkc::Snare, gp.snare);
        find (dkc::Clap, gp.clap);
        find (dkc::Hat, gp.hat);
        find (dkc::OpenHat, gp.openHat);
    }

    dkc::PatternData pd;
    for (size_t i = 0; i < pd.vel.size(); ++i)
    {
        pd.vel[i] = pattern[i].load();
        pd.ratchet[i] = ratchet[i].load();
    }

    const unsigned seed = (unsigned) juce::Random::getSystemRandom().nextInt();
    if (hatsOnly)
        dkc::generateHats (pd, gp, style, intensity, seed);
    else
    {
        dkc::generateFull (pd, gp, style, intensity, seed);
        setParam ("bpm", (float) dkc::styleBpm (style));
    }

    for (size_t i = 0; i < pd.vel.size(); ++i)
    {
        pattern[i].store (pd.vel[i]);
        ratchet[i].store (pd.ratchet[i]);
    }
}

// Notas del patron ya expandidas: rolls, swing y (opcional) humanizacion
std::vector<dkc::NoteEvent> DrumkitProcessor::buildEvents (bool humanize, unsigned seed) const
{
    std::vector<dkc::NoteEvent> ev;
    std::mt19937 gen (seed);
    std::uniform_real_distribution<float> uni (-1.0f, 1.0f);

    const double bpm = juce::jlimit (40.0, 240.0, (double) bpmParam->load());
    const double stepSec = 60.0 / bpm / 4.0;
    const float swing = swingParam->load();
    const double jSteps = humanize ? (double) humTimeParam->load() * 0.025 / stepSec : 0.0;
    const float velRange = humanize ? humVelParam->load() * 30.0f : 0.0f;

    for (int s = 0; s < dkc::numSteps; ++s)
        for (int p = 0; p < dkc::numPads; ++p)
        {
            const int v = pattern[p * dkc::numSteps + s].load();
            if (v <= 0) continue;
            const int r = juce::jlimit (1, 4, (int) ratchet[p * dkc::numSteps + s].load());

            for (int j = 0; j < r; ++j)
            {
                dkc::NoteEvent e;
                e.pad = p;
                e.step = (double) s + (double) j / (double) r;
                if (s % 2 == 1) e.step += (double) swing * 0.33;
                e.step += (double) uni (gen) * jSteps;
                e.len = 0.8 / (double) r;

                const int vel = (int) ((float) v * (1.0f - 0.10f * (float) j) + uni (gen) * velRange);
                e.vel = juce::jlimit (1, 127, vel);
                ev.push_back (e);
            }
        }

    std::sort (ev.begin(), ev.end(), [] (const dkc::NoteEvent& a, const dkc::NoteEvent& b) { return a.step < b.step; });
    return ev;
}

// MIDI: lleva el nombre de cada instrumento (texto en el archivo) y respeta swing y humanizacion
bool DrumkitProcessor::writeMidiFile (const juce::File& file) const
{
    constexpr int tpq = 480;
    constexpr int stepTicks = tpq / 4;

    const double bpm = juce::jlimit (40.0, 240.0, (double) bpmParam->load());

    juce::MidiMessageSequence tempoTrack, notes;
    tempoTrack.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / bpm)));
    tempoTrack.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4));
    notes.addEvent (juce::MidiMessage::textMetaEvent (3, "Drumkit Cloner pattern"));

    const auto events = buildEvents (true, 20241125u);

    std::set<int> used;
    for (const auto& e : events) used.insert (e.pad);
    for (int p : used)   // "C1 = KICK": asi sabes que instrumento es cada nota
        notes.addEvent (juce::MidiMessage::textMetaEvent (
            1, juce::MidiMessage::getMidiNoteName (dkc::firstMidiNote + p, true, true, 3) + " = " + padLabel (p)));

    for (const auto& e : events)
    {
        const int note = dkc::firstMidiNote + e.pad;
        const int tick = juce::jmax (0, (int) std::lround (e.step * stepTicks));
        const int len = juce::jmax (12, (int) std::lround (e.len * stepTicks));

        auto on = juce::MidiMessage::noteOn (1, note, (juce::uint8) e.vel);
        on.setTimeStamp ((double) tick);
        notes.addEvent (on);

        auto off = juce::MidiMessage::noteOff (1, note);
        off.setTimeStamp ((double) (tick + len));
        notes.addEvent (off);
    }
    notes.sort();
    notes.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (tpq);
    mf.addTrack (tempoTrack);
    mf.addTrack (notes);

    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk()) return false;
    return mf.writeTo (out);
}

// WAV: renderiza el loop con todos los ajustes de cada pad. La cola se pliega al principio: el loop no corta.
bool DrumkitProcessor::renderPatternToWav (const juce::File& file)
{
    const double sr = hostRate > 0.0 ? hostRate : 44100.0;
    const double bpm = juce::jlimit (40.0, 240.0, (double) bpmParam->load());
    const double stepLen = 60.0 / bpm / 4.0 * sr;
    const int total = (int) std::lround (stepLen * dkc::numSteps);
    const int tail = (int) (2.0 * sr);
    if (total < 64) return false;

    juce::AudioBuffer<float> buf (2, total + tail);
    buf.clear();
    float* L = buf.getWritePointer (0);
    float* R = buf.getWritePointer (1);

    auto eng = std::make_unique<Engine>();
    updateSnapshots (*eng, sr);

    auto events = buildEvents (true, 20241125u);
    struct Timed { int pos; int pad; int vel; };
    std::vector<Timed> timed;
    for (const auto& e : events)
    {
        double t = e.step * stepLen;
        if (t < 0.0) t += (double) total;          // golpes adelantados por la humanizacion: al final del loop
        if (t >= (double) total) t -= (double) total;
        timed.push_back ({ juce::jlimit (0, total - 1, (int) t), e.pad, e.vel });
    }
    std::sort (timed.begin(), timed.end(), [] (const Timed& a, const Timed& b) { return a.pos < b.pos; });

    int pos = 0;
    for (const auto& t : timed)
    {
        renderEngine (*eng, sr, L, R, nullptr, nullptr, pos, t.pos);
        pos = t.pos;
        triggerVoice (*eng, t.pad, (float) t.vel / 127.0f, true);
    }
    renderEngine (*eng, sr, L, R, nullptr, nullptr, pos, total + tail);

    juce::AudioBuffer<float> out (2, total);
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* src = buf.getReadPointer (ch);
        float* dst = out.getWritePointer (ch);
        for (int i = 0; i < total; ++i) dst[i] = src[i];
        for (int i = 0; i < tail; ++i) dst[i % total] += src[total + i];
    }

    const float peak = juce::jmax (out.getMagnitude (0, 0, total), out.getMagnitude (1, 0, total));
    if (peak > 0.98f)
        out.applyGain (0.95f / peak);

    return writeWav (file, out, sr);
}

//==============================================================================
bool DrumkitProcessor::cloneFromFile (const juce::File& file)
{
    if (isCloning() || ! file.existsAsFile()) return false;
    startJob (file, nullptr, 44100.0, file.getFileNameWithoutExtension());
    return true;
}

bool DrumkitProcessor::recloneSource()
{
    if (isCloning() || sourceBuffer == nullptr) return false;
    startJob ({}, sourceBuffer, sourceRate, sourceName);
    return true;
}

void DrumkitProcessor::startJob (const juce::File& file, std::shared_ptr<juce::AudioBuffer<float>> existing,
                                 double rate, const juce::String& name)
{
    cloneStatus = {};
    cloneProgress.store (0.0f);
    job = std::make_unique<CloneJob> (*this, file, std::move (existing), rate, name, sensitivity.load());
    job->startThread();
    sendChangeMessage();
}

void DrumkitProcessor::finishJob (std::shared_ptr<juce::AudioBuffer<float>> buf, double sr, const juce::String& name,
                                  std::shared_ptr<const dkc::AnalysisResult> result, bool ok)
{
    cloneProgress.store (-1.0f);
    job.reset();

    if (! ok || buf == nullptr || result == nullptr)
    {
        cloneStatus = "No se pudo leer el archivo de audio";
        sendChangeMessage();
        return;
    }

    sourceBuffer = buf;
    sourceRate = sr;
    sourceName = name;
    analysis = result;

    if (result->sounds.empty())
    {
        cloneStatus = "No se detectaron golpes claros: sube la sensibilidad";
    }
    else
    {
        installClone (*result);
        cloneStatus = {};
    }
    sendChangeMessage();
}

void DrumkitProcessor::installClone (const dkc::AnalysisResult& res)
{
    // Los one-shots se guardan en Musica/Drumkit Cloner/Kits/<cancion>/ para que el proyecto los recuerde
    const auto folder = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                            .getChildFile ("Drumkit Cloner").getChildFile ("Kits")
                            .getChildFile (juce::File::createLegalFileName (res.name.isEmpty() ? juce::String ("Kit") : res.name));
    folder.createDirectory();

    {
        const juce::SpinLock::ScopedLockType sl (slotLock);
        for (auto& s : slots) s = Slot();
    }

    for (const auto& snd : res.sounds)
    {
        const int pad = snd.pad;
        const auto base = juce::File::createLegalFileName (snd.name);
        const auto cleanFile = folder.getChildFile (base + ".wav");
        const auto rawFile = folder.getChildFile (base + "_raw.wav");
        const bool saved = writeWav (cleanFile, snd.clean, res.sampleRate);
        writeWav (rawFile, snd.raw, res.sampleRate);

        auto cleanBuf = std::make_shared<juce::AudioBuffer<float>> (snd.clean);
        auto rawBuf = std::make_shared<juce::AudioBuffer<float>> (snd.raw);
        {
            const juce::SpinLock::ScopedLockType sl (slotLock);
            keepAlive.push_back (cleanBuf);
            keepAlive.push_back (rawBuf);
            auto& s = slots[(size_t) pad];
            s.data = cleanBuf.get();
            s.alt = rawBuf.get();
            s.rate = res.sampleRate;
            s.family = snd.family;
            s.name = snd.name;
            s.path = saved ? cleanFile.getFullPathName() : juce::String();
        }

        setParam (dkc::pid (pad, "tune"), 0.0f);
        setParam (dkc::pid (pad, "clean"), 1.0f);
        setParam (dkc::pid (pad, "comp"), 0.0f);
        setParam (dkc::pid (pad, "start"), 0.0f);
        setParam (dkc::pid (pad, "end"), 1.0f);
        setParam (dkc::pid (pad, "mute"), 0.0f);
        setParam (dkc::pid (pad, "solo"), 0.0f);
        setParam (dkc::pid (pad, "out"), (float) (snd.family + 1));                       // salida separada por familia
        setParam (dkc::pid (pad, "choke"), (snd.family == dkc::Hat || snd.family == dkc::OpenHat) ? 1.0f : 0.0f);
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumkitProcessor();
}
