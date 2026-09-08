#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/ParamPanel.h"
#include "ui/PadStrip.h"
#include "ui/PadEditor.h"
#include "ui/BusPanel.h"
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
    void loadKitMenu();
    void exportKit();
    void exportLoop();
    void toggleDetail();

    HicProcessor& proc;
    juce::Label title;
    juce::TextButton kitButton { "Kit" }, exportButton { "Export kit" }, loopButton { "Bounce loop" }, detailButton { "Detail" };
    std::unique_ptr<juce::FileChooser> chooser;
    hicplug::ParamPanel transport, feelDetail, staticPanel, duckPanel, repeatPanel, freezePanel, reverbPanel;
    hicplug::PadStrip pads;
    hicplug::PadEditor padEditor;
    hicplug::BusPanel bus;
    hicplug::StepGrid grid;
    int shownPattern = -1;
    bool detailVisible = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HicEditor)
};
