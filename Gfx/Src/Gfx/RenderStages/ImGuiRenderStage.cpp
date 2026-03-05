#include "Gfx/RenderStages/ImGuiRenderStage.h"
#include "Core/Log.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/RenderView.h"
#include "Interop/ImGUI_CB.h"
#include "Gfx/UploadBuffer.h"
#include "RHI/Device.h"
#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl3.h>

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

    const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(main_viewport->WorkPos);
    ImGui::SetNextWindowSize(main_viewport->WorkSize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("Full-screen window ", 0, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
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
    ImVec2 contentPos = ImGui::GetCursorScreenPos();
    ImVec2 contentSize = ImGui::GetContentRegionAvail();

    ImGui::SetCursorScreenPos(ImVec2{
        contentPos.x + (contentSize.x - textSize.x) * 0.5f,
        contentPos.y + (contentSize.y - textSize.y) * 0.5f });

    ImGui::TextUnformatted(text);
}

st::rhi::BufferOwner& st::gfx::ImGuiRenderStage::GetCurrentVB(GeometryBuffers& geometryBuffers)
{
    return geometryBuffers.VertexBuffer[m_RenderView->GetDeviceManager()->GetFrameModuleIndex()];
}

st::rhi::BufferOwner& st::gfx::ImGuiRenderStage::GetCurrentIB(GeometryBuffers& geometryBuffers)
{
    return geometryBuffers.IndexBuffer[m_RenderView->GetDeviceManager()->GetFrameModuleIndex()];
}

void st::gfx::ImGuiRenderStage::Render()
{
	auto& io = ImGui::GetIO();
    rhi::CommandListHandle commandList = m_RenderView->GetCommandList();

    // Address any change in font data. Needs to be called before ImGui::NewFrame()
    if (!UpdateFontTexture(commandList.get()))
    {
        LOG_ERROR("Failed to update ImGui font texture");
        return;
    }

    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    BuildUI();
    ImGui::Render();

    commandList->BeginRenderPass(
        m_FB.get(),
        { rhi::RenderPassOp{rhi::RenderPassOp::LoadOp::Clear, rhi::RenderPassOp::StoreOp::Store} },
        {},
        {},
        rhi::RenderPassFlags::None);

    RenderDrawData(ImGui::GetDrawData(), m_GeometryBuffers, commandList.get());

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

    m_GuiFontTexture.reset();
    m_CurrentTextures.clear();

    device->ReleaseQueued(std::move(m_FontTexture));
    device->ReleaseQueued(std::move(m_PSO));
    m_GeometryBuffers = {};

    device->ReleaseQueued(std::move(m_PSO));
    device->ReleaseQueued(std::move(m_FB));
    device->ReleaseQueued(std::move(m_PS));
    device->ReleaseQueued(std::move(m_VS));
}

bool st::gfx::ImGuiRenderStage::UpdateFontTexture(rhi::ICommandList* commandList)
{
    st::gfx::DeviceManager* deviceManager = m_RenderView->GetDeviceManager();
    st::gfx::UploadBuffer* uploadBuffer = deviceManager->GetUploadBuffer();
    auto* device = deviceManager->GetDevice();
    ImGuiIO& io = ImGui::GetIO();
    ImTextureData* texData = io.Fonts->TexRef._TexData;

    if (!texData || texData->Status == ImTextureStatus_WantCreate || texData->Status == ImTextureStatus_WantUpdates)
    {
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        rhi::TextureDesc textureDesc;
        textureDesc.width = width;
        textureDesc.height = height;
        textureDesc.format = rhi::Format::RGBA8_UNORM;
        textureDesc.shaderUsage = rhi::TextureShaderUsage::Sampled;
        m_FontTexture = deviceManager->GetDevice()->CreateTexture(textureDesc, rhi::ResourceState::COPY_DST, "ImGuiFontTexture");
        if (!m_FontTexture)
            return false;

        auto [uploadData, offset] = uploadBuffer->RequestSpaceForTextureDataUpload(textureDesc);
        std::memcpy(uploadData, pixels, width * height * 4);

        commandList->WriteTexture(m_FontTexture.get(), rhi::AllSubresources, uploadBuffer->GetBuffer().get(), offset);

        commandList->PushBarrier(
            rhi::Barrier::Texture(m_FontTexture.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE));

        m_GuiFontTexture = st::make_unique_with_weak<ImGuiTexture>(m_FontTexture.get_weak(), ImGuiTexFlags_None);
        io.Fonts->SetTexID((ImTextureID)m_GuiFontTexture.get());
        io.Fonts->TexRef._TexData->Status = ImTextureStatus_OK;
    }
    else if (texData->Status == ImTextureStatus_WantDestroy)
    {
        m_GuiFontTexture.reset();
        m_FontTexture.reset();
        texData->SetTexID(0);
        texData->Status = ImTextureStatus_Destroyed;
    }

#if 0

    // If the font texture exists and is bound to ImGui, we're done.
    // Note: ImGui_Renderer will reset io.Fonts->TexID when new fonts are added.
    if (m_GuiFontTexture && io.Fonts->TexRef.GetTexID())
        return true;

    if (m_GuiFontTexture)
    {
        // Asegúrate de que ImGui siempre tiene el puntero correcto
        io.Fonts->TexRef = m_GuiFontTexture.get();
        return true;
    }

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

    commandList->WriteTexture(m_FontTexture.get(), rhi::AllSubresources, textureUploadBuffer.get(), 0);
    commandList->PushBarrier(
        rhi::Barrier::Texture(m_FontTexture.get(), rhi::ResourceState::COPY_DST, rhi::ResourceState::SHADER_RESOURCE));

    m_GuiFontTexture = st::make_unique_with_weak<ImGuiTexture>(m_FontTexture.get_weak(), ImGuiTexFlags_None);
    io.Fonts->TexRef = m_GuiFontTexture.get();
#endif
    return true;
}

bool st::gfx::ImGuiRenderStage::UpdateGeometry(ImDrawData* drawData, GeometryBuffers& geometryBuffers)
{
    rhi::BufferOwner& currentVB = GetCurrentVB(geometryBuffers);
    rhi::BufferOwner& currentIB = GetCurrentIB(geometryBuffers);

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
        desc.shaderUsage = rhi::BufferShaderUsage::ReadOnly;
        desc.sizeBytes = uint32_t(reallocateSize);
        if (indexBuffer)
        {
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

void st::gfx::ImGuiRenderStage::RenderDrawData(ImDrawData* drawData, GeometryBuffers& geometryBuffers, rhi::ICommandList* commandList)
{
    if (drawData->TotalIdxCount == 0)
        return; // nothing to render

    // Address any change in geometry data
    if (!UpdateGeometry(drawData, geometryBuffers))
    {
        LOG_ERROR("Failed to update ImGui geometry buffer");
        return;
    }

    const float2 invDisplaySize = { 1.0f / drawData->DisplaySize.x, 1.0f / drawData->DisplaySize.y };
    const float2 vpClipOffset = { drawData->DisplayPos.x, drawData->DisplayPos.y };
    const st::rhi::Viewport vp = {
        drawData->DisplaySize.x * drawData->FramebufferScale.x,
        drawData->DisplaySize.y * drawData->FramebufferScale.y
    };

    commandList->SetPipelineState(m_PSO.get());

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
                int2 scissorMin = {
                    (int)(pCmd->ClipRect.x - vpClipOffset.x),
                    (int)(pCmd->ClipRect.y - vpClipOffset.y)
                };
                int2 scissorMax = {
                    (int)(pCmd->ClipRect.z - vpClipOffset.x),
                    (int)(pCmd->ClipRect.w - vpClipOffset.y)
                };

                commandList->SetViewport(rhi::ViewportState()
                    .AddViewport(vp)
                    .AddScissorRect({ scissorMin, scissorMax }));

                interop::ImGUI_CB cb = {};
                cb.invDisplaySize = invDisplaySize;
                cb.clipOffset = vpClipOffset;
                cb.indexBuffer = GetCurrentIB(geometryBuffers)->GetReadOnlyView();
                cb.indexOffset = idxOffset;
                cb.vertexBuffer = GetCurrentVB(geometryBuffers)->GetReadOnlyView();
                cb.vertexBufferOffset = vtxOffset;

                if (pCmd->TexRef._TexData && pCmd->TexRef._TexData->TexID && !((ImGuiTexture*)pCmd->TexRef._TexData->TexID)->tex.expired())
                {
                    auto* guiTex = (ImGuiTexture*)pCmd->TexRef._TexData->TexID;
                    cb.textureIndex = guiTex->tex->GetSampledView();
                    cb.flags = guiTex->flags;
                    // Delete guiTex if it was a requested one (via ShowImage).
                    for (auto it = m_CurrentTextures.begin(); it != m_CurrentTextures.end(); ++it)
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