#pragma once

#include <imgui/imgui.h>

#include <functional>
#include <string>
#include <vector>

namespace alm::fw
{

// Ring buffer of (t, value) samples for realtime plots (from the ImPlot demo).
// While not full, samples are appended; once full, samples overwrite the oldest
// slot and Offset points at it (ImPlot reads series starting from Offset).
struct ScrollingBuffer
{
    int MaxSize;
    int Offset;
    ImVector<ImVec2> Data;

    ScrollingBuffer(int max_size = 2000)
    {
        MaxSize = max_size;
        Offset = 0;
        Data.reserve(MaxSize);
    }
    void AddPoint(float x, float y)
    {
        if (Data.size() < MaxSize)
            Data.push_back(ImVec2(x, y));
        else
        {
            Data[Offset] = ImVec2(x, y);
            Offset = (Offset + 1) % MaxSize;
        }
    }
    void Erase()
    {
        if (Data.size() > 0)
        {
            Data.shrink(0);
            Offset = 0;
        }
    }
};

// Realtime multi-line time plot:
//   - N named series fed by functors (sampled on Update at a fixed interval).
//   - Scrolling X window of a fixed duration; Y auto-scaled per frame over the
//     samples inside the visible window, min locked to 0.
//   - Legend click toggles a series; the current-value label at the right of
//     the plot follows that toggle and matches the series' color.
//
// Usage: AddLine() each series once, then Update() every frame the host window
// is built. Reset() when the host window closes (fresh plot on reopen).
class TimePlot
{
public:

    using ValueFn = std::function<float()>;

    void AddLine(const char* label, const ImVec4& color, ValueFn valueFn);
    void AddLine(const char* label, ValueFn valueFn);   // auto color (cycling palette)

    void SetVisibleSeconds(float seconds) { m_VisibleSeconds = seconds; }   // scrolling X window
    void SetSampleInterval(float seconds) { m_SampleInterval = seconds; }
    void SetLabelStripWidth(float px) { m_LabelStrip = px; }                // px reserved at the right for value labels
    void SetBottomMargin(float px) { m_BottomMargin = px; }                 // px of air between the plot and its container bottom
    void SetMinY(float v) { m_PlotMinY = v; }

    void Reset();
    void Update();                  // fills the available region
    void Update(const ImVec2& size);   // explicit plot region size (includes label strip + bottom margin)

private:

    struct Line
    {
        std::string Label;
        ImVec4 Color;
        ValueFn Value;
        ScrollingBuffer History;

        // Per-frame cache for the value labels
        float LastPixY = 0.f;
        float LastValue = 0.f;
        bool Visible = true;
    };

    std::vector<Line> m_Lines;

    float m_T = 0.f;
    float m_LastT = 0.f;
    float m_VisibleSeconds = 10.f;
    float m_SampleInterval = 0.02f;
    float m_LabelStrip = 45.f;
    float m_BottomMargin = 0.f;
    uint32_t m_AutoColorIdx = 0;
    float m_PlotMinY = 1.f;
    std::string m_PlotId;
};

} // namespace alm::fw
