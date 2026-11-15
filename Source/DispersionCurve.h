#pragma once

#include <vector>
#include <complex>

//==============================================================================
// 色散链的频响分析（纯数学，不依赖 GUI）
//
// 为什么只算群延迟：
//   全通滤波器 |H| = 1，幅度谱是**平的** —— 用频谱看不出"处理了哪些频率"。
//   真正被改动的是相位，能被听见的后果是**群延迟**（不同频率被拖慢多久）。
//   所以频率视图画的是群延迟曲线，而不是普通的 EQ / 频谱曲线。
//
// 系数公式与 DispersionEngine::recomputeStageCoefficients 完全一致，
// 参数钳位也一致（频率 20 Hz..0.45·sr，Q ≥ 0.005，alpha ≥ 2e-4）。
//==============================================================================
namespace dispersion
{
    constexpr double lowestHz  = 20.0;
    constexpr double highestHz = 20000.0;

    struct StageCoefficients
    {
        double a1 = 0.0;    // 分母 z^-1 系数（与分子相同，全通）
        double a2 = 1.0;    // 分母 z^-2 系数
    };

    StageCoefficients makeStageCoefficients (double centreHz, double q, double sampleRate);

    // 单级双二阶全通在 freqHz 处的群延迟（秒）。
    // 用 H'/H 的虚部精确求导，不做差分，低频高 Q 时也不会因为相位卷绕算错。
    double stageGroupDelaySeconds (double freqHz, double sampleRate, double centreHz, double q);

    // 整条链 = 级数 × 单级（每级系数相同）
    double chainGroupDelaySeconds (double freqHz, double sampleRate, double centreHz, double q, int stages);

    // 强度 0..1 → 串联级数 0..50（与引擎的取整方式一致）
    int stageCountForIntensity (float intensity);

    //--------------------------------------------------------------------------
    // 画曲线用的一次性快照
    //--------------------------------------------------------------------------
    struct Curve
    {
        std::vector<float> hz;          // 对数等间隔的采样频率
        std::vector<float> delayMs;     // 对应的群延迟（毫秒，整条链）
        float peakMs = 0.0f;            // 曲线峰值
        float bandLowHz = 0.0f;         // 受影响频段（群延迟 ≥ 峰值 25%）
        float bandHighHz = 0.0f;
        bool bandValid = false;
        int stages = 0;
    };

    // numPoints >= 2
    Curve computeCurve (double sampleRate, double centreHz, double q, int stages, int numPoints = 720);
}
