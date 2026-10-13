#include "Framework/FrameworkPCH.h"
#include "Framework/UI/TimePlot.h"
#include "Framework/UI/ImPlot/implot.h"
#include "Framework/UI/ImPlot/implot_internal.h"   // ImPlot::GetItem: legend toggle state per series

namespace
{
    // Auto-assignment palette for lines added without an explicit color
    constexpr ImVec4 k_AutoPalette[] = {
        ImVec4(0.0f, 0.45f, 1.0f, 1.0f),    // blue
        ImVec4(1.0f, 0.6f, 0.0f, 1.0f),     // orange
        ImVec4(0.2f, 1.0f, 0.35f, 1.0f),    // green
        ImVec4(1.0f, 0.35f, 0.5f, 1.0f),    // pink
        ImVec4(0.7f, 0.35f, 1.0f, 1.0f),    // purple
        ImVec4(0.0f, 1.0f, 1.0f, 1.0f),     // cyan
        ImVec4(1.0f, 1.0f, 0.2f, 1.0f),     // yellow
        ImVec4(1.0f, 0.35f, 0.15f, 1.0f),   // red-orange
    };
    constexpr int k_AutoPaletteSize = (int)(sizeof(k_AutoPalette) / sizeof(k_AutoPalette[0]));

    uint32_t s_PlotInstanceCount = 0;   // unique plot ids for multiple TimePlot instances
}

namespace alm::fw
{

void TimePlot::AddLine(const char* label, const ImVec4& color, ValueFn valueFn)
{
    Line& line = m_Lines.emplace_back();
    line.Label = label;
    line.Color = color;
    line.Value = std::move(valueFn);
}

void TimePlot::AddLine(const char* label, ValueFn valueFn)
{
    const ImVec4& color = k_AutoPalette[m_AutoColorIdx % k_AutoPaletteSize];
    ++m_AutoColorIdx;
    AddLine(label, color, std::move(valueFn));
}

void TimePlot::Reset()
{
    for (Line& line : m_Lines)
        line.History.Erase();

    m_T = 0.f;
    m_LastT = 0.f;
}

void TimePlot::Update()
{
    Update(ImVec2(0, 0));
}

void TimePlot::Update(const ImVec2& size)
{
    const ImVec2 full = (size.x > 0 || size.y > 0) ? size : ImGui::GetContentRegionAvail();

    // Sample the lines at a fixed interval
    if (m_T == 0.f || m_T - m_LastT >= m_SampleInterval)
    {
        for (Line& line : m_Lines)
            line.History.AddPoint(m_T, line.Value());
        m_LastT = m_T;
    }
    m_T += ImGui::GetIO().DeltaTime;

    // Y max re-evaluated every frame over the samples inside the visible window,
    // counting only ACTIVE lines (legend-toggled lines don't inflate the height):
    // when peaks leave the X range, the height shrinks back. Y min locked to 0.
    // Note: uses the previous frame's legend state (the item Show query needs the plot context).
    float plotMaxY = m_PlotMinY;
    {
        const float minVisibleX = m_T - m_VisibleSeconds;
        auto accumulateMaxY = [minVisibleX, &plotMaxY](const ScrollingBuffer& line)
        {
            const int size = (int)line.Data.size();
            for (int i = 0; i < size; ++i)
                if (line.Data[i].x >= minVisibleX)
                    plotMaxY = std::max(plotMaxY, line.Data[i].y * 1.1f);
        };
        for (const Line& line : m_Lines)
            if (line.Visible)
                accumulateMaxY(line.History);
    }

    float plotRightEdge = 0.f;
    bool hasPlot = false;

    if (m_PlotId.empty())
        m_PlotId = "##TimePlot" + std::to_string(++s_PlotInstanceCount);

    if (ImPlot::BeginPlot(m_PlotId.c_str(), ImVec2(full.x - m_LabelStrip, full.y - m_BottomMargin)))
    {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_NoTickLabels);
        ImPlot::SetupAxisLimits(ImAxis_X1, m_T - m_VisibleSeconds, m_T, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, plotMaxY, ImGuiCond_Always);   // Y min locked to 0
        ImPlot::SetupFinish();

        // Series (explicit colors; clicking a legend entry toggles their visibility)
        for (Line& line : m_Lines)
        {
            ImPlotSpec spec;
            spec.Offset = line.History.Offset;
            spec.Stride = 2 * sizeof(float);
            spec.LineColor = line.Color;
            ImPlot::PlotLine(line.Label.c_str(), &line.History.Data[0].x, &line.History.Data[0].y, line.History.Data.size(), spec);
        }

        for (Line& line : m_Lines)
        {
            // The legend click flips item->Show — the value labels follow the toggle
            if (const ImPlotItem* item = ImPlot::GetItem(line.Label.c_str()))
                line.Visible = item->Show;

            if (!line.History.Data.empty())
            {
                // Ring buffer: the last written point is not Data.back() once the buffer wraps
                const int size = (int)line.History.Data.size();
                const ImVec2& lastPt = line.History.Data[(line.History.Offset + size - 1) % size];
                line.LastPixY = ImPlot::PlotToPixels(lastPt.x, lastPt.y).y;
                line.LastValue = lastPt.y;
            }
        }

        plotRightEdge = ImPlot::GetPlotPos().x + ImPlot::GetPlotSize().x;
        hasPlot = true;

        ImPlot::EndPlot();
    }

    // Value labels drawn on the window draw list, OUTSIDE the plot's clip rect:
    // right of the plot, at the pixel height of each series' last sample, in the series' color
    if (hasPlot)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float textHeight = ImGui::GetTextLineHeight();
        char buf[16];
        for (const Line& line : m_Lines)
        {
            if (!line.Visible || line.History.Data.empty())
                continue;
            snprintf(buf, sizeof(buf), "%.2f", line.LastValue);
            drawList->AddText(ImVec2(plotRightEdge + 10.f, line.LastPixY - textHeight * 0.5f),
                ImGui::GetColorU32(line.Color), buf);
        }
    }
}

} // namespace alm::fw
