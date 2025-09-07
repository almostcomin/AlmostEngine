#include "UI/ImGuiRenderPass.h"
#include "Core/Log.h"
#include "Graphics/DeviceManager.h"
#include "Graphics/ShaderFactory.h"
#include <imgui/imgui.h>
//#include <imgui/imgui_impl_sdl3.h>

bool st::ui::ImGuiRenderPass::Init(SDL_Window* window, st::gfx::DeviceManager* deviceManager, st::gfx::ShaderFactory* shaderFactory)
{
    m_Device = device;

	//ImGui::CreateContext();

	//m_commandList = device->createCommandList();

	m_VS = shaderFactory->CreateShader("./Shaders/imgui_vs.vso", nvrhi::ShaderType::Vertex);
	m_PS = shaderFactory->CreateShader("./Shaders/imgui_ps.pso", nvrhi::ShaderType::Pixel);
	if (!m_VS || !m_PS)
	{
		st::log::Error("Failed to create ImGui shaders");
		return false;
	}

	// create attribute layout object
	nvrhi::VertexAttributeDesc vertexAttribLayout[] = {
		{ "POSITION", nvrhi::Format::RG32_FLOAT,  1, 0, offsetof(ImDrawVert,pos), sizeof(ImDrawVert), false },
		{ "TEXCOORD", nvrhi::Format::RG32_FLOAT,  1, 0, offsetof(ImDrawVert,uv),  sizeof(ImDrawVert), false },
		{ "COLOR",    nvrhi::Format::RGBA8_UNORM, 1, 0, offsetof(ImDrawVert,col), sizeof(ImDrawVert), false },
	};
	m_ShaderAttribLayout = m_Device->createInputLayout(vertexAttribLayout, sizeof(vertexAttribLayout) / sizeof(vertexAttribLayout[0]), m_VS);

    // Create PSO
    {
        nvrhi::BlendState blendState;
        blendState.targets[0].setBlendEnable(true)
            .setSrcBlend(nvrhi::BlendFactor::SrcAlpha)
            .setDestBlend(nvrhi::BlendFactor::InvSrcAlpha)
            .setSrcBlendAlpha(nvrhi::BlendFactor::InvSrcAlpha)
            .setDestBlendAlpha(nvrhi::BlendFactor::Zero);

        auto rasterState = nvrhi::RasterState()
            .setFillSolid()
            .setCullNone()
            .setScissorEnable(true)
            .setDepthClipEnable(true);

        auto depthStencilState = nvrhi::DepthStencilState()
            .disableDepthTest()
            .enableDepthWrite()
            .disableStencil()
            .setDepthFunc(nvrhi::ComparisonFunc::Always);

        nvrhi::RenderState renderState;
        renderState.blendState = blendState;
        renderState.depthStencilState = depthStencilState;
        renderState.rasterState = rasterState;

        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.visibility = nvrhi::ShaderType::All;
        layoutDesc.bindings = {
            nvrhi::BindingLayoutItem::PushConstants(0, sizeof(float) * 2),
            nvrhi::BindingLayoutItem::Texture_SRV(0),
            nvrhi::BindingLayoutItem::Sampler(0)
        };
        m_BindingLayout = m_Device->createBindingLayout(layoutDesc);

        m_BasePSODesc.primType = nvrhi::PrimitiveType::TriangleList;
        m_BasePSODesc.inputLayout = m_ShaderAttribLayout;
        m_BasePSODesc.VS = m_VS;
        m_BasePSODesc.PS = m_PS;
        m_BasePSODesc.renderState = renderState;
        m_BasePSODesc.bindingLayouts = { m_BindingLayout };
    }

    {
        const auto desc = nvrhi::SamplerDesc()
            .setAllAddressModes(nvrhi::SamplerAddressMode::Wrap)
            .setAllFilters(true);
        m_FontSampler = m_Device->createSampler(desc);
    }

	return true;
}


void st::ui::ImGuiRenderPass::Render()
{
	ImDrawData* drawData = ImGui::GetDrawData();
	const auto& io = ImGui::GetIO();

    int w, h;
    m_DeviceManager->GetWindowDimensions();

    io.DisplaySize = ImVec2(float(w), float(h));
    if (!m_supportExplicitDisplayScaling)
    {
        io.DisplayFramebufferScale.x = scaleX;
        io.DisplayFramebufferScale.y = scaleY;
    }
}