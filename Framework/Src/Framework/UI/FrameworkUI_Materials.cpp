// FrameworkUI "Material Panel" window (View > Material Panel).
// This is part of the alm::fw::FrameworkUI class: its member functions are split
// across several FrameworkUI_*.cpp files, grouped by UI window.

#include "Framework/FrameworkPCH.h"
#include "Framework/UI/FrameworkUI.h"
#include "Gfx/GpuSceneBuffers.h"
#include "Gfx/Material.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/MaterialManager.h"

void alm::fw::FrameworkUI::BuildMaterialsWindow()
{
    auto* deviceManager = GetDeviceManager();
    auto* materialManager = deviceManager->GetMaterialManager();

    if (!m_ShowMaterials)
        return;

    ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_Once);
    if (!ImGui::Begin("Materials Panel", &m_ShowMaterials, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    // Left panel
    if (ImGui::BeginChild("##materials_panel_left", ImVec2(400, 0), ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders))
    {
        m_MaterialFilter.Draw("Filter");

        std::vector<gfx::MaterialRef> mats;
        mats.reserve(materialManager->GetMaterialCount());
        bool validSelectedMat = false;
        for (int i = 0; i < materialManager->GetMaterialCount(); ++i)
        {
            gfx::MaterialRef matRef = materialManager->GetMaterial(i);
            if (!m_MaterialFilter.PassFilter(matRef.GetMaterial()->GetName().c_str()))
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
                                l.GetMaterial()->GetName() < r.GetMaterial()->GetName() :
                                l.GetMaterial()->GetName() >= r.GetMaterial()->GetName();
                        });
                    break;
                case 1:
                    std::ranges::sort(mats, [&sortSpecs](const gfx::MaterialRef& l, const gfx::MaterialRef& r)
                        {
                            return sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending ?
                                l.GetMaterial()->GetSourceFilename() < r.GetMaterial()->GetSourceFilename() :
                                l.GetMaterial()->GetSourceFilename() >= r.GetMaterial()->GetSourceFilename();
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

            for(const auto& matRef : mats)
            {
                auto* mat = matRef.GetMaterial();                
                if (!mat)
                    continue;

                ImGui::PushID(mat);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                bool isSelected = (m_SelectedMaterial == mat);
                if (ImGui::Selectable(mat->GetName().c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
                {
                    m_SelectedMaterial = mat;
                }

                if (m_ScrollToSelectedMaterial && isSelected)
                {
                    ImGui::SetScrollHereY(0.5f);
                    m_ScrollToSelectedMaterial = false;
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(mat->GetSourceFilename().c_str());

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
            ImGui::Text("%s", mat->GetName().c_str());
            ImGui::Separator();

            bool matDirty = false;

            int domain = (int)mat->GetDomain();
            if (ImGui::Combo("Domain##mat", &domain,
                "Opaque\0Alpha Tested\0Alpha Blended\0\0"))
            {
                mat->SetDomain((gfx::MaterialDomain)domain);
                matDirty = true;
            }
            
            int cullMode = (int)mat->GetCullMode();
            if (ImGui::Combo("Cull Mode##mat", &cullMode,
                "Back\0Front\0None\0\0"))
            {
                mat->SetCullMode((rhi::CullMode)cullMode);
                matDirty = true;
            }

            float3 baseColor = mat->GetBaseColor();
            if (ImGui::ColorEdit3("Base Color##mat", &baseColor.x, ImGuiColorEditFlags_Float))
            {
                mat->SetBaseColor(baseColor);
                matDirty = true;
            }

            float3 emissiveColor = mat->GetEmissiveColor();
            if (ImGui::ColorEdit3("Emissive Color##mat", &emissiveColor.x, ImGuiColorEditFlags_Float))
            {
                mat->SetEmissiveColor(emissiveColor);
                matDirty = true;
            }

            float opacity = mat->GetOpacity();
            if (ImGui::SliderFloat("Opacity##mat", &opacity, 0.f, 1.f))
            {
                mat->SetOpacity(opacity);
                matDirty = true;
            }

            float metallic = mat->GetMetallicFactor();
            if (ImGui::SliderFloat("Metallic##mat", &metallic, 0.f, 1.f))
            {
                mat->SetMetallicFactor(metallic);
                matDirty = true;
            }

            float roughness = mat->GetRoughnessFactor();
            if (ImGui::SliderFloat("Roughness##mat", &roughness, 0.f, 1.f))
            {
                mat->SetRoughnessFactor(roughness);
                matDirty = true;
            }

            float alphaCutoff = mat->GetAlphaCutoff();
            if (ImGui::SliderFloat("Alpha Cutoff##mat", &alphaCutoff, 0.f, 1.f))
            {
                mat->SetAlphaCutoff(alphaCutoff);
                matDirty = true;
            }

            auto baseColorTexture = mat->GetBaseColorTexture();
            if (BuildTextureShowOpenClear("Base Color Texture", baseColorTexture, true, false))
            {
                mat->SetBaseColorTexture(baseColorTexture);
                matDirty = true;
            }

            auto metalRoughTexture = mat->GetMetalRoughTexture();
            if (BuildTextureShowOpenClear("Metal-Rough Texture", metalRoughTexture, false, false))
            {
                mat->SetMetalRoughTexture(metalRoughTexture);
                matDirty = true;
            }

            auto normalTexture = mat->GetNormalTexture();
            if (BuildTextureShowOpenClear("Normal Texture", normalTexture, false, true))
            {
                mat->SetNormalTexture(normalTexture);
                matDirty = true;
            }

            auto emissiveTexture = mat->GetEmissiveTexture();
            if (BuildTextureShowOpenClear("Emissive Texture", emissiveTexture, false, false))
            {
                mat->SetEmissiveTexture(emissiveTexture);
                matDirty = true;
            }

            auto occlusionTexture = mat->GetOcclusionTexture();
            if (BuildTextureShowOpenClear("Occlusion Texture", occlusionTexture, false, false))
            {
                mat->SetOcclusionTexture(occlusionTexture);
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

