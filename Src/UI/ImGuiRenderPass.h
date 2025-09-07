#pragma once

#include "Graphics/RenderPass.h"
#include <nvrhi/nvrhi.h>

struct SDL_Window;

namespace st::gfx
{
	class DeviceManager;
	class ShaderFactory;
}

namespace st::ui
{

class ImGuiRenderPass : public st::gfx::RenderPass
{
public:

	virtual void Build() = 0;

	void Render() override;

protected:

	bool Init(SDL_Window* window, st::gfx::DeviceManager* deviceManager, st::gfx::ShaderFactory* shaderFactory);

private:

	nvrhi::ShaderHandle m_VS;
	nvrhi::ShaderHandle m_PS;

	nvrhi::InputLayoutHandle m_ShaderAttribLayout;
	nvrhi::BindingLayoutHandle m_BindingLayout;
	nvrhi::GraphicsPipelineDesc m_BasePSODesc;

	nvrhi::TextureHandle m_FontTexture;
	nvrhi::SamplerHandle m_FontSampler;

	nvrhi::CommandListHandle m_CommandList;

	nvrhi::DeviceManager* m_DeviceManager;
};

}