//==============================================================================
// PrismEditorProbe —— 「打开界面会不会改参数」的回归测试。
//
// 模拟宿主的两个动作：
//   1. setCurrentProgram(0)      —— REAPER 填充 program 下拉框时会回调
//   2. 创建编辑器 + 派发消息循环  —— ComboBox 的 onChange 是异步的，不跑消息循环测不出来
//
// 任何参数被改动都算失败。
//==============================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "PrismProcessor.h"
#include "PrismEditor.h"
#include "FrequencyView.h"

#include <cstdio>
#include <vector>

namespace
{
    const char* ids[] = { PrismProcessor::pFrequency, PrismProcessor::pQ, PrismProcessor::pIntensity,
                          PrismProcessor::pMix,       PrismProcessor::pOutputDb };

    constexpr int numParams = (int) (sizeof (ids) / sizeof (ids[0]));

    juce::Component* findByID (juce::Component& root, const juce::String& wantedId)
    {
        if (root.getComponentID() == wantedId)
            return &root;

        for (int i = 0; i < root.getNumChildComponents(); ++i)
            if (auto* found = findByID (*root.getChildComponent (i), wantedId))
                return found;

        return nullptr;
    }

    // 「关于」面板固定 600×474，用它精确匹配（不能拿尺寸下限去匹配，会匹配到 720×720 的画布）
    juce::Component* findAboutPanel (juce::Component& root)
    {
        for (int i = 0; i < root.getNumChildComponents(); ++i)
        {
            auto* c = root.getChildComponent (i);

            if (c->getWidth() == 600 && c->getHeight() == 474)
                return c;

            if (auto* found = findAboutPanel (*c))
                return found;
        }

        return nullptr;
    }

    float readParam (PrismProcessor& p, const char* id)
    {
        if (auto* v = p.parameters.getRawParameterValue (id))
            return v->load();

        return -999.0f;
    }

    void writeParam (PrismProcessor& p, const char* id, float value)
    {
        if (auto* base = p.parameters.getParameter (id))
            if (auto* f = dynamic_cast<juce::AudioParameterFloat*> (base))
                base->setValueNotifyingHost (base->convertTo0to1 (value));
    }

    void snapshot (PrismProcessor& p, const char* tag, float* out)
    {
        std::printf ("--- %s ---\n", tag);

        for (int i = 0; i < numParams; ++i)
        {
            out[i] = readParam (p, ids[i]);
            std::printf ("  %-12s = %.4f\n", ids[i], out[i]);
        }
    }
}

//==============================================================================
int main()
{
    setvbuf (stdout, nullptr, _IONBF, 0);   // 崩溃时也能看到进度

    juce::ScopedJuceInitialiser_GUI init;

    PrismProcessor processor;

    // 一个明显不是任何预设的状态
    writeParam (processor, PrismProcessor::pFrequency, 777.0f);
    writeParam (processor, PrismProcessor::pQ,         0.33f);
    writeParam (processor, PrismProcessor::pIntensity, 0.66f);
    writeParam (processor, PrismProcessor::pMix,       0.44f);
    writeParam (processor, PrismProcessor::pOutputDb,  -2.5f);

    float before[numParams] {};
    float afterProgram[numParams] {};
    float afterCreate[numParams] {};
    float afterDestroy[numParams] {};

    std::vector<float> fftInput (8192, 0.0f);

    snapshot (processor, "打开界面前", before);

    processor.setCurrentProgram (0);
    snapshot (processor, "宿主 setCurrentProgram(0) 后", afterProgram);

    {
        PrismEditor editor (processor);

        if (auto* mm = juce::MessageManager::getInstance())
            mm->runDispatchLoopUntil (300);

        snapshot (processor, "界面创建 + 消息循环后", afterCreate);
    }

    if (auto* mm = juce::MessageManager::getInstance())
        mm->runDispatchLoopUntil (150);

    snapshot (processor, "界面销毁后", afterDestroy);

    int failures = 0;

    for (int i = 0; i < numParams; ++i)
    {
        if (std::abs (before[i] - afterProgram[i]) > 1.0e-4f)
        {
            std::printf ("[FAIL] setCurrentProgram(0) 改动了 %s: %.4f -> %.4f\n", ids[i], before[i], afterProgram[i]);
            ++failures;
        }

        if (std::abs (before[i] - afterCreate[i]) > 1.0e-4f)
        {
            std::printf ("[FAIL] 创建编辑器改动了 %s: %.4f -> %.4f\n", ids[i], before[i], afterCreate[i]);
            ++failures;
        }

        if (std::abs (before[i] - afterDestroy[i]) > 1.0e-4f)
        {
            std::printf ("[FAIL] 销毁编辑器改动了 %s: %.4f -> %.4f\n", ids[i], before[i], afterDestroy[i]);
            ++failures;
        }
    }

    if (failures == 0)
    {
        std::printf ("\n=== PASS: 宿主 program 回调 + 打开/关闭界面，都不改动任何参数 ===\n");
    }
    else
    {
        std::printf ("\n=== FAIL: %d 处参数被改动 ===\n", failures);
        return 1;
    }

    //--------------------------------------------------------------------------
    // 「关于」按钮：点一下必须真的把面板弹出来（可见 + 有尺寸）
    //--------------------------------------------------------------------------
    {
        juce::Component* overlay = nullptr;

        {
            PrismEditor editor (processor);

            if (auto* mm = juce::MessageManager::getInstance())
                mm->runDispatchLoopUntil (200);

            auto* button = findByID (editor, "prismAboutButton");

            if (button == nullptr)
            {
                std::printf ("[FAIL] 找不到「关于」按钮\n");
                return 1;
            }

            if (auto* b = dynamic_cast<juce::Button*> (button))
            {
                std::printf ("[about] 点击前可见面板: %s\n",
                             findAboutPanel (editor) != nullptr ? "有" : "无");

                b->triggerClick();

                if (auto* mm2 = juce::MessageManager::getInstance())
                    mm2->runDispatchLoopUntil (250);

                auto* panel = findAboutPanel (editor);

                std::printf ("[about] 点击后: 面板=%s 可见=%s\n",
                             panel != nullptr ? "有" : "无",
                             (panel != nullptr && panel->isVisible()) ? "是" : "否");

                if (panel != nullptr && panel->isVisible())
                    overlay = panel;
            }
        }

        if (overlay == nullptr)
        {
            std::printf ("=== FAIL: 点「关于」没有弹出面板 ===\n");
            return 1;
        }
    }

    std::printf ("=== PASS: 「关于」按钮能弹出面板 ===\n");

    //--------------------------------------------------------------------------
    // 「频率视图」按钮：必须另开一个顶层窗口，而且开窗不改任何参数
    //--------------------------------------------------------------------------
    {
        float beforeOpen[numParams] {};
        snapshot (processor, "打开频率视图前", beforeOpen);

        bool windowSeen = false;

        {
            PrismEditor editor (processor);

            if (auto* mm = juce::MessageManager::getInstance())
                mm->runDispatchLoopUntil (200);

            auto* button = findByID (editor, "prismFreqViewButton");

            if (button == nullptr)
            {
                std::printf ("[FAIL] 找不到「频率视图」按钮\n");
                return 1;
            }

            if (auto* b = dynamic_cast<juce::Button*> (button))
            {
                b->triggerClick();

                if (auto* mm2 = juce::MessageManager::getInstance())
                    mm2->runDispatchLoopUntil (300);
            }

            const juce::String wanted (juce::String::fromUTF8 ("棱镜 · 频率视图"));

            for (int i = 0; i < juce::TopLevelWindow::getNumTopLevelWindows(); ++i)
            {
                if (auto* tw = juce::TopLevelWindow::getTopLevelWindow (i))
                {
                    if (tw->getName() == wanted)
                    {
                        windowSeen = tw->isVisible() && tw->getWidth() > 400 && tw->getHeight() > 300;

                        std::printf ("[freq] 找到窗口 %s  %dx%d 可见=%s\n",
                                     tw->getName().toRawUTF8(), tw->getWidth(), tw->getHeight(),
                                     tw->isVisible() ? "是" : "否");
                    }
                }
            }

            // 关掉再切一次：第二次点击应该只是隐藏 / 显示，而不是重建
            if (auto* b = dynamic_cast<juce::Button*> (button))
            {
                b->triggerClick();

                if (auto* mm2 = juce::MessageManager::getInstance())
                    mm2->runDispatchLoopUntil (150);
            }
        }

        if (! windowSeen)
        {
            std::printf ("=== FAIL: 点「频率视图」没有开出可用的窗口 ===\n");
            return 1;
        }

        float afterClose[numParams] {};
        snapshot (processor, "关掉频率视图后", afterClose);

        for (int i = 0; i < numParams; ++i)
        {
            if (std::abs (beforeOpen[i] - afterClose[i]) > 1.0e-4f)
            {
                std::printf ("[FAIL] 开关频率视图改动了 %s: %.4f -> %.4f\n", ids[i], beforeOpen[i], afterClose[i]);
                return 1;
            }
        }

        std::printf ("=== PASS: 「频率视图」能另开窗口，且不影响参数 ===\n");
    }

    //--------------------------------------------------------------------------
    // 旁通：关掉「效果」后，输出必须与输入逐样本一致
    //--------------------------------------------------------------------------
    {
        PrismProcessor fx;
        fx.prepareToPlay (48000.0, 512);

        if (auto* prm = fx.parameters.getParameter (PrismProcessor::pEffect))
            prm->setValueNotifyingHost (0.0f);

        juce::AudioBuffer<float> buf (2, 512);

        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const float v = 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * (double) i / 48000.0);
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }

        const juce::AudioBuffer<float> before (buf);
        juce::MidiBuffer midi;

        fx.processBlock (buf, midi);

        float maxDiff = 0.0f;

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < buf.getNumSamples(); ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (buf.getSample (ch, i) - before.getSample (ch, i)));

        std::printf ("\n旁通最大偏差 = %.8f\n", maxDiff);

        if (maxDiff > 1.0e-6f)
        {
            std::printf ("=== FAIL: 旁通没有完全直通 ===\n");
            return 1;
        }

        std::printf ("=== PASS: 旁通时逐样本直通 ===\n");
    }

    //--------------------------------------------------------------------------
    // 频谱数据通路：喂 1kHz 正弦，读回环形缓冲做 FFT，主峰必须落在 1kHz 附近
    //--------------------------------------------------------------------------
    {
        std::printf ("[spec] prepareToPlay...\n");
        processor.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> tone (2, 4096);
        const double twoPi = juce::MathConstants<double>::twoPi;

        for (int i = 0; i < tone.getNumSamples(); ++i)
        {
            const float v = 0.5f * (float) std::sin (twoPi * 1000.0 * (double) i / 48000.0);
            tone.setSample (0, i, v);
            tone.setSample (1, i, v);
        }

        juce::MidiBuffer midi;

        std::printf ("[spec] processBlock x4...\n");
        for (int rep = 0; rep < 4; ++rep)
            processor.processBlock (tone, midi);

        std::printf ("[spec] readSpectrum...\n");
        const int n = processor.readSpectrum (fftInput.data(), (int) fftInput.size());
        std::printf ("[spec] got %d samples\n", n);

        if (n < 4096)
        {
            std::printf ("[FAIL] readSpectrum 只读到 %d 个样本\n", n);
            return 1;
        }

        // 取最后 4096 个样本做 FFT
        const int fftOrder = 12;
        const int fftSize = 1 << fftOrder;

        std::vector<float> data ((size_t) fftSize * 2, 0.0f);

        for (int i = 0; i < fftSize; ++i)
            data[(size_t) i] = fftInput[(size_t) (n - fftSize + i)];

        juce::dsp::FFT fft (fftOrder);
        juce::dsp::WindowingFunction<float> window (fftSize, juce::dsp::WindowingFunction<float>::hann);

        window.multiplyWithWindowingTable (data.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform (data.data());

        int peakBin = 1;
        float peakMag = 0.0f;

        for (int b = 1; b < fftSize / 2; ++b)
        {
            if (data[(size_t) b] > peakMag)
            {
                peakMag = data[(size_t) b];
                peakBin = b;
            }
        }

        const double peakHz = (double) peakBin * 48000.0 / (double) fftSize;

        std::printf ("\n频谱主峰 = %.1f Hz（期望 ~1000 Hz）\n", peakHz);

        if (std::abs (peakHz - 1000.0) > 40.0)
        {
            std::printf ("=== FAIL: 频谱数据通路不对 ===\n");
            return 1;
        }

        std::printf ("=== PASS: 频谱数据通路正常 ===\n");
    }

    //--------------------------------------------------------------------------
    // 渲染一张频谱图（粉噪 + 1kHz），用来人工核对低频不再是平顶
    //--------------------------------------------------------------------------
    {
        PrismProcessor fx;
        fx.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buffer (2, 4096);
        juce::Random rng (7);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const double t = (double) i / 48000.0;
            const double twoPi = juce::MathConstants<double>::twoPi;

            // 三个等幅正弦：100 Hz / 1 kHz / 10 kHz，正好对应三条刻度线
            const float v = 0.25f * (float) (std::sin (twoPi * 100.0 * t)
                                           + std::sin (twoPi * 1000.0 * t)
                                           + std::sin (twoPi * 10000.0 * t))
                          + 0.001f * (rng.nextFloat() * 2.0f - 1.0f);

            buffer.setSample (0, i, v);
            buffer.setSample (1, i, v);
        }

        juce::MidiBuffer midi2;

        for (int rep = 0; rep < 6; ++rep)
            fx.processBlock (buffer, midi2);

        SpectrumDisplay display (fx);
        display.setSize (400, 120);

        for (int k = 0; k < 6; ++k)
            display.update();

        const auto snapshot = display.createComponentSnapshot (display.getLocalBounds(), true, 2.0f);

        juce::File outFile ("spectrum-test.png");
        outFile.deleteFile();

        if (auto stream = std::unique_ptr<juce::FileOutputStream> (outFile.createOutputStream()))
        {
            juce::PNGImageFormat png;
            png.writeImageToStream (snapshot, *stream);
            std::printf ("[spec] 频谱渲染 -> %s\n", outFile.getFullPathName().toRawUTF8());
        }
    }

    //--------------------------------------------------------------------------
    // 三个频率各渲染一张，检查「低在下、高在上」的映射
    //--------------------------------------------------------------------------
    for (float freqHz : { 20.0f, 1000.0f, 20000.0f })
    {
        PrismProcessor fx;
        fx.prepareToPlay (48000.0, 512);

        if (auto* prm = fx.parameters.getParameter (PrismProcessor::pFrequency))
            prm->setValueNotifyingHost (prm->convertTo0to1 (freqHz));

        PrismEditor editor (fx);
        editor.setSize (600, 600);

        if (auto* mm = juce::MessageManager::getInstance())
            mm->runDispatchLoopUntil (200);

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        double phase = 0.0;

        for (int block = 0; block < 20; ++block)
        {
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                phase += 2.0 * juce::MathConstants<double>::pi * 90.0 / 48000.0;
                const float v = 0.55f * (float) std::sin (phase)
                              + 0.25f * (float) std::sin (phase * 3.1);
                buffer.setSample (0, i, v);
                buffer.setSample (1, i, v);
            }

            fx.processBlock (buffer, midi);

            if (auto* mm = juce::MessageManager::getInstance())
                mm->runDispatchLoopUntil (25);
        }

        if (auto* mm = juce::MessageManager::getInstance())
            mm->runDispatchLoopUntil (80);

        const auto snap = editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.5f);

        juce::File outFile ("ui-freq-" + juce::String ((int) freqHz) + ".png");
        outFile.deleteFile();

        if (auto stream = std::unique_ptr<juce::FileOutputStream> (outFile.createOutputStream()))
        {
            juce::PNGImageFormat png;
            png.writeImageToStream (snap, *stream);
            std::printf ("[visual] %5d Hz -> %s\n", (int) freqHz, outFile.getFullPathName().toRawUTF8());
        }
    }

    //--------------------------------------------------------------------------
    // 渲染频率视图（docs 用图）：
    //   默认 Q=0.50 —— 过阻尼，延迟堆在低频端
    //   Q=0.08      —— 欠阻尼，峰收回中心频率
    //--------------------------------------------------------------------------
    {
        auto renderView = [] (float qValue, const juce::String& fileName, const char* label)
        {
            PrismProcessor fx;
            fx.prepareToPlay (48000.0, 512);

            auto setParam = [&fx] (const char* id, float value)
            {
                if (auto* prm = fx.parameters.getParameter (id))
                    prm->setValueNotifyingHost (prm->convertTo0to1 (value));
            };

            setParam (PrismProcessor::pFrequency, 1000.0f);
            setParam (PrismProcessor::pQ,         qValue);
            setParam (PrismProcessor::pIntensity, 0.66f);

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            juce::Random rng (3);
            double phase = 0.0;

            FrequencyViewComponent view (fx);
            std::printf ("[freqview] constructed\n");
            view.setSize (920, 560);
            std::printf ("[freqview] sized\n");

            for (int block = 0; block < 90; ++block)
            {
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    phase += juce::MathConstants<double>::twoPi * 110.0 / 48000.0;

                    const float v = 0.30f * (float) std::sin (phase)
                                  + 0.16f * (float) std::sin (phase * 9.0)
                                  + 0.10f * (rng.nextFloat() * 2.0f - 1.0f);

                    buffer.setSample (0, i, v);
                    buffer.setSample (1, i, v);
                }

                fx.processBlock (buffer, midi);

                if (auto* mm = juce::MessageManager::getInstance())
                    mm->runDispatchLoopUntil (12);
            }

            const auto snap = view.createComponentSnapshot (view.getLocalBounds(), true, 1.0f);

            juce::File outFile (fileName);
            outFile.deleteFile();

            if (auto stream = std::unique_ptr<juce::FileOutputStream> (outFile.createOutputStream()))
            {
                juce::PNGImageFormat png;
                png.writeImageToStream (snap, *stream);
                std::printf ("[visual] 频率视图（%s）-> %s\n", label, outFile.getFullPathName().toRawUTF8());
            }
        };

        renderView (0.08f, "ui-frequency-view.png", "Q=0.08");
        renderView (0.50f, "ui-frequency-view-damped.png", "Q=0.50");
    }

    return 0;
}
