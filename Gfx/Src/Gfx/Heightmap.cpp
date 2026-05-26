#include "Gfx/GfxPCH.h"
#include "Gfx/Heightmap.h"
#include "Gfx/HeightmapSource.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/DataUploader.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"
#include "RHI/Device.h"
#include <glm/gtc/packing.hpp>

alm::gfx::Heightmap::Heightmap(
	DeviceManager* deviceManager,
	std::shared_ptr<IHeightmapSource> source,
	std::shared_ptr<TerrainMaterial> material,
	const float2& uvScale,
	const float2& uvOffset,
	uint32_t patchResolution) :
	m_DeviceManager{ deviceManager },
	m_Source{ source },
	m_uvScale{ uvScale },
	m_uvOffset{ uvOffset },
	m_PatchResolution{ patchResolution }
{
	BuildTexture();
	BuildPatch(material);
	ComputeBounds();
}

alm::gfx::Heightmap::~Heightmap()
{}

float alm::gfx::Heightmap::Sample(const float2& uv) const
{
	const float2 sourceUV = uv * m_uvScale + m_uvOffset;
	return m_Source->Sample(sourceUV);
}

void alm::gfx::Heightmap::ComputeBounds()
{
	const float2 heightRange = m_Source->GetHeightRange();
	const float2 normSize = m_Source->GetNormalizedSize();

	// Actually if m_Source->IsTileable bounds in x/y are infinite?
	m_Bounds.min = float3{ 0.f, heightRange.x, 0.f };
	m_Bounds.max = float3{ normSize.x, heightRange.y, normSize.y };
}

void alm::gfx::Heightmap::BuildTexture()
{
	auto* dataUploader = m_DeviceManager->GetDataUploader();
	auto* device = m_DeviceManager->GetDevice();

	rhi::TextureDesc desc{
		.width = m_Source->GetDataSize().x,
		.height = m_Source->GetDataSize().y,
		.format = rhi::Format::R16_FLOAT,
		.shaderUsage = rhi::TextureShaderUsage::Sampled };

	auto requestResult = dataUploader->RequestUploadTicket(desc);
	assert(requestResult);

	const rhi::SubresourceCopyableRequirements copyReq = device->GetSubresourceCopyableRequirements(desc, 0, 0);
	for (uint32_t y = 0; y < desc.height; ++y)
	{
		uint16_t* rowData = (uint16_t*)((uint8_t*)requestResult->GetPtr() + y * (copyReq.rowStride));
		for (uint32_t x = 0; x < desc.width; ++x)
		{
			rowData[x] = glm::packHalf1x16(m_Source->GetHeight(uint2{ x, y }));
		}
	}

	m_HeightsTexture = device->CreateTexture(desc, rhi::ResourceState::COPY_DST, m_Source->GetName());
	assert(m_HeightsTexture);

	auto uploadResult = dataUploader->CommitUploadTextureTicket(
		std::move(*requestResult),
		m_HeightsTexture.get_weak(),
		rhi::ResourceState::COPY_DST,
		rhi::ResourceState::SHADER_RESOURCE);
	assert(uploadResult);

	uploadResult->Wait();
}

void alm::gfx::Heightmap::BuildPatch(std::shared_ptr<TerrainMaterial> mat)
{
	auto* dataUploader = m_DeviceManager->GetDataUploader();
	auto* device = m_DeviceManager->GetDevice();

	// Generate vertices
	// Position : float2
	const uint32_t vertexSize = sizeof(float2);
	alm::rhi::BufferOwner vertexBuffer;
	alm::SignalListener vbUploadSignal;
	const uint32_t numVertices = (m_PatchResolution + 1) * (m_PatchResolution + 1);
	{
		alm::rhi::BufferDesc vertexBufferDesc;
		vertexBufferDesc.memoryAccess = alm::rhi::MemoryAccess::Default;
		vertexBufferDesc.shaderUsage = alm::rhi::BufferShaderUsage::ReadOnly;
		vertexBufferDesc.sizeBytes = numVertices * vertexSize;
		vertexBufferDesc.stride = vertexSize;

		auto verticesRequestResult = dataUploader->RequestUploadTicket(vertexBufferDesc);
		assert(verticesRequestResult);
		auto* vertex = (char*)verticesRequestResult->GetPtr();

		for (uint32_t y = 0; y <= m_PatchResolution; ++y)
		{
			for (uint32_t x = 0; x <= m_PatchResolution; ++x)
			{
				*(float2*)vertex = { (float)x / m_PatchResolution, (float)y / m_PatchResolution };
				vertex += sizeof(float2);
			}
		}

		vertexBuffer = device->CreateBuffer(vertexBufferDesc, alm::rhi::ResourceState::COPY_DST, "HeightmapPatch VB");
		auto verticesUploadResult = dataUploader->CommitUploadBufferTicket(
			std::move(*verticesRequestResult),
			vertexBuffer.get_weak(),
			alm::rhi::ResourceState::COPY_DST,
			alm::rhi::ResourceState::SHADER_RESOURCE);
		assert(verticesUploadResult);
		vbUploadSignal = *verticesUploadResult;
	}

	// Generate indices (two triangles per cell)
	const uint32_t numIndices = 6 * m_PatchResolution * m_PatchResolution;
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
			if (idx32bits)
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

		for (uint32_t y = 0; y < m_PatchResolution; ++y)
		{
			for (uint32_t x = 0; x < m_PatchResolution; ++x)
			{
				uint32_t v00 = y * (m_PatchResolution + 1) + x;	// bottom-left
				uint32_t v10 = v00 + 1;							// bottom-right
				uint32_t v01 = v00 + m_PatchResolution + 1;		// top-left
				uint32_t v11 = v01 + 1;							// top-right
			
				// Front face CCW
				pushIndex(v00); pushIndex(v01); pushIndex(v10);
				pushIndex(v10); pushIndex(v01); pushIndex(v11);
			}
		}

		indexBuffer = device->CreateBuffer(indexBufferDesc, alm::rhi::ResourceState::COPY_DST, "HeightmapPatch IB");
		auto uploadResult = dataUploader->CommitUploadBufferTicket(
			std::move(*requestResult),
			indexBuffer.get_weak(),
			alm::rhi::ResourceState::COPY_DST,
			alm::rhi::ResourceState::SHADER_RESOURCE);
		assert(uploadResult);
		ibUploadSignal = *uploadResult;
	}

	m_PatchMesh = alm::make_unique_with_weak<Mesh>(device, "HeightmapPatch", "[generated]");

	Mesh::VertexFormat vertexFormat{
		.VertexStride = vertexSize,
		.PositionOffset = 0 };

	m_PatchMesh->SetVertexBuffer(std::move(vertexBuffer), vertexFormat);
	m_PatchMesh->SetIndexBuffer(std::move(indexBuffer), rhi::PrimitiveTopology::TriangleList, idx32bits ? sizeof(uint32_t) : sizeof(uint16_t));

#if 0
	// TODO: Lets create a simple material for me moment, so we see something
	auto material = std::make_shared<Material>("HeightmapPatch");
	material->SetBaseColor(float3{ 0.5f, 1.f, 1.f });
	m_PatchMesh->SetMaterial(std::shared_ptr<Material>{ material });
#else
	m_PatchMesh->SetTerrainMaterial(mat);
#endif

	// Actually, this is irrelevant
	m_PatchMesh->SetBounds(aabox3f{ float3(0.f, 0.f, 0.f), float3(1.f, 0.f, 1.f) });
}

bool alm::gfx::Heightmap::InfiniteDepthLevel() const
{
	return m_Source && m_Source->InfiniteDataResolution();
}

uint32_t alm::gfx::Heightmap::GetMaxDepthLevel() const
{
	if (!m_Source)
		return 0;
	if (m_Source->InfiniteDataResolution())
		return UINT32_MAX;

	uint2 dataResolution = m_Source->GetDataResolution();
	uint32_t maxRes = std::max(dataResolution.x, dataResolution.y);
	uint32_t maxLevel = static_cast<uint32_t>(std::floor(std::log2(maxRes / m_PatchResolution)));

	return maxLevel;
}

uint32_t alm::gfx::Heightmap::GetPatchIndicesCount() const
{
	return 6 * m_PatchResolution * m_PatchResolution;
}