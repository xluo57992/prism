#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
// 默认关闭。只有设了环境变量 PRISM_DEBUG=1 才写文件。
//
// 注意：参数变更回调可能跑在**音频线程**上（宿主自动化），所以调用点必须先用
// prismDebugEnabled() 挡一下，避免在实时回调里构造 juce::String 和写磁盘。
//==============================================================================
inline bool prismDebugEnabled()
{
    static const bool enabled = juce::SystemStats::getEnvironmentVariable ("PRISM_DEBUG", "0") == "1";
    return enabled;
}

inline void PrismLog (const juce::String& message)
{
    if (! prismDebugEnabled())
        return;

    static const auto logFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                    .getChildFile ("prism-debug.log");

    static juce::CriticalSection logLock;
    const juce::ScopedLock sl (logLock);

    logFile.appendText (juce::Time::getCurrentTime().toString (true, true, true, true)
                            + "  " + message + "\n");
}
