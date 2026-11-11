// "Luminance histogram" window, opened from the Exposure section of the Settings
// window. Visualizes the luminance histogram buffer of the ExposureRenderStage.

#include "Framework/FrameworkPCH.h"
#include "Framework/UI/LuminanceHistogramWindow.h"
#include "Framework/UI/FrameworkUI.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/RenderStages/ExposureRenderStage.h"

void alm::fw::LuminanceHistogramWindow::Build()
{
    auto renderGraph = m_UI.GetUIRenderView()->GetRenderGraph().get();
    auto exposureRS = renderGraph->GetRenderStage<alm::gfx::ExposureRenderStage>();

    if (!exposureRS)
        return;

    if (!m_Open)
    {
        // Closed: release the render graph buffer view ticket
        if (m_BufferTicket.IsValid())
        {
            renderGraph->ReleaseBufferView(m_BufferTicket);
            m_BufferTicket = { nullptr };
        }
        return;
    }

    if (!m_BufferTicket.IsValid())
    {
        m_BufferTicket = renderGraph->RequestBufferView(
            exposureRS->GetType(), alm::gfx::RenderGraph::AccessMode::Write, renderGraph->GetBufferHandle("LuminanceHistogram"));
    }
    if (!m_BufferTicket.IsValid())
    {
        LOG_ERROR("Failed to requesting histogram buffer ticket");
        return;
    }

    ImGui::SetNextWindowSize(ImVec2{ 600, 200 }, ImGuiCond_Once);
    if (!ImGui::Begin("Luminance histogram", &m_Open, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    alm::gfx::ExposureRenderStage::Stats stats = exposureRS->GetStats();
    alm::rhi::BufferHandle buffer = renderGraph->GetBufferView(m_BufferTicket);
    if (buffer)
    {
        void* data_ptr = buffer->Map();
        auto getValueLog2 = [](void* data, int idx) -> float
            {
                uint32_t* arr = (uint32_t*)data;
                return log2f((float)arr[idx] + 1.0f);
            };
        auto getValueLinear = [](void* data, int idx) -> float
            {
                uint32_t* arr = (uint32_t*)data;
                return (float)arr[idx];
            };

        ImVec2 size = ImGui::GetContentRegionAvail();
        size.y -= ImGui::GetFrameHeightWithSpacing();
        if (m_Mode == 0)
        {
            ImGui::PlotHistogram("##LuminanceHistogram",
                getValueLog2, data_ptr, buffer->GetDesc().sizeBytes / 4, 0, nullptr, 0.f, log2f(stats.totalPixels), size);
        }
        else
        {
            ImGui::PlotHistogram("##LuminanceHistogram",
                getValueLinear, data_ptr, buffer->GetDesc().sizeBytes / 4, 0, nullptr, 0.f, stats.totalPixels, size);
        }

        buffer->Unmap();

        if (ImGui::RadioButton("Log2", m_Mode == 0))
            m_Mode = 0;
        ImGui::SameLine();
        if (ImGui::RadioButton("Linear", m_Mode == 1))
            m_Mode = 1;
        ImGui::SameLine();
        ImGui::Text("| Min: %1.3f | Max: %1.3f | Avg: %1.3f | Bin: %1.1f", stats.minLuminance, stats.maxLuminance, stats.avgLuminance, stats.avgBin);
    }

    ImGui::End();
}
