#pragma once

namespace alm::gfx
{
	class ImGuiRenderStage;
	class DeviceManager;

	void InitImGuiViewportsRenderer(std::shared_ptr<alm::gfx::ImGuiRenderStage> mainRenderStage, alm::gfx::DeviceManager* deviceManager);
	void ReleaseImGuiViewportsRenderer();

} // namespace st::gfx