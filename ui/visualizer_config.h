#pragma once
#include <atomic>
struct PpduViewConfig
{
    std::atomic<bool> preciseMode{true};   // 默认全量精确模式
    std::atomic<int> sampleRate {1};
    std::atomic<bool> absoluteRate{true};
};

extern PpduViewConfig g_ppduViewConfig;
