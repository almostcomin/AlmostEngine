#include "UI/ImGuiRenderPass.h"
#include "Core/Log.h"
#include "Graphics/DeviceManager.h"
#include "Graphics/ShaderFactory.h"
#include <imgui/imgui.h>
//#include <imgui/imgui_impl_sdl3.h>

bool st::ui::ImGuiRenderPass::Init(SDL_Window* window, st::gfx::DeviceManager* deviceManager, st::gfx::ShaderFactory* shaderFactory)
{
    m_DeviceManager = deviceManager;

	m_CommandList = m_DeviceManager->GetDevice()->createCommandList();

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
	m_ShaderAttribLayout = m_DeviceManager->GetDevice()->createInputLayout(vertexAttribLayout, sizeof(vertexAttribLayout) / sizeof(vertexAttribLayout[0]), m_VS);

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
        m_BindingLayout = m_DeviceManager->GetDevice()->createBindingLayout(layoutDesc);

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
        m_FontSampler = m_DeviceManager->GetDevice()->createSampler(desc);
    }

    ImGui::CreateContext();

    UpdateFontTexture();

	return true;
}

void st::ui::ImGuiRenderPass::ReconcileInputState()
{
    ImGuiIO& io = ImGui::GetIO();

    // Reconcile mouse
    for (size_t i = 0; i < m_CachedMouseButtons.size(); i++)
    {
        if (io.MouseDown[i] == true && m_CachedMouseButtons[i] == KeyAction::RELEASE)
        {
            io.MouseDown[i] = false;
        }
    }
}

void st::ui::ImGuiRenderPass::BeginFullScreenWindow()
{
    ImGuiIO const& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(
        io.DisplaySize.x / io.DisplayFramebufferScale.x,
        io.DisplaySize.y / io.DisplayFramebufferScale.y),
        ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin(" ", 0, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
}

void st::ui::ImGuiRenderPass::EndFullScreenWindow()
{
    ImGui::End();
    ImGui::PopStyleVar();
}

void st::ui::ImGuiRenderPass::DrawCenteredText(const char* text)
{
    ImGuiIO const& io = ImGui::GetIO();
    ImVec2 textSize = ImGui::CalcTextSize(text);

    ImVec2 vMin = ImGui::GetCursorScreenPos();
    ImVec2 vMax = ImGui::GetContentRegionAvail();

    ImGui::SetCursorScreenPos(ImVec2{ (vMax.x - vMin.x - textSize.x) * 0.5f, (vMax.y - vMin.y - textSize.y) * 0.5f });
    ImGui::TextUnformatted(text);
}

bool st::ui::ImGuiRenderPass::Render(nvrhi::IFramebuffer* frameBuffer)
{
	auto& io = ImGui::GetIO();

    glm::ivec2 dim = m_DeviceManager->GetWindowDimensions();
    io.DisplaySize = ImVec2(dim.x, dim.y);

    ImGui::NewFrame();
    BuildUI();
    ImGui::Render();

    ReconcileInputState();

    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData->TotalIdxCount == 0)
        return true; // nothing to render

    m_CommandList->open();

    if (!UpdateGeometry())
    {
        m_CommandList->close();
        return false;
    }

    m_CommandList->clearTextureFloat(m_DeviceManager->GetCurrentBackBuffer(), nvrhi::AllSubresources, nvrhi::Color(0.f));

    // Handle DPI scaling
    drawData->ScaleClipRects(io.DisplayFramebufferScale);

    float invDisplaySize[2] = { 1.f / io.DisplaySize.x, 1.f / io.DisplaySize.y };

    // Set up graphics state
    nvrhi::GraphicsState drawState;
    drawState.framebuffer = frameBuffer;
    assert(drawState.framebuffer);
    drawState.pipeline = GetPSO(drawState.framebuffer);
    drawState.viewport.viewports.push_back(nvrhi::Viewport{
        io.DisplaySize.x * io.DisplayFramebufferScale.x,
        io.DisplaySize.y * io.DisplayFramebufferScale.y });
    drawState.viewport.scissorRects.resize(1);

    nvrhi::VertexBufferBinding vbufBinding;
    vbufBinding.buffer = m_VertexBuffer;
    vbufBinding.slot = 0;
    vbufBinding.offset = 0;
    drawState.vertexBuffers.push_back(vbufBinding);

    drawState.indexBuffer.buffer = m_IndexBuffer;
    drawState.indexBuffer.format = (sizeof(ImDrawIdx) == 2 ? nvrhi::Format::R16_UINT : nvrhi::Format::R32_UINT);
    drawState.indexBuffer.offset = 0;

    // Render command lists
    m_CommandList->beginMarker("ImGui");
    int vtxOffset = 0;
    int idxOffset = 0;
    for (int n = 0; n < drawData->CmdLists.Size; n++)
    {
        const ImDrawList* cmdList = drawData->CmdLists[n];
        for (int i = 0; i < cmdList->CmdBuffer.Size; i++)
        {
            const ImDrawCmd* pCmd = &cmdList->CmdBuffer[i];

            if (pCmd->UserCallback)
            {
                pCmd->UserCallback(cmdList, pCmd);
            }
            else 
            {
                drawState.bindings = { GetBindingSet((nvrhi::ITexture*)pCmd->GetTexID()) };
                assert(drawState.bindings[0]);

                drawState.viewport.scissorRects[0] = nvrhi::Rect(int(pCmd->ClipRect.x),
                    int(pCmd->ClipRect.z),
                    int(pCmd->ClipRect.y),
                    int(pCmd->ClipRect.w));

                nvrhi::DrawArguments drawArguments;
                drawArguments.vertexCount = pCmd->ElemCount;
                drawArguments.startIndexLocation = idxOffset;
                drawArguments.startVertexLocation = vtxOffset;

                m_CommandList->setGraphicsState(drawState);
                m_CommandList->setPushConstants(invDisplaySize, sizeof(invDisplaySize));
                m_CommandList->drawIndexed(drawArguments);
            }
            idxOffset += pCmd->ElemCount;
        }
        vtxOffset += cmdList->VtxBuffer.Size;
    }

    m_CommandList->endMarker();
    m_CommandList->close();
    m_DeviceManager->GetDevice()->executeCommandList(m_CommandList);

    return true;
}

void st::ui::ImGuiRenderPass::OnBackbufferResize(const glm::ivec2& size)
{
    // no-op
}

bool st::ui::ImGuiRenderPass::OnMouseMove(float posX, float posY)
{
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos.x = float(posX);
    io.MousePos.y = float(posY);

    return io.WantCaptureMouse;
}

bool st::ui::ImGuiRenderPass::OnMouseButtonUpdate(MouseButton button, KeyAction action)
{
    auto& io = ImGui::GetIO();

    int buttonIndex = 0;
    switch (button)
    {
    case MouseButton::LEFT:
        buttonIndex = 0;
        break;
    case MouseButton::RIGHT:
        buttonIndex = 1;
        break;
    case MouseButton::MIDDLE:
        buttonIndex = 2;
        break;
    }
   
    // Update internal state
    assert(buttonIndex < m_CachedMouseButtons.max_size());
    m_CachedMouseButtons[buttonIndex] = action;

    if (action == KeyAction::PRESS)
    {
        io.MouseDown[buttonIndex] = true;
    }
    else
    {
        // for mouse up events, ImGui state is only updated after the next frame
        // this ensures that short clicks are not missed
    }

    return io.WantCaptureMouse;
}

bool st::ui::ImGuiRenderPass::UpdateFontTexture()
{
    ImGuiIO& io = ImGui::GetIO();

    // If the font texture exists and is bound to ImGui, we're done.
    // Note: ImGui_Renderer will reset io.Fonts->TexID when new fonts are added.
    if (m_FontTexture && io.Fonts->TexRef.GetTexID())
        return true;

    // Get font texture atlas memory
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    if (!pixels)
        return false;

    nvrhi::TextureDesc textureDesc;
    textureDesc.width = width;
    textureDesc.height = height;
    textureDesc.format = nvrhi::Format::RGBA8_UNORM;
    textureDesc.debugName = "ImGui font texture";
    m_FontTexture = m_DeviceManager->GetDevice()->createTexture(textureDesc);
    if (!m_FontTexture)
        return false;

    m_CommandList->open();
    m_CommandList->beginTrackingTextureState(m_FontTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::Common);
    m_CommandList->writeTexture(m_FontTexture, 0, 0, pixels, width * 4);
    m_CommandList->setPermanentTextureState(m_FontTexture, nvrhi::ResourceStates::ShaderResource);
    m_CommandList->commitBarriers();
    m_CommandList->close();

    m_DeviceManager->GetDevice()->executeCommandList(m_CommandList);

    io.Fonts->TexRef = m_FontTexture.Get();

    return true;
}

bool st::ui::ImGuiRenderPass::UpdateGeometry()
{
    const ImDrawData* drawData = ImGui::GetDrawData();

    // Create/resize vertex and index buffers if needed
    if (!ReallocateBuffer(m_VertexBuffer,
        drawData->TotalVtxCount * sizeof(ImDrawVert),
        (drawData->TotalVtxCount + 5000) * sizeof(ImDrawVert),
        false))
    {
        return false;
    }
    if (!ReallocateBuffer(m_IndexBuffer,
        drawData->TotalIdxCount * sizeof(ImDrawIdx),
        (drawData->TotalIdxCount + 5000) * sizeof(ImDrawIdx),
        true))
    {
        return false;
    }

    // Copy and convert all vertices into a single contiguous buffer
    m_VertexData.resize(drawData->TotalVtxCount);
    m_IndexData.resize(drawData->TotalIdxCount);
    ImDrawVert* vtxDst = m_VertexData.data();
    ImDrawIdx* idxDst = m_IndexData.data();

    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* cmdList = drawData->CmdLists[n];

        memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

        vtxDst += cmdList->VtxBuffer.Size;
        idxDst += cmdList->IdxBuffer.Size;
    }

    m_CommandList->writeBuffer(m_VertexBuffer, m_VertexData.data(), m_VertexData.size() * sizeof(ImDrawVert));
    m_CommandList->writeBuffer(m_IndexBuffer, m_IndexData.data(), m_IndexData.size() * sizeof(ImDrawIdx));

    return true;
}

bool st::ui::ImGuiRenderPass::ReallocateBuffer(nvrhi::BufferHandle& buffer, size_t requiredSize, size_t reallocateSize, const bool indexBuffer)
{
    if (buffer == nullptr || size_t(buffer->getDesc().byteSize) < requiredSize)
    {
        nvrhi::BufferDesc desc;
        desc.byteSize = uint32_t(reallocateSize);
        desc.structStride = 0;
        desc.debugName = indexBuffer ? "ImGui index buffer" : "ImGui vertex buffer";
        desc.canHaveUAVs = false;
        desc.isVertexBuffer = !indexBuffer;
        desc.isIndexBuffer = indexBuffer;
        desc.isDrawIndirectArgs = false;
        desc.isVolatile = false;
        desc.initialState = indexBuffer ? nvrhi::ResourceStates::IndexBuffer : nvrhi::ResourceStates::VertexBuffer;
        desc.keepInitialState = true;

        buffer = m_DeviceManager->GetDevice()->createBuffer(desc);
        if (!buffer)
            return false;
    }
    return true;
}

nvrhi::GraphicsPipelineHandle st::ui::ImGuiRenderPass::GetPSO(nvrhi::IFramebuffer* frameBuffer)
{
    if (!m_PSO)
    {
        m_PSO = m_DeviceManager->GetDevice()->createGraphicsPipeline(m_BasePSODesc, frameBuffer);
        assert(m_PSO);
    }
    return m_PSO;
}

nvrhi::IBindingSet* st::ui::ImGuiRenderPass::GetBindingSet(nvrhi::ITexture* texture)
{
    auto iter = m_BindingsCache.find(texture);
    if (iter != m_BindingsCache.end())
    {
        return iter->second;
    }

    nvrhi::BindingSetDesc desc;
    desc.bindings = {
        nvrhi::BindingSetItem::PushConstants(0, sizeof(float) * 2),
        nvrhi::BindingSetItem::Texture_SRV(0, texture),
        nvrhi::BindingSetItem::Sampler(0, m_FontSampler)
    };

    nvrhi::BindingSetHandle binding;
    binding = m_DeviceManager->GetDevice()->createBindingSet(desc, m_BindingLayout);
    assert(binding);

    m_BindingsCache[texture] = binding;
    return binding;
}
