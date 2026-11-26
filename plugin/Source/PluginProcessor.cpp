#include "PluginProcessor.h"
#include "PluginEditor.h"

MusicBox2Processor::MusicBox2Processor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          // Optional audio input, analysed by the spectrogram when its source is "sidechain".
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false))
{
}

void MusicBox2Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    spectrumEngine.prepare (sampleRate, samplesPerBlock);
    heldNotes[0].store (0);
    heldNotes[1].store (0);
}

bool MusicBox2Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto monoOrStereo = [] (const juce::AudioChannelSet& set)
    {
        return set == juce::AudioChannelSet::stereo() || set == juce::AudioChannelSet::mono();
    };

    const auto in = layouts.getMainInputChannelSet();
    return monoOrStereo (layouts.getMainOutputChannelSet()) && (in.isDisabled() || monoOrStereo (in));
}

void MusicBox2Processor::setNote (int note, bool on) noexcept
{
    if (note < 0 || note > 127)
        return;

    auto& word = heldNotes[static_cast<std::size_t> (note / 64)];
    const auto bit = std::uint64_t { 1 } << (note % 64);

    if (on)
        word.fetch_or (bit, std::memory_order_relaxed);
    else
        word.fetch_and (~bit, std::memory_order_relaxed);
}

void MusicBox2Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn())
            setNote (msg.getNoteNumber(), true);
        else if (msg.isNoteOff())
            setNote (msg.getNoteNumber(), false);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            heldNotes[0].store (0, std::memory_order_relaxed);
            heldNotes[1].store (0, std::memory_order_relaxed);
        }
    }

    // Silent output, unless the spectrogram's internal synth is made audible.
    // Reads the sidechain input (if enabled) before overwriting the buffer.
    spectrumEngine.process (buffer, getTotalNumInputChannels(), midi);

    // MIDI is passed through unchanged for now (it stays in `midi`).
    // Notes switched on by hand in the widgets are added to the output.
    uiNoteFifo.read (uiNoteFifo.getNumReady()).forEach ([&] (int index)
    {
        const int code = uiNotes[static_cast<std::size_t> (index)];
        if (code >= 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, code, static_cast<juce::uint8> (100)), 0);
        else
            midi.addEvent (juce::MidiMessage::noteOff (1, -code - 1), 0);
    });
}

void MusicBox2Processor::queueUiNote (int note, bool on) noexcept
{
    if (note < 0 || note > 127)
        return;

    // If the queue is full (audio not running) the event is dropped.
    uiNoteFifo.write (1).forEach ([&] (int index)
    {
        uiNotes[static_cast<std::size_t> (index)] = static_cast<std::int16_t> (on ? note : -note - 1);
    });
}

void MusicBox2Processor::applyPendingState()
{
    juce::ValueTree app;
    {
        const juce::SpinLock::ScopedLockType lock (pendingStateLock);
        std::swap (app, pendingState);
    }
    if (app.isValid())
        appState.restoreFromValueTree (app);
}

juce::AudioProcessorEditor* MusicBox2Processor::createEditor()
{
    applyPendingState();
    return new MusicBox2Editor (*this);
}

void MusicBox2Processor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("MusicBox2State");
    state.setProperty ("version", JucePlugin_VersionString, nullptr);

    juce::ValueTree editor ("Editor");
    editor.setProperty ("tab", lastTabIndex, nullptr);
    editor.setProperty ("width", lastEditorWidth, nullptr);
    editor.setProperty ("height", lastEditorHeight, nullptr);
    editor.setProperty ("sound", static_cast<int> (soundChoice.getValue()), nullptr);
    state.appendChild (editor, nullptr);

    juce::ValueTree spectrum ("Spectrum");
    spectrum.setProperty ("source", static_cast<int> (spectrumEngine.source.load()), nullptr);
    spectrum.setProperty ("synthAudible", spectrumEngine.synthAudible.load(), nullptr);
    spectrum.setProperty ("numPartials", spectrumEngine.getSynth().settings().numPartials.load(), nullptr);
    state.appendChild (spectrum, nullptr);

    // AppState is a message-thread object; hosts call this from the message thread in practice.
    state.appendChild (appState.toValueTree(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MusicBox2Processor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType ("MusicBox2State"))
        return;

    if (const auto editor = state.getChildWithName ("Editor"); editor.isValid())
    {
        lastTabIndex = editor.getProperty ("tab", 0);
        lastEditorWidth = editor.getProperty ("width", 900);
        lastEditorHeight = editor.getProperty ("height", 640);
        const int sound = editor.getProperty ("sound", 1);
        if (juce::MessageManager::existsAndIsCurrentThread())
            soundChoice = juce::jlimit (1, 3, sound);
    }

    if (const auto spectrum = state.getChildWithName ("Spectrum"); spectrum.isValid())
    {
        using mbox::spectrum::Source;
        spectrumEngine.source.store (static_cast<int> (spectrum.getProperty ("source", 0)) == 1 ? Source::sidechain : Source::internalSynth);
        spectrumEngine.synthAudible.store (static_cast<bool> (spectrum.getProperty ("synthAudible", false)));
        spectrumEngine.getSynth().settings().numPartials.store (
            juce::jlimit (1, mbox::spectrum::AdditiveSynth::kMaxPartials, static_cast<int> (spectrum.getProperty ("numPartials", 12))));
    }

    const auto app = state.getChildWithName ("AppState");
    if (juce::MessageManager::existsAndIsCurrentThread())
    {
        appState.restoreFromValueTree (app);
    }
    else
    {
        // Rare: some hosts restore state off the message thread. Keep it for
        // the message thread (applied by the editor / applyPendingState()).
        const juce::SpinLock::ScopedLockType lock (pendingStateLock);
        pendingState = app.createCopy();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MusicBox2Processor();
}
