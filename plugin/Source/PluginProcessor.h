#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include "hic/Engine.h"
#include "Params.h"
#include "EngineBridge.h"

class HicProcessor : public juce::AudioProcessor {
public:
    HicProcessor();
    ~HicProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // For the editor.
    juce::AudioProcessorValueTreeState& params() { return apvts; }
    hicplug::EngineBridge& bridge() { return *bridgeImpl; }
    void auditionPad(int pad, float vel) { auditionPad_.store(pad); auditionVel_.store(vel); }
    int currentStep(int track) const { return stepNow[track].load(); }
    bool transportPlaying() const { return playing.load(); }
    const hic::KitParams& defaultKit() const { return defaultKit_; }

private:
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<hicplug::EngineBridge> bridgeImpl;
    std::unique_ptr<hic::Engine> engine;
    hic::KitParams defaultKit_;
    hic::NoteEvent events[hic::kMaxEvents];
    std::atomic<int> auditionPad_ { -1 };
    std::atomic<float> auditionVel_ { 0.8f };
    std::atomic<int> stepNow[hic::kNumPads];
    std::atomic<bool> playing { false };
    int lastLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HicProcessor)
};
