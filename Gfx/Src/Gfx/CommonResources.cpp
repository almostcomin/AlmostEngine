#include "Gfx/GfxPCH.h"
#include "Gfx/CommonResources.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/MaterialManager.h"
#include "Gfx/DataUploader.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"
#include "Gfx/Math/Util.h"
#include "Interop/RenderResources.h"
#include "RHI/Device.h"

alm::gfx::CommonResources::CommonResources(alm::gfx::ShaderFactory* shaderFactory, MaterialManager* materialManager, alm::rhi::Device* device) :
	m_ShaderFactory(shaderFactory), m_MaterialManager(materialManager),	m_Device(device)
{
	m_BlitVS = m_ShaderFactory->LoadShader("Blit_vs", rhi::ShaderType::Vertex);
	m_BlitPS = m_ShaderFactory->LoadShader("Blit_ps", rhi::ShaderType::Pixel);
	m_BlitCS = m_ShaderFactory->LoadShader("Blit_cs", rhi::ShaderType::Compute);
	m_ClearBufferCS = m_ShaderFactory->LoadShader("ClearBuffer_cs", rhi::ShaderType::Compute);
	m_ClearTexture_RGBA_CS = m_ShaderFactory->LoadShader("ClearTexture_RGBA_cs", rhi::ShaderType::Compute);
	m_ClearTexture_R_CS = m_ShaderFactory->LoadShader("ClearTexture_R_cs", rhi::ShaderType::Compute);

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
		m_ClearTexture_RGBA_PSO = m_Device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ m_ClearTexture_RGBA_CS.get_weak() }, "ClearTexture_RGBA_PSO");
		m_ClearTexture_R_PSO = m_Device->CreateComputePipelineState(
			rhi::ComputePipelineStateDesc{ m_ClearTexture_R_CS.get_weak() }, "ClearTexture_R_PSO");
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

std::shared_ptr<alm::gfx::Mesh> alm::gfx::CommonResources::CreateUVSphere(float radius, uint32_t stacks, uint32_t slices, bool generateCpuGeometry,
	alm::gfx::DataUploader* dataUploader, const std::string& name, const alm::gfx::MaterialRef* opt_materialRef)
{
	assert(stacks >= 2);
	assert(slices >= 3);
	
	// Generate vertices

	// Position : float3
	// Normal : uint32_t
	std::shared_ptr<rhi::BufferOwner> vertexBuffer;
	const uint32_t vertexSize = sizeof(float3) + sizeof(uint32_t);
	alm::SignalListener vbUploadSignal;
	const uint32_t numVertices = (stacks + 1) * (slices + 1);
	std::vector<float3> cpuPositions;
	if (generateCpuGeometry)
	{
		cpuPositions.reserve(numVertices);
	}
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
				float3 position = normal * radius;
				*(float3*)vertex = position;
				vertex += sizeof(float3);
				if (generateCpuGeometry)
				{
					cpuPositions.push_back(position);
				}
				// normal
				*(uint32_t*)vertex = alm::math::VectorToSnorm8(normal);
				vertex += sizeof(uint32_t);
			}
		}

		vertexBuffer = std::make_shared<alm::rhi::BufferOwner>();

		*vertexBuffer = m_Device->CreateBuffer(
			vertexBufferDesc, alm::rhi::ResourceState::COPY_DST, std::format("{} - VB", name));
		auto verticesUploadResult = dataUploader->CommitUploadBufferTicket(
			std::move(*verticesRequestResult),
			vertexBuffer->get_weak(),
			alm::rhi::ResourceState::COPY_DST,
			alm::rhi::ResourceState::SHADER_RESOURCE);
		assert(verticesUploadResult);
		vbUploadSignal = *verticesUploadResult;
	}

	// Generate indices (two triangles per quad)
	std::shared_ptr<rhi::BufferOwner> indexBuffer;
	const uint32_t numIndices = 6 * slices * (stacks - 1);  // poles generate a single triangle, not a quad
	std::vector<uint32_t> cpuIndices;
	if (generateCpuGeometry)
	{
		cpuIndices.reserve(numIndices);
	}
	const bool idx32bits = numVertices > std::numeric_limits<uint16_t>::max() ? true : false;
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

		auto pushIndex = [&indices, &cpuIndices, generateCpuGeometry, idx32bits](uint32_t i)
		{
			if (generateCpuGeometry)
			{
				cpuIndices.push_back(i);
			}

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

		indexBuffer = std::make_shared<alm::rhi::BufferOwner>();

		*indexBuffer = m_Device->CreateBuffer(
			indexBufferDesc, alm::rhi::ResourceState::COPY_DST, std::format("{} - IB", name));
		auto uploadResult = dataUploader->CommitUploadBufferTicket(
			std::move(*requestResult),
			indexBuffer->get_weak(),
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
	mesh->SetVertexBuffer(vertexBuffer, vertexFormat);
	
	mesh->SetIndexBuffer(indexBuffer, rhi::PrimitiveTopology::TriangleList, idx32bits ? sizeof(uint32_t) : sizeof(uint16_t));

	if (generateCpuGeometry)
	{
		mesh->SetCpuPositions(std::move(cpuPositions));
		mesh->SetCpuIndices(std::move(cpuIndices));
	}

	if (opt_materialRef)
	{
		mesh->SetMaterial(*opt_materialRef);
	}
	else
	{
		MaterialRef mRef = m_MaterialManager->CreateNewMaterial(std::format("{} - Material", name));
		mRef->SetBaseColor(float3{ 1.f, 1.f, 1.f });
		mesh->SetMaterial(mRef);
	}

	mesh->SetBounds(aabox3f{ float3{-radius}, float3{radius} });

	// Lets wait until eveything is submitted.
	// We could also just return array of signals al let the called wait
	vbUploadSignal.Wait();
	ibUploadSignal.Wait();

	return std::shared_ptr<alm::gfx::Mesh>{ mesh };
}

std::shared_ptr<alm::gfx::Mesh> alm::gfx::CommonResources::CreateCube(float size, CubeNormalDirection normalDirection, CubeUVMode uvMode,
	bool generateCpuGeometry, alm::gfx::DataUploader* dataUploader, const std::string& name, const alm::gfx::MaterialRef* opt_materialRef)
{
	assert(size > 0.f);

	const bool hasNormals = normalDirection != CubeNormalDirection::None;
	const bool inwardNormals = normalDirection == CubeNormalDirection::Inward;
	const bool hasUVs = uvMode != CubeUVMode::None;

	const uint32_t normalSize = hasNormals ? sizeof(uint32_t) : 0;
	const uint32_t uvSize = uvMode == CubeUVMode::UV2D ? sizeof(float2) : (uvMode == CubeUVMode::UV3D ? sizeof(float3) : 0);
	const uint32_t vertexSize = sizeof(float3) + normalSize + uvSize;

	const uint32_t normalOffset = hasNormals ? sizeof(float3) : UINT32_MAX;
	const uint32_t uvOffset = (hasNormals ? sizeof(float3) + normalSize : sizeof(float3));

	const float halfSize = size * 0.5f;

	// Per face frame, tangentU x tangentV = normal (right handed, faces wound CCW when viewed from outside)
	struct Face
	{
		float3 normal;
		float3 tangentU;
		float3 tangentV;
	};
	static constexpr Face kFaces[6] = {
		{ float3{ 1.f, 0.f, 0.f }, float3{ 0.f, 0.f, -1.f }, float3{ 0.f, 1.f, 0.f } },  // +X
		{ float3{ -1.f, 0.f, 0.f }, float3{ 0.f, 0.f, 1.f }, float3{ 0.f, 1.f, 0.f } },  // -X
		{ float3{ 0.f, 1.f, 0.f }, float3{ 1.f, 0.f, 0.f }, float3{ 0.f, 0.f, -1.f } },  // +Y
		{ float3{ 0.f, -1.f, 0.f }, float3{ 1.f, 0.f, 0.f }, float3{ 0.f, 0.f, 1.f } },  // -Y
		{ float3{ 0.f, 0.f, 1.f }, float3{ 1.f, 0.f, 0.f }, float3{ 0.f, 1.f, 0.f } },   // +Z
		{ float3{ 0.f, 0.f, -1.f }, float3{ -1.f, 0.f, 0.f }, float3{ 0.f, 1.f, 0.f } }  // -Z
	};

	// Generate vertices
	std::shared_ptr<rhi::BufferOwner> vertexBuffer;
	const uint32_t numVertices = 24;
	std::vector<float3> cpuPositions;
	if (generateCpuGeometry)
	{
		cpuPositions.reserve(numVertices);
	}
	alm::SignalListener vbUploadSignal;
	{
		alm::rhi::BufferDesc vertexBufferDesc;
		vertexBufferDesc.memoryAccess = alm::rhi::MemoryAccess::Default;
		vertexBufferDesc.shaderUsage = alm::rhi::BufferShaderUsage::ReadOnly;
		vertexBufferDesc.sizeBytes = numVertices * vertexSize;
		vertexBufferDesc.stride = vertexSize;

		auto verticesRequestResult = dataUploader->RequestUploadTicket(vertexBufferDesc);
		assert(verticesRequestResult);
		auto* vertex = (char*)verticesRequestResult->GetPtr();

		for (const Face& face : kFaces)
		{
			// Corner order: (0,0), (1,0), (1,1), (0,1) in face (u, v) space
			static constexpr float2 kCorners[4] = {
				float2{ 0.f, 0.f }, float2{ 1.f, 0.f }, float2{ 1.f, 1.f }, float2{ 0.f, 1.f }
			};

			for (const float2& corner : kCorners)
			{
				// Corner direction on the unit cube
				float3 direction = face.normal + face.tangentU * (corner.x * 2.f - 1.f) + face.tangentV * (corner.y * 2.f - 1.f);

				// position
				float3 position = direction * halfSize;
				*(float3*)vertex = position;
				vertex += sizeof(float3);
				if (generateCpuGeometry)
				{
					cpuPositions.push_back(position);
				}

				// normal
				if (hasNormals)
				{
					float3 normal = inwardNormals ? -face.normal : face.normal;
					*(uint32_t*)vertex = alm::math::VectorToSnorm8(normal);
					vertex += sizeof(uint32_t);
				}

				// uv
				if (hasUVs)
				{
					if (uvMode == CubeUVMode::UV2D)
					{
						// When viewed from the inside (inward faces) the face appears mirrored horizontally,
						// flip the u coordinate to compensate
						float u = inwardNormals ? 1.f - corner.x : corner.x;
						*(float2*)vertex = float2{ u, corner.y };
						vertex += sizeof(float2);
					}
					else
					{
						// 3d uvs: the unnormalized corner direction, ready to sample a cubemap
						*(float3*)vertex = direction;
						vertex += sizeof(float3);
					}
				}
			}
		}

		vertexBuffer = std::make_shared<alm::rhi::BufferOwner>();

		*vertexBuffer = m_Device->CreateBuffer(
			vertexBufferDesc, alm::rhi::ResourceState::COPY_DST, std::format("{} - VB", name));
		auto verticesUploadResult = dataUploader->CommitUploadBufferTicket(
			std::move(*verticesRequestResult),
			vertexBuffer->get_weak(),
			alm::rhi::ResourceState::COPY_DST,
			alm::rhi::ResourceState::SHADER_RESOURCE);
		assert(verticesUploadResult);
		vbUploadSignal = *verticesUploadResult;
	}

	// Generate indices (two triangles per face)
	std::shared_ptr<rhi::BufferOwner> indexBuffer;
	const uint32_t numIndices = 36;
	std::vector<uint32_t> cpuIndices;
	if (generateCpuGeometry)
	{
		cpuIndices.reserve(numIndices);
	}
	const bool idx32bits = numVertices > std::numeric_limits<uint16_t>::max() ? true : false;
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

		auto pushIndex = [&indices, &cpuIndices, generateCpuGeometry, idx32bits](uint32_t i)
		{
			if (generateCpuGeometry)
			{
				cpuIndices.push_back(i);
			}

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

		for (uint32_t face = 0; face < 6; ++face)
		{
			uint32_t base = face * 4;

			if (inwardNormals)
			{
				// CW when viewed from outside == CCW when viewed from inside
				pushIndex(base + 0);
				pushIndex(base + 2);
				pushIndex(base + 1);
				pushIndex(base + 0);
				pushIndex(base + 3);
				pushIndex(base + 2);
			}
			else
			{
				pushIndex(base + 0);
				pushIndex(base + 1);
				pushIndex(base + 2);
				pushIndex(base + 0);
				pushIndex(base + 2);
				pushIndex(base + 3);
			}
		}

		indexBuffer = std::make_shared<alm::rhi::BufferOwner>();

		*indexBuffer = m_Device->CreateBuffer(
			indexBufferDesc, alm::rhi::ResourceState::COPY_DST, std::format("{} - IB", name));
		auto uploadResult = dataUploader->CommitUploadBufferTicket(
			std::move(*requestResult),
			indexBuffer->get_weak(),
			alm::rhi::ResourceState::COPY_DST,
			alm::rhi::ResourceState::SHADER_RESOURCE);
		assert(uploadResult);
		ibUploadSignal = *uploadResult;
	}

	auto* mesh = new alm::gfx::Mesh{ m_Device, name.c_str(), "[generated]" };

	Mesh::VertexFormat vertexFormat{
		.VertexStride = vertexSize,
		.PositionOffset = 0,
		.NormalOffset = normalOffset,
		.TexCoord0Offset = hasUVs ? uvOffset : UINT32_MAX
	};
	mesh->SetVertexBuffer(vertexBuffer, vertexFormat);

	mesh->SetIndexBuffer(indexBuffer, rhi::PrimitiveTopology::TriangleList, idx32bits ? sizeof(uint32_t) : sizeof(uint16_t));

	if (generateCpuGeometry)
	{
		mesh->SetCpuPositions(std::move(cpuPositions));
		mesh->SetCpuIndices(std::move(cpuIndices));
	}

	if (opt_materialRef)
	{
		mesh->SetMaterial(*opt_materialRef);
	}
	else
	{
		MaterialRef mRef = m_MaterialManager->CreateNewMaterial(std::format("{} - Material", name));
		mRef->SetBaseColor(float3{ 1.f, 1.f, 1.f });
		mesh->SetMaterial(mRef);
	}

	mesh->SetBounds(aabox3f{ float3{-halfSize}, float3{halfSize} });

	// Lets wait until eveything is submitted.
	// We could also just return array of signals al let the caller wait
	vbUploadSignal.Wait();
	ibUploadSignal.Wait();

	return std::shared_ptr<alm::gfx::Mesh>{ mesh };
}

void alm::gfx::CommonResources::ClearTexture2D_RGBA(rhi::ICommandList* commandList, rhi::ITexture* texture, const float4& clearValue,
	rhi::ResourceState currentState, rhi::ResourceState finalState)
{
	const auto& desc = texture->GetDesc();

	assert(desc.dimension == rhi::TextureDimension::Texture2D);
	//assert(desc.format == rhi::Format::R32_FLOAT);
	assert(texture->GetStorageView().IsValid());

	if (currentState != rhi::ResourceState::UNORDERED_ACCESS)
	{
		commandList->PushBarrier(rhi::Barrier::Texture(texture, currentState, rhi::ResourceState::UNORDERED_ACCESS));
	}

	commandList->SetPipelineState(m_ClearTexture_RGBA_PSO.get());

	interop::ClearTextureConstants shaderConstants;
	shaderConstants.textureDI = texture->GetStorageView();
	shaderConstants.textureDim = float2{ desc.width, desc.height };
	shaderConstants.clearValue = clearValue;

	commandList->PushComputeConstants(0, shaderConstants);
	commandList->Dispatch(DivRoundUp(desc.width, 16u), DivRoundUp(desc.height, 16u), 1);

	if (finalState != rhi::ResourceState::UNORDERED_ACCESS)
	{
		commandList->PushBarrier(rhi::Barrier::Texture(texture, rhi::ResourceState::UNORDERED_ACCESS, finalState));
	}
}

void alm::gfx::CommonResources::ClearTexture2D_R(rhi::ICommandList* commandList, rhi::ITexture* texture, float clearValue,
	rhi::ResourceState currentState, rhi::ResourceState finalState)
{
	const auto& desc = texture->GetDesc();

	assert(desc.dimension == rhi::TextureDimension::Texture2D);
	//assert(desc.format == rhi::Format::R32_FLOAT);
	assert(texture->GetStorageView().IsValid());

	if (currentState != rhi::ResourceState::UNORDERED_ACCESS)
	{
		commandList->PushBarrier(rhi::Barrier::Texture(texture, currentState, rhi::ResourceState::UNORDERED_ACCESS));
	}

	commandList->SetPipelineState(m_ClearTexture_R_PSO.get());

	interop::ClearTextureConstants shaderConstants;
	shaderConstants.textureDI = texture->GetStorageView();
	shaderConstants.textureDim = float2{ desc.width, desc.height };
	shaderConstants.clearValue = float4{ clearValue };

	commandList->PushComputeConstants(0, shaderConstants);
	commandList->Dispatch(DivRoundUp(desc.width, 16u), DivRoundUp(desc.height, 16u), 1);

	if (finalState != rhi::ResourceState::UNORDERED_ACCESS)
	{
		commandList->PushBarrier(rhi::Barrier::Texture(texture, rhi::ResourceState::UNORDERED_ACCESS, finalState));
	}
}