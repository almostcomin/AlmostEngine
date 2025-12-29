#include "Gfx/RenderStages/ImGuiRenderStage.h"
#include "Core/Log.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/RenderView.h"
#include "Interop/ImGUI_CB.h"
#include "Gfx/DataUploader.h"
#include "RHI/Device.h"
#include <imgui/imgui.h>

void st::gfx::ImGuiRenderStage::ReconcileInputState()
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

void st::gfx::ImGuiRenderStage::OnAttached()
{
    Init();

    m_RenderView->RequestTextureAccess(this, gfx::RenderView::AccessMode::Read, "SceneColor", 
        rhi::ResourceState::SHADER_RESOURCE, rhi::ResourceState::SHADER_RESOURCE);
}

void st::gfx::ImGuiRenderStage::OnDetached()
{
    Release();
}

void st::gfx::ImGuiRenderStage::BeginFullScreenWindow()
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

void st::gfx::ImGuiRenderStage::EndFullScreenWindow()
{
    ImGui::End();
    ImGui::PopStyleVar();
}

void st::gfx::ImGuiRenderStage::DrawCenteredText(const char* text)
{
    ImGuiIO const& io = ImGui::GetIO();
    ImVec2 textSize = ImGui::CalcTextSize(text);

    ImVec2 vMin = ImGui::GetCursorScreenPos();
    ImVec2 vMax = ImGui::GetContentRegionAvail();

    ImGui::SetCursorScreenPos(ImVec2{ (vMax.x - vMin.x - textSize.x) * 0.5f, (vMax.y - vMin.y - textSize.y) * 0.5f });
    ImGui::TextUnformatted(text);
}

st::rhi::BufferOwner& st::gfx::ImGuiRenderStage::GetCurrentVB()
{
    uint32_t currentFrameIdx = m_RenderView->GetDeviceManager()->GetFrameCount();
    return m_VertexBuffer[currentFrameIdx % 3];
}

st::rhi::BufferOwner& st::gfx::ImGuiRenderStage::GetCurrentIB()
{
    uint32_t currentFrameIdx = m_RenderView->GetDeviceManager()->GetFrameCount();
    return m_IndexBuffer[currentFrameIdx % 3];
}

bool st::gfx::ImGuiRenderStage::Render()
{
    st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
    st::rhi::Device* device = deviceManager->GetDevice();
	auto& io = ImGui::GetIO();

    glm::ivec2 dim = deviceManager->GetWindowDimensions();
    io.DisplaySize = ImVec2(dim.x, dim.y);

    rhi::CommandListHandle commandList = m_RenderView->GetCommandList();

    // Address any change in font data. Needs to be called before ImGui::NewFrame()
    if (!UpdateFontTexture())
    {
        LOG_ERROR("Failed to update ImGui font texture");
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
        return false;
    }

    // Handle DPI scaling
    drawData->ScaleClipRects(io.DisplayFramebufferScale);

    float2 invDisplaySize = { 1.f / io.DisplaySize.x, 1.f / io.DisplaySize.y };

    rhi::FramebufferHandle frameBuffer = m_RenderView->GetFramebuffer();

    commandList->BeginRenderPass(
        frameBuffer.get(),
        { rhi::RenderPassOp{rhi::RenderPassOp::LoadOp::Load, rhi::RenderPassOp::StoreOp::Store} },
        {},
        rhi::RenderPassFlags::None);

    commandList->SetPipelineState(GetPSO(frameBuffer.get()).get());
    
    commandList->SetViewport(rhi::ViewportState().AddViewportAndScissorRect({
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
                commandList->SetViewport(rhi::ViewportState().AddViewport({
                    io.DisplaySize.x * io.DisplayFramebufferScale.x,
                    io.DisplaySize.y * io.DisplayFramebufferScale.y }).AddScissorRect({
                    int2 { pCmd->ClipRect.x, pCmd->ClipRect.y }, int2{pCmd->ClipRect.z, pCmd->ClipRect.w} }));

                interop::ImGUI_CB cb = {};
                cb.invDisplaySize = invDisplaySize;
                cb.indexBuffer = GetCurrentIB()->GetShaderViewIndex(rhi::BufferShaderView::ShaderResource);
                cb.indexOffset = idxOffset;
                cb.vertexBuffer = GetCurrentVB()->GetShaderViewIndex(rhi::BufferShaderView::ShaderResource);
                cb.vertexBufferOffset = vtxOffset;
                cb.textureIndex = m_FontTexture->GetShaderViewIndex(rhi::TextureShaderView::ShaderResource);

                commandList->PushConstants(&cb, sizeof(interop::ImGUI_CB), 0);
                
                commandList->Draw(pCmd->ElemCount);
            }
            idxOffset += pCmd->ElemCount;
        }
        vtxOffset += cmdList->VtxBuffer.Size;
    }

    commandList->EndRenderPass();
    
    return true;
}

void st::gfx::ImGuiRenderStage::OnBackbufferResize(const glm::ivec2& size)
{
    // no-op
}

bool st::gfx::ImGuiRenderStage::OnMouseMove(float posX, float posY)
{
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos.x = float(posX);
    io.MousePos.y = float(posY);

    return io.WantCaptureMouse;
}

bool st::gfx::ImGuiRenderStage::OnMouseButtonUpdate(MouseButton button, KeyAction action)
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

bool st::gfx::ImGuiRenderStage::Init()
{
    st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
    st::rhi::Device* device = deviceManager->GetDevice();
    st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();

    m_VS = shaderFactory->LoadShader("imgui_bindless_vs.vso", rhi::ShaderType::Vertex);
    m_PS = shaderFactory->LoadShader("imgui_bindless_ps.pso", rhi::ShaderType::Pixel);
    if (!m_VS || !m_PS)
    {
        LOG_ERROR("Failed to create ImGui shaders");
        return false;
    }

    // Create PSO
    {
        rhi::BlendState blendState;
        blendState.renderTarget[0] = rhi::BlendState::RenderTargetBlendState
        {
            .blendEnable = true,
            .srcBlend = rhi::BlendFactor::SrcAlpha,
            .destBlend = rhi::BlendFactor::InvSrcAlpha,
            .srcBlendAlpha = rhi::BlendFactor::InvSrcAlpha,
            .destBlendAlpha = rhi::BlendFactor::Zero
        };

        rhi::RasterizerState rasterState =
        {
            .fillMode = rhi::FillMode::Solid,
            .cullMode = rhi::CullMode::None,
            .depthClipEnable = true,
            .scissorEnable = true
        };

        rhi::DepthStencilState depthStencilState =
        {
            .depthTestEnable = false,
            .depthWriteEnable = true,
            .depthFunc = rhi::ComparisonFunc::Always,
            .stencilEnable = false
        };

        m_BasePSODesc = rhi::GraphicsPipelineStateDesc
        {
            .VS = m_VS.get_weak(),
            .PS = m_PS.get_weak(),
            .blendState = blendState,
            .depthStencilState = depthStencilState,
            .rasterState = rasterState
        };
    }

    ImGui::CreateContext();

    return true;
}

void st::gfx::ImGuiRenderStage::Release()
{
    auto* device = m_RenderView->GetDeviceManager()->GetDevice();

    ImGui::DestroyContext();

    device->ReleaseQueued(m_FontTexture);
    device->ReleaseQueued(m_PSO);
    for (int i = 0; i < 3; ++i)
    {
        device->ReleaseQueued(m_VertexBuffer[i]);
        device->ReleaseQueued(m_IndexBuffer[i]);
    }
}

bool st::gfx::ImGuiRenderStage::UpdateFontTexture()
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

    rhi::TextureDesc textureDesc;
    textureDesc.width = width;
    textureDesc.height = height;
    textureDesc.format = rhi::Format::RGBA8_UNORM;
    textureDesc.shaderUsage = rhi::TextureShaderUsage::ShaderResource;
    m_FontTexture = deviceManager->GetDevice()->CreateTexture(textureDesc, rhi::ResourceState::COPY_DST, "ImGuiFontTexture");
    if (!m_FontTexture)
        return false;
    
    rhi::BufferOwner textureUploadBuffer = device->CreateBuffer(rhi::BufferDesc{
        .memoryAccess = rhi::MemoryAccess::Upload,
        .shaderUsage = rhi::BufferShaderUsage::None,
        .sizeBytes = (size_t)width * height * 4 }, 
        rhi::ResourceState::COMMON, "ImGui TextureUploadBuffer");
    
    void* uploadData = textureUploadBuffer->Map();
    std::memcpy(uploadData, pixels, width * height * 4);
    textureUploadBuffer->Unmap();

    m_RenderView->GetCommandList()->WriteTexture(m_FontTexture.get(), rhi::AllSubresources, textureUploadBuffer.get(), 0);
    m_RenderView->GetCommandList()->PushBarrier(
        rhi::Barrier::Texture(m_FontTexture.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE));

    io.Fonts->TexRef = m_FontTexture.get();

    return true;
}

bool st::gfx::ImGuiRenderStage::UpdateGeometry()
{
    rhi::BufferOwner& currentVB = GetCurrentVB();
    rhi::BufferOwner& currentIB = GetCurrentIB();

    const ImDrawData* drawData = ImGui::GetDrawData();

    // Create/resize vertex and index buffers if needed
    if (!ReallocateBuffer(currentVB,
        drawData->TotalVtxCount * sizeof(ImDrawVert),
        (drawData->TotalVtxCount + 5000) * sizeof(ImDrawVert),
        false))
    {
        return false;
    }
    if (!ReallocateBuffer(currentIB,
        drawData->TotalIdxCount * sizeof(ImDrawIdx),
        (drawData->TotalIdxCount + 5000) * sizeof(ImDrawIdx),
        true))
    {
        return false;
    }

    // Copy and convert all vertices into a single contiguous buffer
    ImDrawVert* vtxDst = (ImDrawVert*)currentVB->Map();
    ImDrawIdx* idxDst = (ImDrawIdx*)currentIB->Map();

    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* cmdList = drawData->CmdLists[n];

        memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

        vtxDst += cmdList->VtxBuffer.Size;
        idxDst += cmdList->IdxBuffer.Size;
    }

    currentVB->Unmap();
    currentIB->Unmap();

    return true;
}

bool st::gfx::ImGuiRenderStage::ReallocateBuffer(rhi::BufferOwner& buffer, size_t requiredSize, size_t reallocateSize, const bool indexBuffer)
{
    st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();

    if (!buffer || size_t(buffer->GetDesc().sizeBytes) < requiredSize)
    {
        rhi::BufferDesc desc;
        desc.memoryAccess = rhi::MemoryAccess::Upload;
        desc.shaderUsage = rhi::BufferShaderUsage::ShaderResource;
        desc.sizeBytes = uint32_t(reallocateSize);
        desc.allowUAV = false;
        if (indexBuffer)
        {
            //desc.format = rhi::Format::R16_UINT;
            desc.stride = 2;
        }
        else
        {
            desc.stride = sizeof(ImDrawVert);
        }

        buffer = deviceManager->GetDevice()->CreateBuffer(desc, rhi::ResourceState::SHADER_RESOURCE, 
            indexBuffer ? "ImGui index buffer" : "ImGui vertex buffer");
        if (!buffer)
            return false;
    }
    return true;
}

st::rhi::GraphicsPipelineStateHandle st::gfx::ImGuiRenderStage::GetPSO(rhi::IFramebuffer* frameBuffer)
{
    st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();

    if (!m_PSO)
    {
        m_PSO = deviceManager->GetDevice()->CreateGraphicsPipelineState(m_BasePSODesc, frameBuffer->GetFramebufferInfo(), "ImGui PSO");
        assert(m_PSO);
    }
    return m_PSO.get_weak();
}
