#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "hic/Kit.h"

namespace hicplug {

/// Twelve pad buttons. Clicking auditions and selects a pad.
class PadStrip : public juce::Component {
public:
    std::function<void(int)> onSelect;
    std::function<void(int)> onAudition;

    PadStrip() {
        for (int i = 0; i < hic::kNumPads; ++i) {
            auto* b = buttons.add(new juce::TextButton(hic::defaultPadName(i)));
            b->setClickingTogglesState(false);
            b->setRadioGroupId(0);
            b->onClick = [this, i] { select(i); if (onAudition) onAudition(i); };
            addAndMakeVisible(b);
        }
        select(0);
    }

    void select(int pad) {
        selected = pad;
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setColour(juce::TextButton::buttonColourId, i == pad ? juce::Colour(0xff4a6a8a) : juce::Colour(0xff2b2f36));
        if (onSelect) onSelect(pad);
    }
    int selectedPad() const { return selected; }

    void resized() override {
        auto r = getLocalBounds();
        const int h = r.getHeight() / hic::kNumPads;
        for (auto* b : buttons) b->setBounds(r.removeFromTop(h).reduced(2, 1));
    }

private:
    juce::OwnedArray<juce::TextButton> buttons;
    int selected = 0;
};

} // namespace hicplug
