#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace hicplug {

/// A labelled grid of controls bound to parameters. Chooses a combo box,
/// toggle or slider from the parameter type. rebuild() rebinds it to a new
/// set of IDs (used when the selected pad changes).
class ParamPanel : public juce::Component {
public:
    struct Item { juce::String id; juce::String label; bool rotary = true; };

    ParamPanel(juce::AudioProcessorValueTreeState& s, juce::String titleText = {}) : state(s), title(std::move(titleText)) {}

    void rebuild(const std::vector<Item>& items) {
        controls.clear();
        for (const auto& it : items) {
            auto* param = state.getParameter(it.id);
            if (param == nullptr) continue;
            auto c = std::make_unique<Control>();
            c->label.setText(it.label, juce::dontSendNotification);
            c->label.setJustificationType(juce::Justification::centred);
            c->label.setFont(juce::Font(juce::FontOptions(11.0f)));
            addAndMakeVisible(c->label);
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(param)) {
                c->combo = std::make_unique<juce::ComboBox>();
                c->combo->addItemList(choice->choices, 1);
                addAndMakeVisible(*c->combo);
                c->comboAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, it.id, *c->combo);
            } else if (dynamic_cast<juce::AudioParameterBool*>(param) != nullptr) {
                c->toggle = std::make_unique<juce::ToggleButton>();
                addAndMakeVisible(*c->toggle);
                c->toggleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, it.id, *c->toggle);
            } else {
                c->slider = std::make_unique<juce::Slider>(it.rotary ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearHorizontal,
                                                           juce::Slider::TextBoxBelow);
                c->slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, juce::jmax(54, cellW - 14), 16);
                c->slider->setNumDecimalPlacesToDisplay(2);
                addAndMakeVisible(*c->slider);
                c->sliderAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, it.id, *c->slider);
            }
            controls.push_back(std::move(c));
        }
        resized();
    }

    void setCellSize(int w, int h) { cellW = w; cellH = h; resized(); }

    void paint(juce::Graphics& g) override {
        if (title.isNotEmpty()) {
            g.setColour(juce::Colours::white.withAlpha(0.55f));
            g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
            g.drawText(title, 6, 2, getWidth() - 12, 16, juce::Justification::left);
        }
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.0f);
    }

    void resized() override {
        const int top = title.isNotEmpty() ? 18 : 2;
        const int perRow = juce::jmax(1, (getWidth() - 8) / cellW);
        int i = 0;
        for (auto& c : controls) {
            const int col = i % perRow, row = i / perRow;
            juce::Rectangle<int> cell(4 + col * cellW, top + row * cellH, cellW, cellH);
            c->label.setBounds(cell.removeFromTop(14));
            if (c->slider) c->slider->setBounds(cell.reduced(2));
            if (c->combo) c->combo->setBounds(cell.removeFromTop(22).reduced(3, 0));
            if (c->toggle) c->toggle->setBounds(cell.withSizeKeepingCentre(24, 24));
            ++i;
        }
    }

private:
    struct Control {
        juce::Label label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleAtt;
    };
    juce::AudioProcessorValueTreeState& state;
    juce::String title;
    std::vector<std::unique_ptr<Control>> controls;
    int cellW = 74, cellH = 78;
};

} // namespace hicplug
