#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ParamPanel.h"
#include "../Params.h"
#include "hic/Pad.h"
#include "hic/Kit.h"

namespace hicplug {

/// Editor for one pad: the six macros, large, plus the common controls.
class PadEditor : public juce::Component {
public:
    explicit PadEditor(juce::AudioProcessorValueTreeState& s) : state(s), macros(s), common(s) {
        addAndMakeVisible(title); addAndMakeVisible(macros); addAndMakeVisible(common);
        title.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
        macros.setCellSize(88, 104);
        common.setCellSize(58, 80);
        setPad(0);
    }

    void setPad(int newPad) {
        pad = newPad;
        title.setText(juce::String(hic::defaultPadName(pad)), juce::dontSendNotification);
        std::vector<ParamPanel::Item> items;
        for (int m = 0; m < hic::kNumMacros; ++m) items.push_back({ id::pad(pad, static_cast<id::PadParam>(id::Tune + m)), hic::macroName(m) });
        macros.rebuild(items);
        common.rebuild({
            { id::pad(pad, id::Level), "Level" }, { id::pad(pad, id::Pan), "Pan" }, { id::pad(pad, id::Send), "Reverb" },
            { id::pad(pad, id::ScatterMul), "Scatter" }, { id::pad(pad, id::Choke), "Choke" }, { id::pad(pad, id::Poly), "Poly" },
            { id::pad(pad, id::FollowKey), "Key" }, { id::pad(pad, id::Reverse), "Reverse" },
            { id::pad(pad, id::FreezeSrc), "Freeze Src" }, { id::pad(pad, id::DuckSrc), "Duck Src" },
        });
    }

    void resized() override {
        auto r = getLocalBounds().reduced(4);
        title.setBounds(r.removeFromTop(20));
        macros.setBounds(r.removeFromLeft(6 * 88 + 8));
        r.removeFromLeft(6);
        common.setBounds(r);
    }

private:
    juce::AudioProcessorValueTreeState& state;
    int pad = 0;
    juce::Label title;
    ParamPanel macros, common;
};

} // namespace hicplug
