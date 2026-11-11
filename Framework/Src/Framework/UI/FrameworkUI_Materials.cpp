// "Materials Panel" window (View > Material Panel): material list with name
// filter on the left, material editor on the right.

#include "Framework/FrameworkPCH.h"
#include "Framework/UI/MaterialsWindow.h"
#include "Framework/UI/FrameworkUI.h"
#include "Gfx/GpuSceneBuffers.h"
#include "Gfx/Material.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/MaterialManager.h"

namespace
{

    static bool IsTransmissionDomain(alm::gfx::MaterialDomain domain)
    {
        switch (domain)
        {
        case alm::gfx::MaterialDomain::OpaqueTransmission:
        case alm::gfx::MaterialDomain::AlphaTestedTransmission:
        case alm::gfx::MaterialDomain::AlphaBlendedTransmission:
            return true;
        default:
            return false;
        }
    }

} // anonymous namespace

void alm::fw::MaterialsWindow::SelectMaterial(alm::gfx::Material* material)
{
    m_SelectedMaterial = material;
    SetOpen(true);
    m_ScrollToSelectedMaterial = true;
}

void alm::fw::MaterialsWindow::Build()
{
    auto* deviceManager = m_UI.GetDeviceManager();
    auto* materialManager = deviceManager->GetMaterialManager();

    if (!m_Open)
        return;

    ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_Once);
    if (!ImGui::Begin("Materials Panel", &m_Open, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    // Left panel
    if (ImGui::BeginChild("##materials_panel_left", ImVec2(400, 0), ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders))
    {
        m_Filter.Draw("Filter");

        std::vector<gfx::MaterialRef> mats;
        mats.reserve(materialManager->GetMaterialCount());
        bool validSelectedMat = false;
        for (int i = 0; i < materialManager->GetMaterialCount(); ++i)
        {
            gfx::MaterialRef matRef = materialManager->GetMaterial(i);
            if (!m_Filter.PassFilter(matRef.GetMaterial()->Name.c_str()))
                continue;

            mats.push_back(matRef);
            validSelectedMat |= (m_SelectedMaterial == matRef.GetMaterial());
        }
        if (!validSelectedMat)
            m_SelectedMaterial = nullptr;

        ImGui::Text("Material count: %d / %d", mats.size(), materialManager->GetMaterialCount());

        ImGui::Separator();

        // Table
        if (ImGui::BeginTable("##list", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable))
        {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Filename");
            ImGui::TableSetupColumn("Ref Count");
            ImGui::TableHeadersRow();

            ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();
            if (sortSpecs && sortSpecs->Specs->SortDirection != ImGuiSortDirection_None)
            {
                switch (sortSpecs->Specs->ColumnIndex)
                {
                case 0:
                    std::ranges::sort(mats, [&sortSpecs](const gfx::MaterialRef& l, const gfx::MaterialRef& r)
                        {
                            return sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending ?
                                l.GetMaterial()->Name < r.GetMaterial()->Name :
                                l.GetMaterial()->Name >= r.GetMaterial()->Name;
                        });
                    break;
                case 1:
                    std::ranges::sort(mats, [&sortSpecs](const gfx::MaterialRef& l, const gfx::MaterialRef& r)
                        {
                            return sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending ?
                                l.GetMaterial()->SourceFileName < r.GetMaterial()->SourceFileName :
                                l.GetMaterial()->SourceFileName >= r.GetMaterial()->SourceFileName;
                        });
                    break;
                case 2:
                    std::ranges::sort(mats, [&sortSpecs](const gfx::MaterialRef& l, const gfx::MaterialRef& r)
                        {
                            return sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending ?
                                l.GetRefCount() < r.GetRefCount() : l.GetRefCount() >= r.GetRefCount();
                        });
                    break;
                }
            }

            for (const auto& matRef : mats)
            {
                auto* mat = matRef.GetMaterial();
                if (!mat)
                    continue;

                ImGui::PushID(mat);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                bool isSelected = (m_SelectedMaterial == mat);
                if (ImGui::Selectable(mat->Name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
                {
                    m_SelectedMaterial = mat;
                }

                if (m_ScrollToSelectedMaterial && isSelected)
                {
                    ImGui::SetScrollHereY(0.5f);
                    m_ScrollToSelectedMaterial = false;
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(mat->SourceFileName.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%d", matRef.GetRefCount() - 1);

                ImGui::PopID();
            }

            ImGui::EndTable();
            m_ScrollToSelectedMaterial = false;
        }
        ImGui::EndChild();
    }

    // Right panel
    ImGui::SameLine();
    if (ImGui::BeginChild("##materials_panel_right", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_AlwaysVerticalScrollbar))
    {
        if (auto* mat = m_SelectedMaterial)
        {
            ImGui::Text("%s", mat->Name.c_str());
            ImGui::Separator();

            bool matDirty = false;

            int domain = (int)mat->Domain;
            if (ImGui::Combo("Domain##mat", &domain,
                "Opaque\0Alpha Tested\0Alpha Blended\0"
                "Opaque Transmission\0Alpha Tested Transmission\0Alpha Blended Transmission\0"
                "Terrain\0\0"))
            {
                mat->Domain = (gfx::MaterialDomain)domain;
                matDirty = true;
            }

            int cullMode = (int)mat->CullMode;
            if (ImGui::Combo("Cull Mode##mat", &cullMode,
                "Back\0Front\0None\0\0"))
            {
                mat->CullMode = (rhi::CullMode)cullMode;
                matDirty = true;
            }

            float3 baseColor = mat->BaseColor;
            if (ImGui::ColorEdit3("Base Color##mat", &baseColor.x, ImGuiColorEditFlags_Float))
            {
                mat->BaseColor = baseColor;
                matDirty = true;
            }

            float3 emissiveColor = mat->EmissiveColor;
            if (ImGui::ColorEdit3("Emissive Color##mat", &emissiveColor.x, ImGuiColorEditFlags_Float))
            {
                mat->EmissiveColor = emissiveColor;
                matDirty = true;
            }

            float opacity = mat->Opacity;
            if (ImGui::SliderFloat("Opacity##mat", &opacity, 0.f, 1.f))
            {
                mat->Opacity = opacity;
                matDirty = true;
            }

            float metallic = mat->MetallicFactor;
            if (ImGui::SliderFloat("Metallic##mat", &metallic, 0.f, 1.f))
            {
                mat->MetallicFactor = metallic;
                matDirty = true;
            }

            float roughness = mat->RoughnessFactor;
            if (ImGui::SliderFloat("Roughness##mat", &roughness, 0.f, 1.f))
            {
                mat->RoughnessFactor = roughness;
                matDirty = true;
            }

            float alphaCutoff = mat->AlphaCutoff;
            if (ImGui::SliderFloat("Alpha Cutoff##mat", &alphaCutoff, 0.f, 1.f))
            {
                mat->AlphaCutoff = alphaCutoff;
                matDirty = true;
            }

            if (IsTransmissionDomain(mat->Domain))
            {
                float transmission = mat->TransmissionFactor;
                if (ImGui::SliderFloat("Transmission Factor##mat", &transmission, 0.f, 1.f))
                {
                    mat->TransmissionFactor = transmission;
                    matDirty = true;
                }
            }

            auto baseColorTexture = mat->BaseColorTexture;
            if (m_UI.BuildTextureShowBrowseClear("Base Color Texture", baseColorTexture,
                alm::gfx::TextureLoadFlags::ForceSRGB | alm::gfx::TextureLoadFlags::GenerateMips))
            {
                mat->BaseColorTexture = baseColorTexture;
                matDirty = true;
            }

            auto metalRoughTexture = mat->MetalRoughTexture;
            if (m_UI.BuildTextureShowBrowseClear("Metal-Rough Texture", metalRoughTexture,
                alm::gfx::TextureLoadFlags::GenerateMips))
            {
                mat->MetalRoughTexture = metalRoughTexture;
                matDirty = true;
            }

            auto normalTexture = mat->NormalTexture;
            if (m_UI.BuildTextureShowBrowseClear("Normal Texture", normalTexture,
                alm::gfx::TextureLoadFlags::IsNormalMap | alm::gfx::TextureLoadFlags::GenerateMips))
            {
                mat->NormalTexture = normalTexture;
                matDirty = true;
            }

            auto emissiveTexture = mat->EmissiveTexture;
            if (m_UI.BuildTextureShowBrowseClear("Emissive Texture", emissiveTexture,
                alm::gfx::TextureLoadFlags::GenerateMips))
            {
                mat->EmissiveTexture = emissiveTexture;
                matDirty = true;
            }

            auto occlusionTexture = mat->OcclusionTexture;
            if (m_UI.BuildTextureShowBrowseClear("Occlusion Texture", occlusionTexture,
                alm::gfx::TextureLoadFlags::GenerateMips))
            {
                mat->OcclusionTexture = occlusionTexture;
                matDirty = true;
            }

            auto transmissionTexture = mat->TransmissionTexture;
            if (m_UI.BuildTextureShowBrowseClear("Transmission Texture", transmissionTexture,
                alm::gfx::TextureLoadFlags::GenerateMips))
            {
                mat->TransmissionTexture = transmissionTexture;
                matDirty = true;
            }

            if (matDirty)
            {
                deviceManager->GetGpuSceneBuffers()->SetDirtyMaterial(mat);
            }
        }
        ImGui::EndChild();
    }

    ImGui::End();
}
