#include "Gfx/RenderStages/ImGuiRenderStage.h"
#include "Core/Log.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/RenderView.h"
#include "Interop/ImGUI_CB.h"
#include "Gfx/DataUploader.h"
#include "RHI/Device.h"
#include <imgui/imgui.h>

void st::gfx::ImGuiRenderStage::OnAttached()
{
    m_RenderView->CreateColorTarget("ImGui", RenderView::c_BBSize, RenderView::c_BBSize, 1, rhi::Format::RGBA8_UNORM);
    m_RenderView->RequestTextureAccess(this, RenderView::AccessMode::Write, "ImGui",
        rhi::ResourceState::RENDERTARGET, rhi::ResourceState::RENDERTARGET);

    Init();
}

void st::gfx::ImGuiRenderStage::OnDetached()
{
    m_RenderView->ReleaseTexture("ImGui");
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
    uint32_t currentFrameIdx = m_RenderView->GetDeviceManager()->GetFrameIndex();
    return m_VertexBuffer[currentFrameIdx % 3];
}

st::rhi::BufferOwner& st::gfx::ImGuiRenderStage::GetCurrentIB()
{
    uint32_t currentFrameIdx = m_RenderView->GetDeviceManager()->GetFrameIndex();
    return m_IndexBuffer[currentFrameIdx % 3];
}

void st::gfx::ImGuiRenderStage::Render()
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
        return;
    }

    ImGui::NewFrame();
    BuildUI();
    ImGui::Render();

    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData->TotalIdxCount == 0)
        return; // nothing to render

    // Address any change in geometry data
    if (!UpdateGeometry())
    {
        LOG_ERROR("Failed to update ImGui geometry buffer");
        return;
    }

    // Handle DPI scaling
    drawData->ScaleClipRects(io.DisplayFramebufferScale);

    float2 invDisplaySize = { 1.f / io.DisplaySize.x, 1.f / io.DisplaySize.y };

    commandList->BeginRenderPass(
        m_FB.get(),
        { rhi::RenderPassOp{rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store} },
        {},
        {},
        rhi::RenderPassFlags::None);

    commandList->SetPipelineState(m_PSO.get());
    
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
                if (pCmd->TexRef._TexID != NULL && !((ImGuiTexture*)pCmd->TexRef._TexID)->tex.expired())
                {
                    auto* guiTex = (ImGuiTexture*)pCmd->TexRef._TexID;
                    cb.textureIndex = guiTex->tex->GetSampledView();
                    cb.flags = guiTex->flags;
                    // Delete guiTex if it was a requested one (via ShowImage).
                    for(auto it = m_CurrentTextures.begin(); it != m_CurrentTextures.end(); ++it)
                    {
                        if (it->get() == guiTex)
                        {
                            m_CurrentTextures.erase(it);
                            break;
                        }
                    }
                }
                else
                {
                    cb.textureIndex = {};
                }

                commandList->PushConstants(&cb, sizeof(interop::ImGUI_CB), 0, false);
                
                commandList->Draw(pCmd->ElemCount);
            }
            idxOffset += pCmd->ElemCount;
        }
        vtxOffset += cmdList->VtxBuffer.Size;
    }

    commandList->EndRenderPass();
}

void st::gfx::ImGuiRenderStage::OnBackbufferResize()
{
    st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
    st::rhi::Device* device = deviceManager->GetDevice();

    // Recreate framebuffer
    {
        m_FB = device->CreateFramebuffer(rhi::FramebufferDesc()
            .AddColorAttachment(m_RenderView->GetTexture("ImGui")), "ImGui");
    }

    // Recreatre PSO
    {
        m_PSO = deviceManager->GetDevice()->CreateGraphicsPipelineState(
            m_BasePSODesc, m_FB->GetFramebufferInfo(), "ImGui PSO");
        assert(m_PSO);
    }
}

bool st::gfx::ImGuiRenderStage::Init()
{
    st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
    st::rhi::Device* device = deviceManager->GetDevice();
    st::gfx::ShaderFactory* shaderFactory = deviceManager->GetShaderFactory();

    // Load shaders
    {
        m_VS = shaderFactory->LoadShader("imgui_bindless_vs", rhi::ShaderType::Vertex);
        m_PS = shaderFactory->LoadShader("imgui_bindless_ps", rhi::ShaderType::Pixel);
        if (!m_VS || !m_PS)
        {
            LOG_ERROR("Failed to create ImGui shaders");
            return false;
        }
    }

    // Create framebuffer
    {
        m_FB = device->CreateFramebuffer(rhi::FramebufferDesc()
            .AddColorAttachment(m_RenderView->GetTexture("ImGui")), "ImGui");
    }

    // Create PSO
    {
        rhi::BlendState blendState;
        blendState.renderTarget[0] = rhi::BlendState::RenderTargetBlendState
        {
            .blendEnable = true,
            .srcBlend = rhi::BlendFactor::SrcAlpha,
            .destBlend = rhi::BlendFactor::InvSrcAlpha,
            .srcBlendAlpha = rhi::BlendFactor::One,
            .destBlendAlpha = rhi::BlendFactor::InvSrcAlpha
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

        m_PSO = deviceManager->GetDevice()->CreateGraphicsPipelineState(m_BasePSODesc, m_FB->GetFramebufferInfo(), "ImGui PSO");
        assert(m_PSO);
    }

    return true;
}

void st::gfx::ImGuiRenderStage::Release()
{
    auto* device = m_RenderView->GetDeviceManager()->GetDevice();

    ImGui::DestroyContext();

    m_GuiFontTexture.reset();
    m_CurrentTextures.clear();

    device->ReleaseQueued(m_FontTexture);
    device->ReleaseQueued(m_PSO);
    for (int i = 0; i < 3; ++i)
    {
        device->ReleaseQueued(m_VertexBuffer[i]);
        device->ReleaseQueued(m_IndexBuffer[i]);
    }

    device->ReleaseQueued(m_PSO);
    device->ReleaseQueued(m_FB);
    device->ReleaseQueued(m_PS);
    device->ReleaseQueued(m_VS);
}

bool st::gfx::ImGuiRenderStage::UpdateFontTexture()
{
    st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
    auto* device = deviceManager->GetDevice();
    ImGuiIO& io = ImGui::GetIO();

    // If the font texture exists and is bound to ImGui, we're done.
    // Note: ImGui_Renderer will reset io.Fonts->TexID when new fonts are added.
    if (m_GuiFontTexture && io.Fonts->TexRef.GetTexID())
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
    textureDesc.shaderUsage = rhi::TextureShaderUsage::Sampled;
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

    m_GuiFontTexture = st::make_unique_with_weak<ImGuiTexture>(m_FontTexture.get_weak(), ImGuiTexFlags_None);
    io.Fonts->TexRef = m_GuiFontTexture.get();

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

void st::gfx::ImGuiRenderStage::ShowImage(rhi::TextureHandle tex, const float2& size, const float2& uv0, const float2& uv1, ImGuiTexFlags flags)
{
    auto* imGuiTex = new ImGuiTexture{ tex, flags };
    m_CurrentTextures.push_back(st::unique<ImGuiTexture>{ imGuiTex });

    ImGui::Image(ImTextureRef{ imGuiTex }, ImVec2{ size.x, size.y }, ImVec2{ uv0.x, uv0.y }, ImVec2{ uv1.x, uv1.y });
}

bool st::gfx::ImGuiRenderStage::ShowToggleButton(const char* label, bool* v)
{
    bool newV = *v;
    if (*v)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button(label))
        newV = !newV;
    if (*v)
        ImGui::PopStyleColor();
    *v = newV;
    return *v;
}