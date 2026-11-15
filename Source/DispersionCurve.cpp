#include "DispersionCurve.h"

#include <algorithm>
#include <cmath>

namespace dispersion
{
namespace
{
    constexpr double twoPi = 6.283185307179586476925286766559;
}

//==============================================================================
StageCoefficients makeStageCoefficients (double centreHz, double q, double sampleRate)
{
    StageCoefficients c;

    if (sampleRate <= 0.0)
        return c;

    // 与引擎相同的钳位
    centreHz = std::min (std::max (centreHz, 20.0), sampleRate * 0.45);
    q        = std::max (q, 0.005);

    const double w0    = twoPi * centreHz / sampleRate;
    const double alpha = std::max (2.0e-4, std::sin (w0) * (2.0 * std::sqrt (2.0) * q));
    const double a0    = 1.0 + alpha;

    c.a1 = -2.0 * std::cos (w0) / a0;
    c.a2 = (1.0 - alpha) / a0;

    return c;
}

//==============================================================================
double stageGroupDelaySeconds (double freqHz, double sampleRate, double centreHz, double q)
{
    if (sampleRate <= 0.0 || freqHz <= 0.0)
        return 0.0;

    // 采样点超出奈奎斯特就没有意义了（曲线本身最多画到 20 kHz）
    if (freqHz >= sampleRate * 0.5)
        return 0.0;

    const auto c = makeStageCoefficients (centreHz, q, sampleRate);

    // H(z) = (a2 + a1·z⁻¹ + z⁻²) / (1 + a1·z⁻¹ + a2·z⁻²)
    // τ(ω) = -dφ/dω = -Im(H'/H)，其中 H'/H = N'/N − D'/D
    const double w = twoPi * freqHz / sampleRate;

    const std::complex<double> u  (std::cos (w), -std::sin (w));   // e^{-jω}
    const std::complex<double> u2 = u * u;
    const std::complex<double> j  (0.0, 1.0);

    const std::complex<double> N  = c.a2 + c.a1 * u + u2;
    const std::complex<double> Np = -j * (c.a1 * u + 2.0 * u2);

    const std::complex<double> D  = 1.0 + c.a1 * u + c.a2 * u2;
    const std::complex<double> Dp = -j * (c.a1 * u + 2.0 * c.a2 * u2);

    const double dPhiPerSample = std::imag (Np / N - Dp / D);

    return -dPhiPerSample / sampleRate;
}

//==============================================================================
double chainGroupDelaySeconds (double freqHz, double sampleRate, double centreHz, double q, int stages)
{
    if (stages <= 0)
        return 0.0;

    return stageGroupDelaySeconds (freqHz, sampleRate, centreHz, q) * (double) stages;
}

//==============================================================================
int stageCountForIntensity (float intensity)
{
    return (int) std::lround (std::min (std::max (intensity, 0.0f), 1.0f) * 50.0f);
}

//==============================================================================
Curve computeCurve (double sampleRate, double centreHz, double q, int stages, int numPoints)
{
    Curve curve;
    curve.stages = std::max (stages, 0);

    numPoints = std::max (numPoints, 2);

    curve.hz.resize ((size_t) numPoints);
    curve.delayMs.resize ((size_t) numPoints);

    const double ratio = highestHz / lowestHz;

    for (int i = 0; i < numPoints; ++i)
    {
        const double pos = (double) i / (double) (numPoints - 1);
        const double hz  = lowestHz * std::pow (ratio, pos);

        curve.hz[(size_t) i] = (float) hz;

        const double delay = chainGroupDelaySeconds (hz, sampleRate, centreHz, q, curve.stages);

        curve.delayMs[(size_t) i] = (float) (delay * 1000.0);
    }

    if (curve.stages <= 0)
        return curve;

    curve.peakMs = *std::max_element (curve.delayMs.begin(), curve.delayMs.end());

    if (curve.peakMs <= 0.0f)
        return curve;

    //--------------------------------------------------------------------------
    // 受影响频段：群延迟 ≥ 峰值的 25%（约等于 -12 dB 的"主要作用范围"）
    // 曲线是单峰的（中心频率附近最高），所以从两端往中间扫即可。
    //--------------------------------------------------------------------------
    const float threshold = curve.peakMs * 0.25f;

    int first = 0;
    int last  = numPoints - 1;

    while (first < last && curve.delayMs[(size_t) first] < threshold) ++first;
    while (last > first && curve.delayMs[(size_t) last] < threshold) --last;

    if (curve.delayMs[(size_t) first] >= threshold)
    {
        curve.bandLowHz  = curve.hz[(size_t) first];
        curve.bandHighHz = curve.hz[(size_t) last];
        curve.bandValid  = true;
    }

    return curve;
}
}
