#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "SpectrumDisplay.h"
#include "DispersionCurve.h"

class PrismProcessor;

//==============================================================================
// 频率视图 —— 单独开一个窗口，看「它到底处理了哪些频率」。
//
// 全通链的幅度谱是平的，所以画频谱看不出任何东西；真正能看见的是**群延迟**：
// 中心频率附近被拖慢最多，两边迅速回落。曲线上方垫一层实时输出频谱，
// 是为了把"被拖慢的频段"和"音乐里实际有多少能量"对上号。
//
// 布局：标题栏 → 曲线区（对数频率轴 20 Hz–20 kHz）→ 底部说明。
//==============================================================================
class FrequencyViewComponent final : public juce::Component, private juce::Timer
{
public:
    explicit FrequencyViewComponent (PrismProcessor& processorToUse);
    ~FrequencyViewComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    //--- 给曲线层 / 测试用的只读访问 ------------------------------------------
    const dispersion::Curve& getCurve() const noexcept { return curve; }
    float getDisplayPeakMs() const noexcept;
    int getStages() const noexcept { return stages; }
    bool isEffectOn() const noexcept { return effectOn; }
    float getCentreHz() const noexcept { return centreHz; }

    // 曲线层坐标系（组件的 plot 矩形内部）下的映射
    static float logPosition (double hz);
    static double hzAtPosition (double pos);

private:
    class CurveLayer;

    void timerCallback() override;
    void updateFromParams (bool force);
    juce::Rectangle<float> plotArea() const;
    juce::String headerInfo() const;

    static float rawParam (PrismProcessor& p, const char* id, float fallback);

    PrismProcessor& processor;
    SpectrumDisplay spectrum;
    std::unique_ptr<CurveLayer> curveLayer;

    // 置顶开关：不置顶时点回宿主，窗口会被 FX 窗口盖住
    juce::TextButton pinButton;

    dispersion::Curve curve;
    double sampleRate = 48000.0;

    float centreHz = 1000.0f;
    float qValue   = 0.50f;
    float mixValue = 1.00f;
    int   stages   = 0;
    bool  effectOn = true;

    // 纵轴峰值做平滑：扫 Q 的时候坐标轴不会一格一格跳
    float smoothedPeakMs = 1.0f;

    // 参数签名：只有真的变了才重算曲线（720 点 × 复数运算是笔开销）
    double lastSignature = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrequencyViewComponent)
};

//==============================================================================
// 独立窗口。点右上角的 ✕ 只是隐藏，不销毁 —— 编辑器关掉时才连带销毁。
//==============================================================================
class FrequencyViewWindow final : public juce::DocumentWindow
{
public:
    explicit FrequencyViewWindow (PrismProcessor& processorToUse);

    void closeButtonPressed() override { setVisible (false); }

    FrequencyViewComponent& view() noexcept { return *viewComponent; }

private:
    FrequencyViewComponent* viewComponent = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrequencyViewWindow)
};
