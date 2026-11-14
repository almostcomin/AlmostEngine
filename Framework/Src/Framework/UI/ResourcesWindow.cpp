// "Resources Inspector" window: resource list with name filter, per-type
// checkboxes and per-row inspect shortcuts (double click / Inspect button).
// Textures and buffers open their inspection windows via FrameworkUI.

#include "Framework/FrameworkPCH.h"
#include "Framework/UI/ResourcesWindow.h"
#include "Framework/UI/FrameworkUI.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/RenderView.h"
#include "RHI/Device.h"
#include <imgui/imgui_internal.h> // For ImGuiItemFlags_MixedValue

namespace
{
	constexpr uint32_t kAllTypesMask = (1u << (size_t)alm::rhi::ResourceType::_Count) - 1;
	constexpr size_t kTypeFilterColumns = 3;
	constexpr size_t kTypeFilterRows = ((size_t)alm::rhi::ResourceType::_Count + kTypeFilterColumns - 1) / kTypeFilterColumns;

	void BuildTypeFilter(uint32_t& visibleTypeMask)
	{
		// Tri-state "All": checked = all visible, unchecked = none, mixed = partial.
		const bool allOn = (visibleTypeMask & kAllTypesMask) == kAllTypesMask;
		const bool allOff = visibleTypeMask == 0;
		ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, !allOn && !allOff);
		bool allValue = allOn;
		if (ImGui::Checkbox("All", &allValue))
			visibleTypeMask = allOn ? 0u : kAllTypesMask;
		ImGui::PopItemFlag();

		if (ImGui::BeginTable("##type_filter", (int)kTypeFilterColumns))
		{
			for (size_t row = 0; row < kTypeFilterRows; ++row)
			{
				ImGui::TableNextRow();
				for (size_t col = 0; col < kTypeFilterColumns; ++col)
				{
					const size_t typeIdx = row + col * kTypeFilterRows;
					ImGui::TableNextColumn();
					if (typeIdx >= (size_t)alm::rhi::ResourceType::_Count)
						continue;

					const auto type = (alm::rhi::ResourceType)typeIdx;
					const uint32_t bit = 1u << typeIdx;

					bool visible = visibleTypeMask & bit;
					if (ImGui::Checkbox(alm::rhi::ResourceTypeToString(type), &visible))
						visibleTypeMask = visible ? (visibleTypeMask | bit) : (visibleTypeMask & ~bit);

					// Right click: show only this type.
					if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
						visibleTypeMask = bit;
				}
			}
			ImGui::EndTable();
		}
	}
} // anonymous namespace

void alm::fw::ResourcesWindow::Inspect(rhi::IResource* res)
{
	switch (res->GetResourceType())
	{
	case rhi::ResourceType::Texture:
		m_UI.AddTextureWindow(alm::checked_pointer_cast<rhi::ITexture>(res->weak_from_this()));
		break;

	case rhi::ResourceType::Buffer:
		m_UI.AddBufferWindow(alm::checked_pointer_cast<rhi::IBuffer>(res->weak_from_this()));
		break;

	default:
		break;
	}
}

void alm::fw::ResourcesWindow::Build()
{
    auto* deviceManager = m_UI.GetDeviceManager();

    if (!m_Open)
        return;

    alm::gfx::RenderGraph* renderGraph = m_UI.GetUIRenderView()->GetRenderGraph().get();
    const float parentHeight = ImGui::GetIO().DisplaySize.y;
    const float windowHeight = parentHeight * 0.9f;

    ImGui::SetNextWindowSize(ImVec2(900, windowHeight), ImGuiCond_Once);
    if (!ImGui::Begin("Resources Inspector", &m_Open, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    if (ImGui::BeginChild("##resources_panel", ImVec2(0, 0), ImGuiChildFlags_None))
    {
        m_Filter.Draw("Filter");
        ImGui::Spacing();
        BuildTypeFilter(m_VisibleTypeMask);

        ImGui::Separator();

        bool validSelectedResource = false;
        std::vector<std::pair<rhi::IResource*, uint64_t>> resources;
        uint32_t totalResCount = 0;
        deviceManager->GetDevice()->EnumerateLivingResources([&](rhi::IResource* res)
        {
            ++totalResCount;
            if (m_Filter.PassFilter(res->GetDebugName().c_str()) &&
                (m_VisibleTypeMask & (1u << (size_t)res->GetResourceType())))
            {
                resources.emplace_back(res, res->GetSizeInBytes());
                validSelectedResource |= (res == m_SelectedResource);
            }
        });
        if (!validSelectedResource)
            m_SelectedResource = nullptr;

        ImGui::Text("Resources count: %d / %d", (int)resources.size(), (int)totalResCount);

        // Table area: it fills the window except for the bottom row, so the
        // Inspect button below stays visible without scrolling. Scrolling and
        // sorting happen inside this child.
        const float inspectFooterHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y + 2.0f;
        if (ImGui::BeginChild("##list_area", ImVec2(0, -inspectFooterHeight), ImGuiChildFlags_None))
        {
            ImGui::Separator();

            // Table
            if (ImGui::BeginTable("##list", 3,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableHeadersRow();

                ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();
                if (sortSpecs && sortSpecs->SpecsCount > 0 && sortSpecs->Specs->SortDirection != ImGuiSortDirection_None)
                {
                    switch (sortSpecs->Specs->ColumnIndex)
                    {
                    case 0:
                        std::ranges::stable_sort(resources, [&sortSpecs](const auto& l, const auto& r)
                        {
                            return sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending ?
                                l.first->GetDebugName() < r.first->GetDebugName() : l.first->GetDebugName() > r.first->GetDebugName();
                        });
                        break;
                    case 1:
                        std::ranges::stable_sort(resources, [&sortSpecs](const auto& l, const auto& r)
                            {
                                int cmp_result = std::strcmp(
                                    rhi::ResourceTypeToString(l.first->GetResourceType()), rhi::ResourceTypeToString(r.first->GetResourceType()));
                                return sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending ?
                                    cmp_result < 0 : cmp_result > 0;
                            });
                        break;
                    case 2:
                        std::ranges::stable_sort(resources, [&sortSpecs](const auto& l, const auto& r)
                        {
                            return sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending ?
                                l.second < r.second : l.second > r.second;
                        });
                        break;
                    }
                }

                for (const auto& entry : resources)
                {
                    rhi::IResource* res = entry.first;

                    ImGui::PushID(res);
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    bool isSelected = (m_SelectedResource == res);
                    if (ImGui::Selectable(res->GetDebugName().c_str(), isSelected,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        m_SelectedResource = res;
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            Inspect(res);
                    }

                    if (m_ScrollToSelectedResource && isSelected)
                    {
                        ImGui::SetScrollHereY(0.5f);
                        m_ScrollToSelectedResource = false;
                    }

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(rhi::ResourceTypeToString(res->GetResourceType()));

                    ImGui::TableNextColumn();
                    uint64_t sizeBytes = entry.second;

                    if (sizeBytes > MiB(1))
                        ImGui::Text("%1.1f MiB", (float)sizeBytes / MiB(1));
                    else if(sizeBytes > KiB(1))
                        ImGui::Text("%1.1f KiB", (float)sizeBytes / KiB(1));
                    else
                        ImGui::Text("%llu B", (unsigned long long)sizeBytes);

                    ImGui::PopID();
                }

                ImGui::EndTable();
                m_ScrollToSelectedResource = false;
            }
        }
        ImGui::EndChild();

        // Inspect shortcut (always visible): only textures and buffers have
        // inspection windows.
        const bool canInspect = m_SelectedResource &&
            (m_SelectedResource->GetResourceType() == rhi::ResourceType::Texture ||
             m_SelectedResource->GetResourceType() == rhi::ResourceType::Buffer);
        ImGui::Separator();
        ImGui::BeginDisabled(!canInspect);
        if (ImGui::Button("Inspect"))
            Inspect(m_SelectedResource);
        ImGui::EndDisabled();
        if (!canInspect && m_SelectedResource)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(only textures and buffers can be inspected)");
        }

        ImGui::EndChild();
    }

    ImGui::End();
}
