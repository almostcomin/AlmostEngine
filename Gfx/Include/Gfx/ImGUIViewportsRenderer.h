#pragma once

namespace st::gfx
{
	class ImGuiRenderStage;
	class DeviceManager;

	void InitImGuiViewportsRenderer(std::shared_ptr<st::gfx::ImGuiRenderStage> mainRenderStage, st::gfx::DeviceManager* deviceManager);
	void ReleaseImGuiViewportsRenderer();

} // namespace st::gfx