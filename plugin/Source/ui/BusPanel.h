#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ParamPanel.h"
#include "../Params.h"

namespace hicplug {

/// The six bus knobs plus the key.
class BusPanel : public juce::Component {
public:
    explicit BusPanel(juce::AudioProcessorValueTreeState& s) : knobs(s, "Bus"), key(s, "Key") {
        knobs.setCellSize(88, 104);
        knobs.rebuild({ { id::busDrive, "Drive" }, { id::busDamp, "Damp" }, { id::busTexture, "Texture" },
                        { id::busSpace, "Space" }, { id::busDrift, "Drift" }, { id::busFeel, "Feel" } });
        key.setCellSize(110, 60);
        key.rebuild({ { id::keyRoot, "Root" }, { id::keyScale, "Scale" } });
        addAndMakeVisible(knobs); addAndMakeVisible(key);
    }
    void resized() override {
        auto r = getLocalBounds();
        knobs.setBounds(r.removeFromLeft(6 * 88 + 8));
        r.removeFromLeft(6);
        key.setBounds(r.removeFromLeft(2 * 110 + 8));
    }
private:
    ParamPanel knobs, key;
};

} // namespace hicplug
