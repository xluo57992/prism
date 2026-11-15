#pragma once

#include "PrismProcessor.h"
#include "PrismLookAndFeel.h"
#include "AboutOverlay.h"
#include "FrequencyView.h"
#include <array>

//==============================================================================
// 棱镜编辑器（正方形，可缩放）
//
//   背景 = 程序化棱镜：白光入射 → 棱镜内折射 → 右侧散成光谱
//   常驻 = 一个大「强度」旋钮 + 底部 4 个参数旋钮（无抽屉，全部直接可见）
//
//   布局写在 720×720 的设计坐标系里，实际按窗口大小整体缩放（canvas + transform），
//   所以任何一个尺寸下控件都不会错位。
//==============================================================================
class PrismEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PrismEditor (PrismProcessor& processorToUse);
    ~PrismEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // 设计坐标系的画布：背景在这上面画，所有子控件也都挂在它下面
    void paintCanvas (juce::Graphics&);

private:
    // 把 paint 转发回编辑器的 720×720 画布
    class Canvas final : public juce::Component
    {
    public:
        explicit Canvas (PrismEditor& editorToUse) : owner (editorToUse) {}

        void paint (juce::Graphics& g) override { owner.paintCanvas (g); }
        void resized() override {}

    private:
        PrismEditor& owner;
    };

    void timerCallback() override;
    void toggleFrequencyView();

    struct Knob
    {
        KnobSlider slider;
        juce::Label nameLabel;
        juce::Label valueLabel;
        juce::String paramId;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    std::unique_ptr<Knob> createKnob (const juce::String& paramId, const juce::String& label, const juce::String& tip);
    juce::String formatValue (const juce::String& id, float value) const;
    float rawParam (const char* id, float fallback) const;
    void refreshValues();
    void drawBackground (juce::Graphics&);

    PrismProcessor& processor;
    PrismLookAndFeel lookAndFeel;
    Canvas canvas { *this };

    //--------------------------------------------------------------------------
    // 常驻的大「强度」旋钮
    //--------------------------------------------------------------------------
    KnobSlider mixSlider;
    juce::Label mixValueLabel;
    juce::Label mixStateLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    //--------------------------------------------------------------------------
    // 底部常驻的 4 个参数旋钮
    //--------------------------------------------------------------------------
    std::vector<std::unique_ptr<Knob>> drawerKnobs;

    juce::TextButton aboutButton;
    juce::TextButton visualButton;      // 效果总开关（绑定 bypass 参数，可被宿主自动化）
    juce::TextButton freqButton;        // 打开独立的「频率视图」窗口
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> effectAttachment;

    // 频率视图是独立顶层窗口：编辑器销毁时一起销毁，✕ 只是隐藏
    std::unique_ptr<FrequencyViewWindow> frequencyWindow;

    // 光效开/关的过渡量（0 = 收起，1 = 完全展开），带非线性缓动
    float visualAmount = 1.0f;
    float visualTarget = 1.0f;

    std::unique_ptr<AboutOverlay> aboutOverlay;

    float lastMaster = -1.0f;

    // 音频驱动：背景的光强度跟着输出电平走
    float meterSmoothed = 0.0f;
    float lastMeter = -1.0f;

    // 「色散度 · xx 级」只在刚调过强度时显示，之后淡出
    float stateAlpha = 0.0f;
    juce::uint32 lastStateEditMs = 0;
};
