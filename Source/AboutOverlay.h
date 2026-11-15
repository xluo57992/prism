#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SpectrumDisplay.h"

class PrismProcessor;

//==============================================================================
// 关于面板：半透明悬浮窗。
//   左上角 ✕ 关闭
//   上半部分放正文（现在还是占位）
//   底部是频率频谱可视化
//==============================================================================
class AboutOverlay final : public juce::Component
{
public:
    explicit AboutOverlay (PrismProcessor& processorToUse);

    void paint (juce::Graphics&) override;
    void resized() override;

    void updateSpectrum() { spectrum.update(); }

    juce::TextButton closeButton;
    SpectrumDisplay spectrum;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutOverlay)
};
