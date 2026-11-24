#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

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
        addFloat (dkc::pid (p, "clean"),  n + "Clean",  0.0f,   1.0f,  0.0f);   // se usa en la Fase 6

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
    bpmParam = apvts.getRawParameterValue ("bpm");

    for (int p = 0; p < dkc::numPads; ++p)
    {
        auto get = [this, p] (const char* id) { return apvts.getRawParameterValue (dkc::pid (p, id)); };
        auto& pp = params[(size_t) p];
        pp.gain = get ("gain");     pp.pan = get ("pan");     pp.tune = get ("tune");
        pp.attack = get ("attack"); pp.fade = get ("fade");   pp.hp = get ("hp");
        pp.lp = get ("lp");         pp.clean = get ("clean"); pp.choke = get ("choke");
        pp.mute = get ("mute");     pp.solo = get ("solo");   pp.out = get ("out");
    }
}

DrumkitProcessor::~DrumkitProcessor() {}

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
    stopAll();
}

juce::AudioProcessorEditor* DrumkitProcessor::createEditor()
{
    return new DrumkitEditor (*this);
}

//==============================================================================
bool DrumkitProcessor::loadSample (int pad, const juce::File& file)
{
    if (pad < 0 || pad >= dkc::numPads || ! file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    const juce::int64 maxLen = (juce::int64) (reader->sampleRate * 30.0);   // maximo 30 s por pad
    const int numSamples = (int) juce::jmin (reader->lengthInSamples, maxLen);
    const int numChannels = (int) juce::jmin (2u, reader->numChannels);

    auto buf = std::make_shared<juce::AudioBuffer<float>> (numChannels, numSamples);
    reader->read (buf.get(), 0, numSamples, 0, true, true);

    {
        const juce::SpinLock::ScopedLockType sl (slotLock);
        keepAlive.push_back (buf);
        auto& s = slots[(size_t) pad];
        s.data = buf.get();
        s.rate = reader->sampleRate;
        s.name = file.getFileNameWithoutExtension();
        s.path = file.getFullPathName();
    }

    sendChangeMessage();
    return true;
}

const juce::AudioBuffer<float>* DrumkitProcessor::getSampleData (int pad) const
{
    if (pad < 0 || pad >= dkc::numPads) return nullptr;
    const juce::SpinLock::ScopedLockType sl (slotLock);
    return slots[(size_t) pad].data;
}

juce::String DrumkitProcessor::getPadName (int pad) const
{
    if (pad < 0 || pad >= dkc::numPads) return {};
    const juce::SpinLock::ScopedLockType sl (slotLock);
    return slots[(size_t) pad].name;
}

//==============================================================================
void DrumkitProcessor::updateSnapshots()
{
    anySolo = false;

    for (int p = 0; p < dkc::numPads; ++p)
    {
        auto& pp = params[(size_t) p];
        auto& s = snaps[(size_t) p];

        s.gain = juce::Decibels::decibelsToGain (pp.gain->load(), -60.0f);

        const float pan = pp.pan->load();
        s.panL = juce::jmin (1.0f, 1.0f - pan);
        s.panR = juce::jmin (1.0f, 1.0f + pan);

        s.tuneRatio = std::pow (2.0, (double) pp.tune->load() / 12.0);
        s.attackSamples = (int) (pp.attack->load() * 0.001 * hostRate);
        s.fadeSamples   = (int) (pp.fade->load()   * 0.001 * hostRate);

        s.hp = pp.hp->load();
        s.lp = pp.lp->load();
        s.useHp = s.hp > 20.5f;
        s.useLp = s.lp < 19500.0f;

        s.mute  = pp.mute->load() > 0.5f;
        s.solo  = pp.solo->load() > 0.5f;
        s.out   = (int) std::lround (pp.out->load());
        s.choke = (int) std::lround (pp.choke->load());

        anySolo = anySolo || s.solo;
    }
}

void DrumkitProcessor::trigger (int pad, float velocity)
{
    if (pad < 0 || pad >= dkc::numPads) return;

    const juce::SpinLock::ScopedTryLockType lock (slotLock);
    if (! lock.isLocked() || slots[(size_t) pad].data == nullptr) return;

    auto& v = voices[(size_t) pad];
    v.data = slots[(size_t) pad].data;
    v.srcRate = slots[(size_t) pad].rate;
    v.pos = 0.0;
    v.age = 0;
    v.vel = velocity;
    v.active = true;
    v.hp.reset();
    v.lp.reset();
    v.lastHp = v.lastLp = -1.0f;

    const int group = snaps[(size_t) pad].choke;
    if (group > 0)
        for (int j = 0; j < dkc::numPads; ++j)
            if (j != pad && snaps[(size_t) j].choke == group)
                voices[(size_t) j].active = false;
}

void DrumkitProcessor::stopAll()
{
    for (auto& v : voices) v.active = false;
}

void DrumkitProcessor::render (juce::AudioBuffer<float>& buffer, int start, int end)
{
    if (end <= start) return;

    auto mainBus = getBusBuffer (buffer, false, 0);
    const int mainCh = mainBus.getNumChannels();
    if (mainCh < 1) return;
    float* mainL = mainBus.getWritePointer (0);
    float* mainR = mainBus.getWritePointer (mainCh > 1 ? 1 : 0);

    for (int p = 0; p < dkc::numPads; ++p)
    {
        auto& v = voices[(size_t) p];
        if (! v.active || v.data == nullptr) continue;

        const auto& s = snaps[(size_t) p];
        const bool audible = ! s.mute && (! anySolo || s.solo);
        if (! audible) { v.active = false; continue; }

        // Salida separada opcional (siempre suena tambien en Main)
        float* sepL = nullptr;
        float* sepR = nullptr;
        juce::AudioBuffer<float> sepBus;
        if (s.out > 0 && s.out < getBusCount (false))
        {
            if (auto* bus = getBus (false, s.out))
            {
                if (bus->isEnabled() && bus->getNumberOfChannels() >= 2)
                {
                    sepBus = getBusBuffer (buffer, false, s.out);
                    sepL = sepBus.getWritePointer (0);
                    sepR = sepBus.getWritePointer (1);
                }
            }
        }

        if (s.useHp && v.lastHp != s.hp) { v.hp.set (true,  hostRate, s.hp); v.lastHp = s.hp; }
        if (s.useLp && v.lastLp != s.lp) { v.lp.set (false, hostRate, s.lp); v.lastLp = s.lp; }

        const float* L = v.data->getReadPointer (0);
        const float* R = v.data->getNumChannels() > 1 ? v.data->getReadPointer (1) : L;
        const int len = v.data->getNumSamples();
        const double step = v.srcRate / hostRate * s.tuneRatio;

        for (int i = start; i < end; ++i)
        {
            const int i0 = (int) v.pos;
            if (i0 >= len - 1) { v.active = false; break; }

            const float frac = (float) (v.pos - (double) i0);
            float l = L[i0] + (L[i0 + 1] - L[i0]) * frac;
            float r = R[i0] + (R[i0 + 1] - R[i0]) * frac;

            if (s.useHp) { l = v.hp.process (0, l); r = v.hp.process (1, r); }
            if (s.useLp) { l = v.lp.process (0, l); r = v.lp.process (1, r); }

            float g = v.vel * s.gain;

            if (s.attackSamples > 0 && v.age < s.attackSamples)
                g *= (float) v.age / (float) s.attackSamples;

            if (s.fadeSamples > 0)
            {
                const double remaining = ((double) (len - 1) - v.pos) / step;
                if (remaining < (double) s.fadeSamples)
                    g *= (float) (remaining / (double) s.fadeSamples);
            }

            const float oL = l * g * s.panL;
            const float oR = r * g * s.panR;

            mainL[i] += oL;
            mainR[i] += oR;
            if (sepL != nullptr) { sepL[i] += oL; sepR[i] += oR; }

            v.pos += step;
            ++v.age;
        }
    }
}

void DrumkitProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // Secuenciador interno: inyecta las notas del patron en el buffer MIDI
    const bool playing = seqPlaying.load();
    if (playing && ! wasPlaying) { seqStep = 0; nextStepIn = 0.0; }
    if (! playing && wasPlaying) currentStep.store (-1);
    wasPlaying = playing;

    if (playing)
    {
        const double bpm = juce::jlimit (40.0, 240.0, (double) bpmParam->load());
        const double stepLen = 60.0 / bpm / 4.0 * hostRate;
        double t = nextStepIn;

        while (t < (double) numSamples)
        {
            currentStep.store (seqStep);
            for (int p = 0; p < dkc::numPads; ++p)
            {
                const int vel = pattern[p * dkc::numSteps + seqStep].load();
                if (vel > 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, dkc::firstMidiNote + p, (juce::uint8) vel), (int) t);
            }
            seqStep = (seqStep + 1) % dkc::numSteps;
            t += stepLen;
        }
        nextStepIn = t - (double) numSamples;
    }

    // Mezcla los clicks de los pads de la interfaz con el MIDI del DAW
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    updateSnapshots();

    int pos = 0;
    for (const auto meta : midi)
    {
        const int evPos = juce::jlimit (pos, numSamples, meta.samplePosition);
        render (buffer, pos, evPos);
        pos = evPos;

        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            trigger (msg.getNoteNumber() - dkc::firstMidiNote, msg.getFloatVelocity());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            stopAll();
    }
    render (buffer, pos, numSamples);
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
                samples.setProperty ("pad" + juce::String (p), slots[(size_t) p].path, nullptr);
    }
    state.appendChild (samples, nullptr);

    auto oldExtra = state.getChildWithName ("EXTRA");
    if (oldExtra.isValid()) state.removeChild (oldExtra, nullptr);

    juce::ValueTree extra ("EXTRA");
    juce::MemoryBlock mb ((size_t) (dkc::numPads * dkc::numSteps), true);
    auto* bytes = static_cast<juce::uint8*> (mb.getData());
    for (int i = 0; i < dkc::numPads * dkc::numSteps; ++i)
        bytes[i] = pattern[i].load();
    extra.setProperty ("pattern", mb.toBase64Encoding(), nullptr);
    extra.setProperty ("accent", accentIndex.load(), nullptr);
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
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (extra.getProperty ("pattern").toString())
            && mb.getSize() == (size_t) (dkc::numPads * dkc::numSteps))
        {
            auto* bytes = static_cast<const juce::uint8*> (mb.getData());
            for (int i = 0; i < dkc::numPads * dkc::numSteps; ++i)
                pattern[i].store (bytes[i]);
        }
        accentIndex.store ((int) extra.getProperty ("accent", 0));
    }

    auto samples = tree.getChildWithName ("SAMPLES");
    for (int p = 0; p < dkc::numPads; ++p)
    {
        const auto path = samples.getProperty ("pad" + juce::String (p)).toString();
        if (path.isNotEmpty())
            loadSample (p, juce::File (path));
    }
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
    const float semis = juce::jlimit (-24.0f, 24.0f, target - r.midiNote);

    if (auto* prm = apvts.getParameter (dkc::pid (pad, "tune")))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (semis));
        prm->endChangeGesture();
    }
    return r;
}

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

void DrumkitProcessor::clearPattern()
{
    for (auto& v : pattern) v.store (0);
}

bool DrumkitProcessor::writeMidiFile (const juce::File& file) const
{
    constexpr int tpq = 480;
    constexpr int stepTicks = tpq / 4;
    constexpr int noteTicks = 100;

    const double bpm = juce::jlimit (40.0, 240.0, (double) bpmParam->load());

    juce::MidiMessageSequence tempoTrack, notes;
    tempoTrack.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / bpm)));
    tempoTrack.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4));

    for (int p = 0; p < dkc::numPads; ++p)
    {
        for (int s = 0; s < dkc::numSteps; ++s)
        {
            const int vel = pattern[p * dkc::numSteps + s].load();
            if (vel <= 0) continue;

            const int note = dkc::firstMidiNote + p;
            auto on = juce::MidiMessage::noteOn (1, note, (juce::uint8) vel);
            on.setTimeStamp ((double) (s * stepTicks));
            notes.addEvent (on);

            auto off = juce::MidiMessage::noteOff (1, note);
            off.setTimeStamp ((double) (s * stepTicks + noteTicks));
            notes.addEvent (off);
        }
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

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumkitProcessor();
}
