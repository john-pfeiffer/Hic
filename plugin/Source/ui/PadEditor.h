#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ParamPanel.h"
#include "../Params.h"
#include "hic/Pad.h"

namespace hicplug {

/// Editor for one pad: type and preset, eight macros whose labels follow the
/// type, and the common controls.
class PadEditor : public juce::Component, private juce::AudioProcessorValueTreeState::Listener {
public:
    explicit PadEditor(juce::AudioProcessorValueTreeState& s) : state(s), macros(s), common(s) {
        addAndMakeVisible(typeBox); addAndMakeVisible(presetBox); addAndMakeVisible(title);
        addAndMakeVisible(macros); addAndMakeVisible(common);
        title.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        presetBox.onChange = [this] {
            if (auto* p = state.getParameter(id::pad(pad, id::Preset)))
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(presetBox.getSelectedId() - 1)));
        };
        macros.setCellSize(70, 84);
        common.setCellSize(70, 84);
        setPad(0);
    }
    ~PadEditor() override { unlisten(); }

    void setPad(int newPad) {
        unlisten();
        pad = newPad;
        title.setText(juce::String(hic::defaultPadName(pad)), juce::dontSendNotification);
        typeAtt.reset();
        typeBox.clear(juce::dontSendNotification);
        typeBox.addItemList(padTypeNames(), 1);
        typeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, id::pad(pad, id::Type), typeBox);
        std::vector<ParamPanel::Item> commonItems = {
            { id::pad(pad, id::Level), "Level" }, { id::pad(pad, id::Pan), "Pan" }, { id::pad(pad, id::Lowpass), "Lowpass" },
            { id::pad(pad, id::Drive), "Drive" }, { id::pad(pad, id::Send), "Reverb" }, { id::pad(pad, id::TailCut), "Tail Cut" },
            { id::pad(pad, id::Morph), "Morph" }, { id::pad(pad, id::ScatterMul), "Scatter" }, { id::pad(pad, id::Choke), "Choke" }, { id::pad(pad, id::Poly), "Poly" },
            { id::pad(pad, id::Reverse), "Reverse" }, { id::pad(pad, id::FreezeSrc), "Freeze Src" }, { id::pad(pad, id::DuckSrc), "Duck Src" },
        };
        common.rebuild(commonItems);
        state.addParameterListener(id::pad(pad, id::Type), this);
        state.addParameterListener(id::pad(pad, id::Preset), this);
        listening = true;
        refreshForType();
    }

    void resized() override {
        auto r = getLocalBounds().reduced(4);
        auto top = r.removeFromTop(24);
        title.setBounds(top.removeFromLeft(110));
        typeBox.setBounds(top.removeFromLeft(100).reduced(2, 0));
        presetBox.setBounds(top.removeFromLeft(150).reduced(2, 0));
        macros.setBounds(r.removeFromLeft(8 * 70 + 8));
        common.setBounds(r);
    }

private:
    void unlisten() {
        if (!listening) return;
        state.removeParameterListener(id::pad(pad, id::Type), this);
        state.removeParameterListener(id::pad(pad, id::Preset), this);
        listening = false;
    }

    void parameterChanged(const juce::String&, float) override {
        juce::MessageManager::callAsync([sp = juce::Component::SafePointer<PadEditor>(this)] { if (sp) sp->refreshForType(); });
    }

    void refreshForType() {
        const auto type = static_cast<hic::PadType>(juce::jlimit(0, static_cast<int>(hic::PadType::Count) - 1, static_cast<int>(state.getRawParameterValue(id::pad(pad, id::Type))->load() + 0.5f)));
        std::vector<ParamPanel::Item> items;
        for (int m = 0; m < hic::kNumMacros; ++m) items.push_back({ id::pad(pad, static_cast<id::PadParam>(id::M0 + m)), hic::macroName(type, m) });
        macros.rebuild(items);
        presetBox.clear(juce::dontSendNotification);
        presetBox.addItemList(presetNames(type), 1);
        const int preset = static_cast<int>(state.getRawParameterValue(id::pad(pad, id::Preset))->load() + 0.5f);
        presetBox.setSelectedId(juce::jlimit(1, presetBox.getNumItems(), preset + 1), juce::dontSendNotification);
    }

    juce::AudioProcessorValueTreeState& state;
    int pad = 0;
    bool listening = false;
    juce::Label title;
    juce::ComboBox typeBox, presetBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt;
    ParamPanel macros, common;
};

} // namespace hicplug
