#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

class PrismProcessor;

//==============================================================================
// 频谱可视化：对数频率轴（20 Hz – 20 kHz），峰值平滑下落。
//
// 注意：**按 FFT bin 取点**，横坐标再按对数频率映射；不能预先按对数频率均分点，
// 否则低频段多个显示点会落到同一个 bin 上，数值一模一样 → 一条"平顶矩形"。
//==============================================================================
class SpectrumDisplay final : public juce::Component
{
public:
    explicit SpectrumDisplay (PrismProcessor& processorToUse);

    void paint (juce::Graphics&) override;
    void update();          // 由编辑器的定时器调用

private:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;      // 2048
    static constexpr int numBins  = fftSize / 2;        // 1024

    PrismProcessor& processor;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };

    std::array<float, fftSize * 2> fftData {};
    std::array<float, numBins> binLevel {};   // 0..1，已做时间平滑
    std::array<float, numBins> binPeak {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};
