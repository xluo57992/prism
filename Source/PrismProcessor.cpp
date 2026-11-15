#include "PrismProcessor.h"
#include "PrismEditor.h"
#include "PrismLog.h"

//==============================================================================
void PrismProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    // 这个回调可能跑在音频线程上（宿主自动化），所以先挡一道再构造字符串
    if (! prismDebugEnabled())
        return;

    PrismLog ("  [param] " + parameterID + " = " + juce::String (newValue, 5));
}

//==============================================================================
PrismProcessor::PrismProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PRISM_STATE",
                 {
                     std::make_unique<juce::AudioParameterFloat> (
                         juce::ParameterID { pFrequency, 1 }, juce::String::fromUTF8 ("频率"),
                         juce::NormalisableRange<float> { 20.0f, 20000.0f, 1.0f, 0.25f }, 1000.0f,
                         juce::AudioParameterFloatAttributes().withLabel ("Hz")),

                     std::make_unique<juce::AudioParameterFloat> (
                         juce::ParameterID { pQ, 1 }, juce::String::fromUTF8 ("Q"),
                         juce::NormalisableRange<float> { 0.02f, 1.0f, 0.001f }, 0.50f),

                     std::make_unique<juce::AudioParameterFloat> (
                         juce::ParameterID { pIntensity, 1 }, juce::String::fromUTF8 ("强度"),
                         juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.35f),

                     std::make_unique<juce::AudioParameterFloat> (
                         juce::ParameterID { pMix, 1 }, juce::String::fromUTF8 ("混音"),
                         juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.00f),

                     std::make_unique<juce::AudioParameterFloat> (
                         juce::ParameterID { pOutputDb, 1 }, juce::String::fromUTF8 ("输出增益"),
                         juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f }, 0.0f,
                         juce::AudioParameterFloatAttributes().withLabel ("dB")),

                     std::make_unique<juce::AudioParameterBool> (
                         juce::ParameterID { pEffect, 1 }, juce::String::fromUTF8 ("效果"), true)
                 })
{
    PrismLog (juce::String ("=== Prism (dispersion) created, built ") + __DATE__ + " " + __TIME__
              + " | getNumPrograms()=" + juce::String (getNumPrograms()) + " ===");

    if (prismDebugEnabled())
        for (const auto& id : getAllParamIds())
            parameters.addParameterListener (id, this);
}

PrismProcessor::~PrismProcessor()
{
    for (const auto& id : getAllParamIds())
        parameters.removeParameterListener (id, this);

    PrismLog ("=== Prism instance destroyed ===");
}

//==============================================================================
bool PrismProcessor::isEffectOn() const
{
    if (auto* v = parameters.getRawParameterValue (pEffect))
        return v->load() > 0.5f;

    return true;
}

//==============================================================================
const std::vector<juce::String>& PrismProcessor::getAllParamIds()
{
    static const std::vector<juce::String> ids = {
        pFrequency, pQ, pIntensity, pMix, pOutputDb, pEffect
    };

    return ids;
}

//==============================================================================
void PrismProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    PrismLog ("--- prepareToPlay(sr=" + juce::String (sampleRate)
              + ", block=" + juce::String (samplesPerBlock) + ") ---");

    currentSampleRate.store (sampleRate > 1000.0 ? sampleRate : 48000.0);

    engine.prepare (sampleRate);
    pushParamsToEngine();
}

void PrismProcessor::releaseResources()
{
    engine.reset();
}

bool PrismProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainInput  = layouts.getChannelSet (true,  0);
    const auto& mainOutput = layouts.getChannelSet (false, 0);

    if (mainInput != mainOutput)
        return false;

    return mainInput.size() <= 2;
}

//==============================================================================
void PrismProcessor::pushParamsToEngine()
{
    DispersionEngine::Params p;

    auto get = [this] (const char* id, float fallback)
    {
        if (auto* v = parameters.getRawParameterValue (id))
            return v->load();

        return fallback;
    };

    p.frequencyHz = get (pFrequency, 1000.0f);
    p.q           = get (pQ, 0.50f);
    p.intensity   = get (pIntensity, 0.35f);
    p.mix         = get (pMix, 1.00f);
    p.outputDb    = get (pOutputDb, 0.0f);

    engine.setParams (p);

    intensityLevel.store (p.intensity);
}

//==============================================================================
void PrismProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (isEffectOn())
    {
        pushParamsToEngine();
        engine.process (buffer);
    }

    //--------------------------------------------------------------------------
    // 把输出灌进频谱环形缓冲（给界面上的频谱用）
    //--------------------------------------------------------------------------
    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();

    if (numSamples > 0 && numCh > 0)
    {
        int start1, size1, start2, size2;
        spectrumFifo.prepareToWrite (numSamples, start1, size1, start2, size2);

        // 注意：ringStart 是环形缓冲里的绝对位置（最大 16383），
        // 不能拿去当 buffer 的采样索引 —— 要用块内偏移 srcOffset。
        auto push = [&] (int ringStart, int count, int srcOffset)
        {
            for (int k = 0; k < count; ++k)
            {
                const int src = srcOffset + k;

                float v = buffer.getSample (0, src);

                if (numCh > 1)
                    v = 0.5f * (v + buffer.getSample (1, src));

                spectrumData[(size_t) (ringStart + k)] = v;
            }
        };

        push (start1, size1, 0);
        push (start2, size2, size1);
        spectrumFifo.finishedWrite (size1 + size2);
    }

    //--------------------------------------------------------------------------
    // 电平表：快起慢落，给背景的光强度用（界面线程只读原子量）
    //--------------------------------------------------------------------------
    {
        float peak = 0.0f;

        for (int ch = 0; ch < numCh; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, numSamples));

        const float prev = meterLevel.load (std::memory_order_relaxed);
        meterLevel.store (juce::jmax (peak, prev * 0.88f), std::memory_order_relaxed);
    }
}

//==============================================================================
int PrismProcessor::readSpectrum (float* dest, int maxSamples)
{
    int start1, size1, start2, size2;
    spectrumFifo.prepareToRead (maxSamples, start1, size1, start2, size2);

    int written = 0;

    for (int i = 0; i < size1; ++i)
        dest[written++] = spectrumData[(size_t) (start1 + i)];

    for (int i = 0; i < size2; ++i)
        dest[written++] = spectrumData[(size_t) (start2 + i)];

    spectrumFifo.finishedRead (size1 + size2);

    return written;
}

//==============================================================================
juce::AudioProcessorEditor* PrismProcessor::createEditor()
{
    return new PrismEditor (*this);
}

//==============================================================================
// 状态：手写二进制，版本 3（棱镜版）。旧版本的状态会被直接忽略。
void PrismProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    PrismLog ("--- getStateInformation() ---");

    juce::MemoryOutputStream mos (destData, false);

    mos.writeInt (3);                      // 版本号

    for (const auto& id : getAllParamIds())
    {
        mos.writeString (id);

        auto* p = parameters.getParameter (id);
        mos.writeFloat (p != nullptr ? p->getValue() : 0.0f);   // 归一化值
    }
}

void PrismProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    PrismLog ("--- setStateInformation(" + juce::String (sizeInBytes) + " bytes) ---");

    juce::MemoryInputStream mis (data, (size_t) sizeInBytes, false);

    if (mis.readInt() != 3)
    {
        PrismLog ("  (忽略旧版本状态)");
        return;
    }


    for (const auto& id : getAllParamIds())
    {
        if (mis.isExhausted())
            break;

        const auto storedId = mis.readString();
        const float normalised = mis.readFloat();

        if (storedId != id)
            continue;

        if (auto* target = parameters.getParameter (id))
            target->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
    }

    pushParamsToEngine();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PrismProcessor();
}
