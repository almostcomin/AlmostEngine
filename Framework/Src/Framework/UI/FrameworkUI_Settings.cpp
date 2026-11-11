// "Settings" window (View > Settings) and the "Luminance histogram" window opened
// from its Exposure section. These are UIWindow subclasses of the FrameworkUI
// window set (see Framework/UI/UIWindows.h); the Shadowmap, Directional light and
// Ambient sections live in the shared SettingsWidgets, shown by this window and
// by the Atmosphere/Lighting window.

#include "Framework/FrameworkPCH.h"
#include "Framework/CameraController.h"
#include "Framework/UI/FrameworkUI.h"
#include "Framework/UI/UIWindows.h"
#include "Framework/UI/FrameworkUIWidgets.h"
#include "Gfx/Scene.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/Camera.h"
#include "Gfx/RenderStages/DeferredLightingRenderStage.h"
#include "Gfx/RenderStages/BloomRenderStage.h"
#include "Gfx/RenderStages/ExposureRenderStage.h"
#include "RHI/Device.h"

void alm::fw::SettingsWindow::Build()
{
    if (!m_Open)
        return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Settings", &m_Open, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    const ImGuiStyle& style = ImGui::GetStyle();
    const float availWidth = ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2;

    BuildRenderModesSettings();
    BuildCameraSettings(availWidth);
    BuildDebugViewSettings(availWidth);
    m_UI.GetShadowmapWidget().Build(availWidth);
    m_UI.GetDirectionalLightWidget().Build(availWidth);
    m_UI.GetAmbientLightWidget().Build(availWidth);
    BuildMaterialChannelsSettings(availWidth);
    BuildSSAOSettings(availWidth);
    BuildBloomSettings(availWidth);
    BuildExposureSettings(availWidth);
    BuildTonemappingSettings(availWidth);

    ImGui::End();
}

void alm::fw::SettingsWindow::BuildRenderModesSettings()
{
    if (ImGui::CollapsingHeader("Render modes", ImGuiTreeNodeFlags_None))
    {
        auto renderGraph = m_UI.GetUIRenderView()->GetRenderGraph();
        std::vector<std::string> renderModes = renderGraph->GetRenderModes();
        int selectedIdx = 0;
        for (int i = 0; i < renderModes.size(); ++i)
        {
            if (renderModes[i] == renderGraph->GetCurrentRenderMode())
            {
                selectedIdx = i;
                break;
            }
        }

        if (ImGui::BeginListBox("##RenderModesListBox"))
        {
            for (int n = 0; n < renderModes.size(); n++)
            {
                const bool is_selected = (n == selectedIdx);
                if (ImGui::Selectable(renderModes[n].c_str(), is_selected))
                    selectedIdx = n;
                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndListBox();
        }

        if (renderModes[selectedIdx] != renderGraph->GetCurrentRenderMode())
            m_UI.FrameworkData.RequestedRenderMode = renderModes[selectedIdx];
    }
}

void alm::fw::SettingsWindow::BuildCameraSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_None))
    {
        auto camera = m_UI.GetUIRenderView()->GetCamera();

        // Position
        float3 pos = camera->GetPosition();
        if (ImGui::InputFloat3("Position", (float*)&pos))
        {
            camera->SetPosition(pos);
        }

        // Direction
        float yaw = camera->GetYaw();
        float pitch = camera->GetPitch();
        float roll = camera->GetRoll();

        if (ImGui::SliderAngle("Yaw", &yaw, -180.0f, 180.0f))
            camera->SetYaw(yaw);
        if (ImGui::SliderAngle("Pitch", &pitch, -90.0f, 90))
            camera->SetPitch(pitch);
        if (ImGui::SliderAngle("Roll", &roll, -180, 180.f))
            camera->SetRoll(roll);

        // FOV
        float fov = camera->GetVerticalFOV();
        if (ImGui::SliderAngle("FOV", &fov, 0.f, 180.f))
        {
            camera->SetVerticalFov(fov);
        }

        // 3 elements in a row
        float itemWidth = availWidth / 4;

        // Near plane
        ImGui::SetNextItemWidth(itemWidth);
        float znear = camera->GetZNear();
        if (ImGui::InputFloat("Near", &znear, 0.f, 0.f, "%.3f", 0))
        {
            camera->SetZNear(znear);
        }

        // Speed 
        if (auto* cameraController = m_UI.GetCameraController())
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(itemWidth);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + itemWidth / 2.f);
            float speed = cameraController->GetSpeed();
            if (ImGui::InputFloat("Speed", &speed, 0.f, 0.f, "%.3f", 0))
            {
                cameraController->SetSpeed(speed);
            }
        }
    }
}

void alm::fw::SettingsWindow::BuildDebugViewSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Debug view", ImGuiTreeNodeFlags_None))
    {
        ImGui::Checkbox("Mesh BBoxes", &m_UI.FrameworkData.Debug.ShowMeshBBoxes);
        ImGui::SameLine(availWidth / 2);
        ImGui::Checkbox("Light BBoxes", &m_UI.FrameworkData.Debug.ShowLightBBoxes);
        ImGui::Checkbox("Heightmap BBoxes", &m_UI.FrameworkData.Debug.ShowHeightmapBBoxes);
    }
}

void alm::fw::SettingsWindow::BuildMaterialChannelsSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Material channels"))
    {
        auto& data = m_UI.FrameworkData;
        if (ImGui::RadioButton("Disabled##matDisabled", data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Disabled))
            data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Disabled;
        if (ImGui::RadioButton("Base Color##matBaseColor", data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::BaseColor))
            data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::BaseColor;
        if (ImGui::RadioButton("Metalness##matMetalness", data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Metalness))
            data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Metalness;
        if (ImGui::RadioButton("Anisotropy##matAnisotropy", data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Anisotropy))
            data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Anisotropy;
        if (ImGui::RadioButton("Roughness##matRoughness", data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Roughness))
            data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Roughness;
        if (ImGui::RadioButton("Scattering##matScattering", data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Scattering))
            data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Scattering;
        if (ImGui::RadioButton("Translucency##matTranslucency", data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Translucency))
            data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Translucency;
        if (ImGui::RadioButton("Normal Map##matNormalMap", data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::NormalMap))
            data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::NormalMap;
        if (ImGui::RadioButton("Occlusion Map##matOcclusionMap", data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::OcclusionMap))
            data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::OcclusionMap;
        if (ImGui::RadioButton("Emissive##matEmissive", data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Emissive))
            data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Emissive;
        if (ImGui::RadioButton("Specular F0##matSpecularF0", data.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::SpecularF0))
            data.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::SpecularF0;
    }
}

void alm::fw::SettingsWindow::BuildSSAOSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("SSAO"))
    {
        auto& ssao = m_UI.FrameworkData.SSAO;

        ImGui::SetNextItemWidth(availWidth / 2);
        ImGui::Checkbox("Enabled##SSAO", &ssao.Enabled);
        ImGui::SameLine(availWidth / 2);
        ImGui::SetNextItemWidth(availWidth / 2);
        ImGui::Checkbox("Show##SSAO", &ssao.View);

        ImGui::InputFloat("Radius##SSAO", &ssao.Radius);
        ImGui::InputFloat("Power##SSAO", &ssao.Power);
        ImGui::InputFloat("Bias##Bias", &ssao.Bias);
    }
}

void alm::fw::SettingsWindow::BuildBloomSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Bloom"))
    {
        auto bloomRS = m_UI.GetUIRenderView()->GetRenderGraph()->GetRenderStage<alm::gfx::BloomRenderStage>();
        if (!bloomRS)
        { 
            ImGui::TextUnformatted("No BloomRenderStage present");
            return; 
        }

        bool enabled = bloomRS->IsBloomEnabled();
        if (ImGui::Checkbox("Enabled##Bloom", &enabled))
        {
            bloomRS->SetBloomEnabled(enabled);
        }

        float radius = bloomRS->GetFilterRadius();
        if (ImGui::InputFloat("Radius##Bloom", &radius))
        {
            bloomRS->SetFilterRadius(radius);
        }

        float internalBlend = bloomRS->GetInternalBlend();
        if (ImGui::InputFloat("Internal Blend##Bloom", &internalBlend))
        {
            internalBlend = alm::saturate(internalBlend);
            bloomRS->SetInternalBlend(internalBlend);
        }

        float strength = bloomRS->GetStrength();
        if (ImGui::InputFloat("Strength##Bloom", &strength))
        {
            strength = alm::saturate(internalBlend);
            bloomRS->SetStrength(strength);
        }

        // Calc whats the max mip chain
        alm::rhi::TextureHandle tex = m_UI.GetUIRenderView()->GetRenderGraph()->GetTexture("SceneColor");
        int maxMips = static_cast<int>(std::floor(std::log2((std::max)(tex->GetDesc().width, tex->GetDesc().height))));

        int mipChainL = bloomRS->GetMipChainLength();
        if (ImGui::SliderInt("Mip Chain Length##Bloom", &mipChainL, 1, maxMips))
        {
            bloomRS->SetMipChainLength(mipChainL);
        }
    }
}

void alm::fw::SettingsWindow::BuildExposureSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Exposure"))
    {
        auto& exposure = m_UI.FrameworkData.Exposure;

        // Luminance

        const float minLogLuminance = -20.f;
        const float maxLogLuminance = 0.f;
        ImGui::SliderScalar("Min log luminance", ImGuiDataType_Float, &exposure.MinLogLuminance,
            &minLogLuminance, &maxLogLuminance, "%.3f");

        const float minLogRange = 0.f;
        const float maxLogRange = 36.f;
        ImGui::SliderScalar("Log luminance range", ImGuiDataType_Float, &exposure.LogLuminanceRange,
            &minLogRange, &maxLogRange, "%.3f");

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        const float minLum = exp2(exposure.MinLogLuminance);
        const float maxLum = minLum * std::exp2(exposure.LogLuminanceRange);
        ImGui::Text("Luminance [%1.6f .. %1.6f]", minLum, maxLum);
        ImGui::PopStyleColor();

        ImGui::Spacing();

        ImGui::SliderFloat("Middle Gray Nits", &exposure.MiddleGrayNits, 0.f, 1000.f);

        const float minExposureBias = 0.f;
        const float maxExposureBias = 1.f;
        ImGui::SliderScalar("SDR Exposure Bias", ImGuiDataType_Float, &exposure.SdrExposureBias,
            &minExposureBias, &maxExposureBias, "%.3f");

        ImGui::Spacing();

        ImGui::InputFloat("Dark to light adaptation speed", &exposure.AdaptationUpSpeed);
        ImGui::InputFloat("Light to dark adaptation speed", &exposure.AdaptationDownSpeed);

        ImGui::Spacing();

        if (ImGui::Button("View Histogram"))
        {
            if (auto* histogram = m_UI.GetWindow<LuminanceHistogramWindow>())
                histogram->SetOpen(true);
        }
    }
}

void alm::fw::SettingsWindow::BuildTonemappingSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Tonemapping"))
    {
        ImGui::Checkbox("Enabled##Tonemapping", &m_UI.FrameworkData.Tonemapping.Enabled);

        alm::rhi::ColorSpace colorSpace = m_UI.GetDeviceManager()->GetColorSpace();
        ImGui::SameLine(0.f, availWidth / 8);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        TextRightAligned("Color Space: %s", colorSpace == alm::rhi::ColorSpace::HDR10_ST2084 ? "HDR10" : "SDR");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SliderFloat("Paper White Nits", &m_UI.FrameworkData.Tonemapping.PaperWhiteNits, 0.f, 1000.f);
    }
}

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
