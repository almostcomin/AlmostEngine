#include "UI/ImGuiRenderPass.h"
#include "Core/Log.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/RenderView.h"
#include "Interop/ImGUI_CB.h"
#include "Gfx/DataUploader.h"
#include "RenderAPI/Device.h"
#include <imgui/imgui.h>

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

void st::ui::ImGuiRenderPass::OnAttached()
{
    Init();
}

void st::ui::ImGuiRenderPass::OnDetached()
{
    // TODO
    assert(0);
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

bool st::ui::ImGuiRenderPass::Render(rapi::IFramebuffer* frameBuffer)
{
    st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
    st::rapi::Device* device = deviceManager->GetDevice();
	auto& io = ImGui::GetIO();

    glm::ivec2 dim = deviceManager->GetWindowDimensions();
    io.DisplaySize = ImVec2(dim.x, dim.y);

    m_CommandList->Open();
    m_CommandList->BeginMarker("ImGui");

    m_CommandList->PushBarrier(rapi::Barrier().Texture(
        frameBuffer->GetDesc().ColorAttachments[0].texture, rapi::ResourceState::COMMON, rapi::ResourceState::RENDERTARGET));

    // Address any change in font data. Needs to be called before ImGui::NewFrame()
    if (!UpdateFontTexture())
    {
        LOG_ERROR("Failed to update ImGui font texture");
        m_CommandList->Close();
        return false;
    }

    ImGui::NewFrame();
    BuildUI();
    ImGui::Render();

    ReconcileInputState();

    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData->TotalIdxCount == 0)
        return true; // nothing to render

    // Address any change in geometry data
    if (!UpdateGeometry())
    {
        LOG_ERROR("Failed to update ImGui geometry buffer");
        m_CommandList->Close();
        return false;
    }

    // Handle DPI scaling
    drawData->ScaleClipRects(io.DisplayFramebufferScale);

    float2 invDisplaySize = { 1.f / io.DisplaySize.x, 1.f / io.DisplaySize.y };

    m_CommandList->BeginRenderPass(
        frameBuffer,
        { rapi::RenderPassOp{rapi::RenderPassOp::LoadOp::Clear, rapi::RenderPassOp::StoreOp::Store, rapi::ClearValue::Black} },
        {},
        rapi::RenderPassFlags::None);

    m_CommandList->SetPipelineState(GetPSO(frameBuffer).get());
    
    m_CommandList->SetViewport(rapi::ViewportState().AddViewport({
        io.DisplaySize.x* io.DisplayFramebufferScale.x,
        io.DisplaySize.y* io.DisplayFramebufferScale.y }));

    int idxOffset = 0;
    int vtxOffset = 0;
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
                m_CommandList->SetViewport(rapi::ViewportState().AddViewport({
                    io.DisplaySize.x * io.DisplayFramebufferScale.x,
                    io.DisplaySize.y * io.DisplayFramebufferScale.y }).AddScissorRect({
                    int2 { pCmd->ClipRect.x, pCmd->ClipRect.y }, int2{pCmd->ClipRect.z, pCmd->ClipRect.w} }));

                interop::ImGUI_CB cb = {};
                cb.invDisplaySize = invDisplaySize;
                cb.indexBuffer = m_IndexBuffer->GetDescriptorIndex(rapi::DescriptorType::SRV);
                cb.indexOffset = idxOffset;
                cb.vertexBuffer = m_VertexBuffer->GetDescriptorIndex(rapi::DescriptorType::SRV);
                cb.vertexBufferOffset = vtxOffset;
                cb.textureIndex = m_FontTexture->GetDescriptorIndex(rapi::DescriptorType::SRV);

                m_CommandList->PushConstants(&cb, sizeof(interop::ImGUI_CB), 0);
                
                m_CommandList->Draw(pCmd->ElemCount);

                idxOffset += pCmd->ElemCount;
            }
            vtxOffset += cmdList->VtxBuffer.Size;
        }
    }

    m_CommandList->EndMarker();
    m_CommandList->EndRenderPass();
    m_CommandList->PushBarrier(rapi::Barrier().Texture(
        frameBuffer->GetDesc().ColorAttachments[0].texture, rapi::ResourceState::RENDERTARGET, rapi::ResourceState::COMMON));
    m_CommandList->Close();
    
    device->ExecuteCommandList(m_CommandList.get(), st::rapi::QueueType::Graphics);

#if 0
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
    device->executeCommandList(m_CommandList, nvrhi::CommandQueue::Graphics);
#endif

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

bool st::ui::ImGuiRenderPass::Init()
{
    st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
    st::rapi::Device* device = deviceManager->GetDevice();
    st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();

    m_CommandList = device->CreateCommandList(st::rapi::CommandListParams{ st::rapi::QueueType::Graphics });

    m_VS = shaderFactory->LoadShader("imgui_bindless_vs.vso", rapi::ShaderType::Vertex);
    m_PS = shaderFactory->LoadShader("imgui_bindless_ps.pso", rapi::ShaderType::Pixel);
    if (!m_VS || !m_PS)
    {
        LOG_ERROR("Failed to create ImGui shaders");
        return false;
    }

    // Create PSO
    {
        rapi::BlendState blendState;
        blendState.renderTarget[0] = rapi::BlendState::RenderTargetBlendState
        {
            .blendEnable = true,
            .srcBlend = rapi::BlendFactor::SrcAlpha,
            .destBlend = rapi::BlendFactor::InvSrcAlpha,
            .srcBlendAlpha = rapi::BlendFactor::InvSrcAlpha,
            .destBlendAlpha = rapi::BlendFactor::Zero
        };

        rapi::RasterizerState rasterState =
        {
            .fillMode = rapi::FillMode::Solid,
            .cullMode = rapi::CullMode::None,
            .depthClipEnable = true,
            .scissorEnable = true
        };

        rapi::DepthStencilState depthStencilState =
        {
            .depthTestEnable = false,
            .depthWriteEnable = true,
            .depthFunc = rapi::ComparisonFunc::Always,
            .stencilEnable = false
        };

        m_BasePSODesc = rapi::GraphicsPipelineStateDesc
        {
            .VS = m_VS,
            .PS = m_PS,
            .blendState = blendState,
            .depthStencilState = depthStencilState,
            .rasterState = rasterState
        };
    }

    ImGui::CreateContext();

    return true;
}

bool st::ui::ImGuiRenderPass::UpdateFontTexture()
{
    st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
    auto* device = deviceManager->GetDevice();
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

    rapi::TextureDesc textureDesc;
    textureDesc.width = width;
    textureDesc.height = height;
    textureDesc.format = rapi::Format::RGBA8_UNORM;
    textureDesc.debugName = "ImGuiFontTexture";
    textureDesc.shaderUsage = rapi::ShaderUsage::ShaderResource;
    m_FontTexture = deviceManager->GetDevice()->CreateTexture(textureDesc, rapi::ResourceState::COPY_DST);
    if (!m_FontTexture)
        return false;

    m_TextureUploadBuffer = device->CreateBuffer(rapi::BufferDesc{
        .memoryAccess = rapi::MemoryAccess::Upload,
        .shaderUsage = rapi::ShaderUsage::None,
        .sizeBytes = (size_t)width * height * 4,
        .debugName = "ImGui TextureUploadBuffer" }, rapi::ResourceState::COMMON);
    
    void* uploadData = m_TextureUploadBuffer->Map();
    std::memcpy(uploadData, pixels, width * height * 4);
    m_TextureUploadBuffer->Unmap();

    m_CommandList->WriteTexture(m_FontTexture.get(), rapi::AllSubresources, m_TextureUploadBuffer.get(), 0);

    io.Fonts->TexRef = m_FontTexture.get();

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
    ImDrawVert* vtxDst = (ImDrawVert*)m_VertexBuffer->Map();
    ImDrawIdx* idxDst = (ImDrawIdx*)m_IndexBuffer->Map();

    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* cmdList = drawData->CmdLists[n];

        memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

        vtxDst += cmdList->VtxBuffer.Size;
        idxDst += cmdList->IdxBuffer.Size;
    }

    m_VertexBuffer->Unmap();
    m_IndexBuffer->Unmap();

    return true;
}

bool st::ui::ImGuiRenderPass::ReallocateBuffer(rapi::BufferHandle& buffer, size_t requiredSize, size_t reallocateSize, const bool indexBuffer)
{
    st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();

    if (buffer == nullptr || size_t(buffer->GetDesc().sizeBytes) < requiredSize)
    {
        rapi::BufferDesc desc;
        desc.memoryAccess = rapi::MemoryAccess::Upload;
        desc.shaderUsage = rapi::ShaderUsage::ShaderResource;
        desc.sizeBytes = uint32_t(reallocateSize);
        desc.allowUAV = false;
        desc.stride = 0;
        desc.debugName = indexBuffer ? "ImGui index buffer" : "ImGui vertex buffer";

        buffer = deviceManager->GetDevice()->CreateBuffer(desc, rapi::ResourceState::SHADER_RESOURCE);
        if (!buffer)
            return false;
    }
    return true;
}

st::rapi::GraphicsPipelineStateHandle st::ui::ImGuiRenderPass::GetPSO(rapi::IFramebuffer* frameBuffer)
{
    st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();

    if (!m_PSO)
    {
        m_PSO = deviceManager->GetDevice()->CreateGraphicsPipelineState(m_BasePSODesc, frameBuffer->GetFramebufferInfo());
        assert(m_PSO);
    }
    return m_PSO;
}
