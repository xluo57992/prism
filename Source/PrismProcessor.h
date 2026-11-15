#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DispersionEngine.h"

//==============================================================================
// 棱镜 / Prism —— 全通相位色散效果器
//
// 参数只有五个：频率 / Q / 强度 / 混音 / 输出增益。
// 「强度」是常驻的大旋钮，同时驱动背景照片的模糊程度。
//==============================================================================
class PrismProcessor : public juce::AudioProcessor,
                       private juce::AudioProcessorValueTreeState::Listener
{
public:
    PrismProcessor();
    ~PrismProcessor() override;

    // 调试用：任何参数被写都会记一行日志（需 PRISM_DEBUG=1）
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    //--------------------------------------------------------------------------
    // 参数 ID
    static constexpr const char* pFrequency = "freq";
    static constexpr const char* pQ         = "q";
    static constexpr const char* pIntensity = "intensity";
    static constexpr const char* pMix       = "mix";
    static constexpr const char* pOutputDb  = "output_db";
    static constexpr const char* pEffect    = "effect";      // 效果总开关（false = 旁通）

    static const std::vector<juce::String>& getAllParamIds();

    bool isEffectOn() const;

    //--------------------------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return juce::String::fromUTF8 ("棱镜 · 色散"); }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    //--------------------------------------------------------------------------
    // 不暴露 program：REAPER 一开 FX 窗口就会回调 setCurrentProgram，容易把参数打回。
    //--------------------------------------------------------------------------
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }

    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //--------------------------------------------------------------------------
    juce::AudioProcessorValueTreeState parameters;

    // 给 GUI 用：色散强度（0..1），驱动背景光谱的张开程度
    std::atomic<float> intensityLevel { 0.35f };

    // 给 GUI 用：输出电平（0..1，已做快起慢落），驱动背景的光强度
    std::atomic<float> meterLevel { 0.0f };

    //--------------------------------------------------------------------------
    // 频谱可视化用：输出信号的单声道混合，写进环形缓冲，界面线程来取
    //--------------------------------------------------------------------------
    int readSpectrum (float* dest, int maxSamples);
    double getCurrentSampleRate() const noexcept { return currentSampleRate.load(); }

    juce::AbstractFifo spectrumFifo { 16384 };
    std::array<float, 16384> spectrumData {};

private:
    void pushParamsToEngine();

    DispersionEngine engine;

    std::atomic<double> currentSampleRate { 48000.0 };

    juce::UndoManager undoManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismProcessor)
};
