#include "Gfx/GfxPCH.h"
#include "Gfx/CommonResources.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/DataUploader.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"
#include "Gfx/Math/Util.h"
#include "RHI/Device.h"

alm::gfx::CommonResources::CommonResources(alm::gfx::ShaderFactory* shaderFactory, alm::rhi::Device* device) :
	m_ShaderFactory(shaderFactory), m_Device(device)
{
	m_BlitVS = m_ShaderFactory->LoadShader("Blit_vs", rhi::ShaderType::Vertex);
	m_BlitPS = m_ShaderFactory->LoadShader("Blit_ps", rhi::ShaderType::Pixel);
	m_BlitCS = m_ShaderFactory->LoadShader("Blit_cs", rhi::ShaderType::Compute);
	m_ClearBufferCS = m_ShaderFactory->LoadShader("ClearBuffer_cs", rhi::ShaderType::Compute);
	m_ClearTextureCS = m_ShaderFactory->LoadShader("ClearTexture_cs", rhi::ShaderType::Compute);

	// Create blit PSO desc
	{
		rhi::BlendState blendState;
		blendState.renderTarget[0] = rhi::BlendState::RenderTargetBlendState
		{
			.blendEnable = false,
		};

		rhi::RasterizerState rasterState =
		{
			.fillMode = rhi::FillMode::Solid,
			.cullMode = rhi::CullMode::None
		};

		rhi::DepthStencilState depthStencilState =
		{
			.depthTestEnable = false,
			.depthWriteEnable = false,
			.stencilEnable = false
		};

		m_BlitGraphicsPSODesc = rhi::GraphicsPipelineStateDesc
		{
			.VS = m_BlitVS.get_weak(),
			.PS = m_BlitPS.get_weak(),
			.blendState = blendState,
			.depthStencilState = depthStencilState,
			.rasterState = rasterState,
		};
	}

	// Compute blit
	{
		m_BlitComputePSO = m_Device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ m_BlitCS.get_weak() }, "BlitComputePSO");
	}

	// Compute ClearBuffer PSO
	{
		m_ClearBufferPSO = m_Device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ m_ClearBufferCS.get_weak() }, "ClearBufferPSO");
	}

	// Compute ClearTexture PSO
	{
		m_ClearTexturePSO = m_Device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ m_ClearTextureCS.get_weak() }, "ClearTexturePSO");
	}
}

alm::gfx::CommonResources::~CommonResources()
{
}

alm::rhi::GraphicsPipelineStateOwner alm::gfx::CommonResources::CreateBlitGraphicsPSO(const rhi::FramebufferInfo& fbInfo)
{
	return m_Device->CreateGraphicsPipelineState(m_BlitGraphicsPSODesc, fbInfo, "BlitGraphicsPSO");
}

alm::rhi::GraphicsPipelineStateOwner alm::gfx::CommonResources::CreateFullscreenPassPSO(const rhi::FramebufferInfo& fbInfo,
	const rhi::ShaderHandle& PS, const std::string debugName)
{
	auto desc = m_BlitGraphicsPSODesc;
	desc.PS = PS;

	return m_Device->CreateGraphicsPipelineState(desc, fbInfo, debugName);
}

alm::rhi::GraphicsPipelineStateOwner alm::gfx::CommonResources::CreateFullscreenPassPSO(const rhi::FramebufferInfo& fbInfo,
	const rhi::ShaderHandle& VS, const rhi::ShaderHandle& PS, const std::string debugName)
{
	auto desc = m_BlitGraphicsPSODesc;
	desc.VS = VS;
	desc.PS = PS;

	return m_Device->CreateGraphicsPipelineState(desc, fbInfo, debugName);
}

std::shared_ptr<alm::gfx::Mesh> alm::gfx::CommonResources::CreateUVSphere(float radius, uint32_t stacks, uint32_t slices, alm::gfx::DataUploader* dataUploader,
	const std::string& name)
{
	assert(stacks >= 2);
	assert(slices >= 3);
	
	// Generate vertices

	// Position : float3
	// Normal : uint32_t
	const uint32_t vertexSize = sizeof(float3) + sizeof(uint32_t);
	alm::rhi::BufferOwner vertexBuffer;
	alm::SignalListener vbUploadSignal;
	const uint32_t numVertices = (stacks + 1) * (slices + 1);
	{
		alm::rhi::BufferDesc vertexBufferDesc;
		vertexBufferDesc.memoryAccess = alm::rhi::MemoryAccess::Default;
		vertexBufferDesc.shaderUsage = alm::rhi::BufferShaderUsage::ReadOnly;
		vertexBufferDesc.sizeBytes = numVertices * vertexSize;
		vertexBufferDesc.stride = vertexSize;

		auto verticesRequestResult = dataUploader->RequestUploadTicket(vertexBufferDesc);
		assert(verticesRequestResult);
		auto* vertex = (char*)verticesRequestResult->GetPtr();

		for (uint32_t stack = 0; stack <= stacks; ++stack)
		{
			// phi goes from 0 (north pole) to PI (south pole)
			float phi = static_cast<float>(stack) / stacks * glm::pi<float>();
			float sinPhi = std::sin(phi);
			float cosPhi = std::cos(phi);

			for (uint32_t slice = 0; slice <= slices; ++slice)
			{
				// theta goes from 0 to 2*PI around the sphere
				float theta = static_cast<float>(slice) / slices * 2.0f * glm::pi<float>();
				float sinTheta = std::sin(theta);
				float cosTheta = std::cos(theta);

				float3 normal = float3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);

				// position
				*(float3*)vertex = normal * radius;
				vertex += sizeof(float3);
				// normal
				*(uint32_t*)vertex = alm::math::VectorToSnorm8(normal);
				vertex += sizeof(uint32_t);
			}
		}

		vertexBuffer = m_Device->CreateBuffer(vertexBufferDesc, alm::rhi::ResourceState::COPY_DST, std::format("{} - VB", name));
		auto verticesUploadResult = dataUploader->CommitUploadBufferTicket(
			std::move(*verticesRequestResult),
			vertexBuffer.get_weak(),
			alm::rhi::ResourceState::COPY_DST,
			alm::rhi::ResourceState::SHADER_RESOURCE);
		assert(verticesUploadResult);
		vbUploadSignal = *verticesUploadResult;
	}

	// Generate indices (two triangles per quad)
	const uint32_t numIndices = 6 * slices * (stacks - 1);  // poles generate a single triangle, not a quad
	const bool idx32bits = numVertices > std::numeric_limits<uint16_t>::max() ? true : false;
	alm::rhi::BufferOwner indexBuffer;
	alm::SignalListener ibUploadSignal;
	{
		alm::rhi::BufferDesc indexBufferDesc;
		indexBufferDesc.memoryAccess = alm::rhi::MemoryAccess::Default;
		indexBufferDesc.shaderUsage = alm::rhi::BufferShaderUsage::ReadOnly;
		indexBufferDesc.sizeBytes = numIndices * (idx32bits ? sizeof(uint32_t) : sizeof(uint16_t));
		indexBufferDesc.stride = idx32bits ? sizeof(int32_t) : sizeof(int16_t);

		auto requestResult = dataUploader->RequestUploadTicket(indexBufferDesc);
		assert(requestResult);
		char* indices = (char*)requestResult->GetPtr();

		auto pushIndex = [&indices, idx32bits](uint32_t i)
		{
			if(idx32bits)
			{
				*(uint32_t*)indices = i;
				indices += sizeof(uint32_t);
			}
			else
			{
				*(uint16_t*)indices = i;
				indices += sizeof(uint16_t);
			}
		};

		const  uint32_t verticesPerRow = slices + 1;
		for (uint32_t stack = 0; stack < stacks; ++stack)
		{
			for (uint32_t slice = 0; slice < slices; ++slice)
			{
				uint32_t topLeft = stack * verticesPerRow + slice;
				uint32_t topRight = topLeft + 1;
				uint32_t bottomLeft = (stack + 1) * verticesPerRow + slice;
				uint32_t bottomRight = bottomLeft + 1;

				// Skip degenerate triangles at poles
				if (stack != 0)
				{
					// Top triangle
					pushIndex(topLeft);
					pushIndex(topRight);
					pushIndex(bottomLeft);
				}

				if (stack != stacks - 1)
				{
					// Bottom triangle
					pushIndex(topRight);
					pushIndex(bottomRight);
					pushIndex(bottomLeft);
				}
			}
		}

		indexBuffer = m_Device->CreateBuffer(indexBufferDesc, alm::rhi::ResourceState::COPY_DST, std::format("{} - IB", name));
		auto uploadResult = dataUploader->CommitUploadBufferTicket(
			std::move(*requestResult),
			indexBuffer.get_weak(),
			alm::rhi::ResourceState::COPY_DST,
			alm::rhi::ResourceState::SHADER_RESOURCE);
		assert(uploadResult);
		ibUploadSignal = *uploadResult;
	}

	auto* mesh = new alm::gfx::Mesh{ m_Device, name.c_str(), "[generated]" };

	Mesh::VertexFormat vertexFormat{
		.VertexStride = vertexSize,
		.PositionOffset = 0,
		.NormalOffset = sizeof(float3)
	};
	mesh->SetVertexBuffer(std::move(vertexBuffer), vertexFormat);
	
	mesh->SetIndexBuffer(std::move(indexBuffer), rhi::PrimitiveTopology::TriangleList, idx32bits ? sizeof(uint32_t) : sizeof(uint16_t));

	Material* material = new Material(m_Device, std::format("{} - Material", name), "[generated]");
	material->SetBaseColor(float3{ 0.5f, 1.f, 1.f });
	mesh->SetMaterial(std::shared_ptr<Material>{ material });

	mesh->SetBounds(math::aabox3f{ float3{-radius}, float3{radius} });

	// Lets wait until eveything is submitted.
	// We could also just return array of signals al let the called wait
	vbUploadSignal.Wait();
	ibUploadSignal.Wait();

	return std::shared_ptr<alm::gfx::Mesh>{ mesh };
}