#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

//==============================================================================
// 棱镜 / Prism —— 全通相位色散 (All-pass Dispersion)
//
// 原理：把信号串过一长串双二阶全通滤波器。全通不改变幅度谱，只改相位：
// 每个全通在中心频率附近给出一段巨大的群延迟，几十级串起来，高频会"跑"到
// 低频前面/后面好几个毫秒 —— 听感上是瞬态被抹开、金属味的"激光 zap"、
// 贝斯上则变成一种黏稠的相位拖尾。
//
// 信号链：
//   输入 → 干湿分支 ┬ 全通链 ×N（N = 强度 × 50） ┬→ 混合 → 输出增益
//                   └ dry                       ┘
//
// 与参考实现（AllPassPhase / enummusic）的差异：
//   1. 采样率按实际值计算（参考实现把 44100 写死在系数里，48k 工程下中心频率是偏的）；
//   2. 系数按 32 采样一块、由平滑后的频率/Q 重算，扫频时不会"咔"；
//   3. 滤波器状态用 double，低频高 Q 时不掉精度。
//
//==============================================================================
class DispersionEngine
{
public:
    struct Params
    {
        float frequencyHz = 1000.0f;   // 色散中心频率
        float q           = 0.50f;     // 参考实现的 Q 旋钮：越小越"捏"，色散越强
        float intensity   = 0.35f;     // 0..1 → 0..50 级串联
        float mix         = 0.50f;     // 干湿
        float outputDb    = 0.0f;      // 输出增益
    };

    void prepare (double sampleRate);
    void reset();
    void setParams (const Params& p);
    void process (juce::AudioBuffer<float>& buffer);

    int getActiveStages() const noexcept { return activeStages; }

    static constexpr int maxStages = 50;

private:
    //--------------------------------------------------------------------------
    // 单个双二阶全通（RBJ cookbook 的 all-pass 形式）
    //--------------------------------------------------------------------------
    struct Stage
    {
        double c0 = 1.0, c1 = 0.0, c2 = 0.0, c3 = 0.0, c4 = 0.0;
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

        inline float process (float input) noexcept
        {
            const double x = (double) input;

            const double y = c0 * x + c1 * x1 + c2 * x2 - c3 * y1 - c4 * y2;

            x2 = x1; x1 = x;
            y2 = y1; y1 = y;

            return (float) y;
        }

        void reset() { x1 = x2 = y1 = y2 = 0.0; }

        void setCoeffs (double n0, double n1, double n2, double n3, double n4)
        {
            c0 = n0; c1 = n1; c2 = n2; c3 = n3; c4 = n4;
        }
    };

    void updateCoefficients();
    void recomputeStageCoefficients (float freqHz, float qKnob);

    double sampleRate = 48000.0;
    Params params;

    std::array<Stage, maxStages> stagesL;
    std::array<Stage, maxStages> stagesR;

    int activeStages = 0;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> freqSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> qSmooth;

    // 暂存一份算好的系数，复制到所有级
    double c0 = 1.0, c1 = 0.0, c2 = 0.0, c3 = 0.0, c4 = 0.0;

    JUCE_LEAK_DETECTOR (DispersionEngine)
};
