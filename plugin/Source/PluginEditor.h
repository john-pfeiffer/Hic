#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/ParamPanel.h"
#include "ui/PadStrip.h"
#include "ui/PadEditor.h"
#include "ui/StepGrid.h"

class HicEditor : public juce::AudioProcessorEditor, private juce::Timer, private juce::AudioProcessorValueTreeState::Listener {
public:
    explicit HicEditor(HicProcessor&);
    ~HicEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void parameterChanged(const juce::String& id, float value) override;
    void showPattern();

    HicProcessor& proc;
    juce::Label title;
    hicplug::ParamPanel transport, feel, bedPanel, duckPanel, repeatPanel, freezePanel, reverbPanel;
    hicplug::PadStrip pads;
    hicplug::PadEditor padEditor;
    hicplug::StepGrid grid;
    int shownPattern = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HicEditor)
};
