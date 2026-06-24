#include "Gfx/GfxPCH.h"
#include "Gfx/Heightmap.h"
#include "Gfx/HeightmapSource.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/DataUploader.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"
#include "RHI/Device.h"
#include <glm/gtc/packing.hpp>

namespace
{
	const char* GetEdgeModeString(alm::gfx::Heightmap::EdgeMode edgeMode)
	{
		switch (edgeMode)
		{
		case alm::gfx::Heightmap::EdgeMode::Normal:
			return "Normal";
		case alm::gfx::Heightmap::EdgeMode::Low:
			return "Low";
		default:
			assert(0);
			return "";
		}
	}

	template<typename PIf>
	void TriangulateNorthWestCorner(const alm::gfx::Heightmap::PatchEdgeConfig& edgeConfig, uint32_t N, PIf pushIndex)
	{
		const uint32_t stride = N + 1;

		uint32_t vBL = (N - 2) * stride;		// bottom-left
		uint32_t vBM = (N - 2) * stride + 1;	// bottom-middle
		uint32_t vBR = (N - 2) * stride + 2;	// bottom-right
		uint32_t vML = vBL + stride;			// middle-left
		uint32_t vMM = vBM + stride;			// middle-middle
		uint32_t vMR = vBR + stride;			// middle-right
		uint32_t vTL = vML + stride;			// top-left
		uint32_t vTM = vMM + stride;			// top-middle
		uint32_t vTR = vMR + stride;			// top-right

		if (edgeConfig.North == alm::gfx::Heightmap::EdgeMode::Normal && edgeConfig.West == alm::gfx::Heightmap::EdgeMode::Normal)
		{
			pushIndex(vBL); pushIndex(vML); pushIndex(vBM);
			pushIndex(vML); pushIndex(vMM); pushIndex(vBM);

			pushIndex(vML); pushIndex(vTL); pushIndex(vMM);
			pushIndex(vTL); pushIndex(vTM); pushIndex(vMM);

			pushIndex(vTM); pushIndex(vMR); pushIndex(vMM);
			pushIndex(vTM); pushIndex(vTR); pushIndex(vMR);
		}
		else if (edgeConfig.North == alm::gfx::Heightmap::EdgeMode::Normal && edgeConfig.West == alm::gfx::Heightmap::EdgeMode::Low)
		{
			pushIndex(vBL); pushIndex(vMM); pushIndex(vBM);
			pushIndex(vBL); pushIndex(vTL); pushIndex(vMM);
			pushIndex(vTL); pushIndex(vTM); pushIndex(vMM);

			pushIndex(vTM); pushIndex(vMR); pushIndex(vMM);
			pushIndex(vTM); pushIndex(vTR); pushIndex(vMR);
		}
		else if (edgeConfig.North == alm::gfx::Heightmap::EdgeMode::Low && edgeConfig.West == alm::gfx::Heightmap::EdgeMode::Normal)
		{
			pushIndex(vBL); pushIndex(vML); pushIndex(vBM);
			pushIndex(vML); pushIndex(vMM); pushIndex(vBM);

			pushIndex(vML); pushIndex(vTL); pushIndex(vMM);
			pushIndex(vTL); pushIndex(vTR); pushIndex(vMM);
			pushIndex(vMM); pushIndex(vTR); pushIndex(vMR);
		}
		else if (edgeConfig.North == alm::gfx::Heightmap::EdgeMode::Low && edgeConfig.West == alm::gfx::Heightmap::EdgeMode::Low)
		{
			pushIndex(vBL); pushIndex(vMM); pushIndex(vBM);
			pushIndex(vBL); pushIndex(vTL); pushIndex(vMM);

			pushIndex(vTL); pushIndex(vTR); pushIndex(vMM);
			pushIndex(vMM); pushIndex(vTR); pushIndex(vMR);
		}
	}

	template<typename PIf>
	void TriangulateNorthEastCorner(const alm::gfx::Heightmap::PatchEdgeConfig& edgeConfig, uint32_t N, PIf pushIndex)
	{
		const uint32_t stride = N + 1;
		uint32_t vBL = (N - 2) * stride + (N - 2);	// bottom-left
		uint32_t vBM = (N - 2) * stride + (N - 1);	// bottom-middle
		uint32_t vBR = (N - 2) * stride + N;		// bottom-right
		uint32_t vML = vBL + stride;				// middle-left
		uint32_t vMM = vBM + stride;				// middle-middle
		uint32_t vMR = vBR + stride;				// middle-right
		uint32_t vTL = vML + stride;				// top-left
		uint32_t vTM = vMM + stride;				// top-middle
		uint32_t vTR = vMR + stride;				// top-right

		if (edgeConfig.North == alm::gfx::Heightmap::EdgeMode::Normal && edgeConfig.East == alm::gfx::Heightmap::EdgeMode::Normal)
		{
			pushIndex(vBL); pushIndex(vML); pushIndex(vBM);
			pushIndex(vML); pushIndex(vMM); pushIndex(vBM);

			pushIndex(vBM); pushIndex(vMM); pushIndex(vBR);
			pushIndex(vBR); pushIndex(vMM); pushIndex(vMR);

			pushIndex(vML); pushIndex(vTL); pushIndex(vMM);
			pushIndex(vTL); pushIndex(vTM); pushIndex(vMM);

			pushIndex(vMM); pushIndex(vTM); pushIndex(vMR);
			pushIndex(vTM); pushIndex(vTR); pushIndex(vMR);
		}
		else if (edgeConfig.North == alm::gfx::Heightmap::EdgeMode::Normal && edgeConfig.East == alm::gfx::Heightmap::EdgeMode::Low)
		{
			// East is Low: discard vMR (middle of east edge)
			pushIndex(vBL); pushIndex(vML); pushIndex(vBM);
			pushIndex(vML); pushIndex(vMM); pushIndex(vBM);

			pushIndex(vBM); pushIndex(vMM); pushIndex(vBR);

			pushIndex(vML); pushIndex(vTL); pushIndex(vMM);
			pushIndex(vTL); pushIndex(vTM); pushIndex(vMM);

			pushIndex(vBR); pushIndex(vMM); pushIndex(vTR);
			pushIndex(vMM); pushIndex(vTM); pushIndex(vTR);
		}
		else if (edgeConfig.North == alm::gfx::Heightmap::EdgeMode::Low && edgeConfig.East == alm::gfx::Heightmap::EdgeMode::Normal)
		{
			// North is Low: discard vTM (middle of north edge)
			pushIndex(vBL); pushIndex(vML); pushIndex(vBM);
			pushIndex(vML); pushIndex(vMM); pushIndex(vBM);

			pushIndex(vBM); pushIndex(vMM); pushIndex(vBR);
			pushIndex(vBR); pushIndex(vMM); pushIndex(vMR);

			pushIndex(vML); pushIndex(vTL); pushIndex(vMM);
			pushIndex(vTL); pushIndex(vTR); pushIndex(vMM);

			pushIndex(vMM); pushIndex(vTR); pushIndex(vMR);
		}
		else // North == Low && East == Low
		{
			// Discard both vTM and vMR
			pushIndex(vBL); pushIndex(vML); pushIndex(vBM);
			pushIndex(vML); pushIndex(vMM); pushIndex(vBM);

			pushIndex(vBM); pushIndex(vMM); pushIndex(vBR);

			pushIndex(vML); pushIndex(vTL); pushIndex(vMM);
			pushIndex(vTL); pushIndex(vTR); pushIndex(vMM);

			pushIndex(vBR); pushIndex(vMM); pushIndex(vTR);
		}
	}

	template<typename PIf>
	void TriangulateSouthEastCorner(const alm::gfx::Heightmap::PatchEdgeConfig& edgeConfig, uint32_t N, PIf pushIndex)
	{
		const uint32_t stride = N + 1;
		uint32_t vBL = 0 * stride + (N - 2);	// bottom-left
		uint32_t vBM = 0 * stride + (N - 1);	// bottom-middle
		uint32_t vBR = 0 * stride + N;			// bottom-right
		uint32_t vML = vBL + stride;
		uint32_t vMM = vBM + stride;
		uint32_t vMR = vBR + stride;
		uint32_t vTL = vML + stride;
		uint32_t vTM = vMM + stride;
		uint32_t vTR = vMR + stride;

		if (edgeConfig.South == alm::gfx::Heightmap::EdgeMode::Normal && edgeConfig.East == alm::gfx::Heightmap::EdgeMode::Normal)
		{
			pushIndex(vBL); pushIndex(vML); pushIndex(vBM);
			pushIndex(vML); pushIndex(vMM); pushIndex(vBM);

			pushIndex(vBM); pushIndex(vMM); pushIndex(vBR);
			pushIndex(vBR); pushIndex(vMM); pushIndex(vMR);

			pushIndex(vML); pushIndex(vTL); pushIndex(vMM);
			pushIndex(vTL); pushIndex(vTM); pushIndex(vMM);

			pushIndex(vMM); pushIndex(vTM); pushIndex(vMR);
			pushIndex(vTM); pushIndex(vTR); pushIndex(vMR);
		}
		else if (edgeConfig.South == alm::gfx::Heightmap::EdgeMode::Normal && edgeConfig.East == alm::gfx::Heightmap::EdgeMode::Low)
		{
			// East Low: discard vMR
			pushIndex(vBL); pushIndex(vML); pushIndex(vBM);
			pushIndex(vML); pushIndex(vMM); pushIndex(vBM);

			pushIndex(vBM); pushIndex(vMM); pushIndex(vBR);

			pushIndex(vML); pushIndex(vTL); pushIndex(vMM);
			pushIndex(vTL); pushIndex(vTM); pushIndex(vMM);

			pushIndex(vBR); pushIndex(vMM); pushIndex(vTR);
			pushIndex(vMM); pushIndex(vTM); pushIndex(vTR);
		}
		else if (edgeConfig.South == alm::gfx::Heightmap::EdgeMode::Low && edgeConfig.East == alm::gfx::Heightmap::EdgeMode::Normal)
		{
			// South Low: discard vBM
			pushIndex(vBL); pushIndex(vML); pushIndex(vMM);
			pushIndex(vBL); pushIndex(vMM); pushIndex(vBR);
			pushIndex(vBR); pushIndex(vMM); pushIndex(vMR);

			pushIndex(vML); pushIndex(vTL); pushIndex(vMM);
			pushIndex(vTL); pushIndex(vTM); pushIndex(vMM);

			pushIndex(vMM); pushIndex(vTM); pushIndex(vMR);
			pushIndex(vTM); pushIndex(vTR); pushIndex(vMR);
		}
		else // Both Low
		{
			// Discard vBM and vMR
			pushIndex(vBL); pushIndex(vML); pushIndex(vMM);
			pushIndex(vBL); pushIndex(vMM); pushIndex(vBR);
			pushIndex(vBR); pushIndex(vMM); pushIndex(vTR);

			pushIndex(vML); pushIndex(vTL); pushIndex(vMM);
			pushIndex(vTL); pushIndex(vTM); pushIndex(vMM);
			pushIndex(vMM); pushIndex(vTM); pushIndex(vTR);
		}
	}

	template<typename PIf>
	void TriangulateSouthWestCorner(const alm::gfx::Heightmap::PatchEdgeConfig& edgeConfig, uint32_t N, PIf pushIndex)
	{
		const uint32_t stride = N + 1;
		uint32_t vBL = 0 * stride;			// bottom-left
		uint32_t vBM = 0 * stride + 1;		// bottom-middle
		uint32_t vBR = 0 * stride + 2;		// bottom-right
		uint32_t vML = vBL + stride;
		uint32_t vMM = vBM + stride;
		uint32_t vMR = vBR + stride;
		uint32_t vTL = vML + stride;
		uint32_t vTM = vMM + stride;
		uint32_t vTR = vMR + stride;

		if (edgeConfig.South == alm::gfx::Heightmap::EdgeMode::Normal && edgeConfig.West == alm::gfx::Heightmap::EdgeMode::Normal)
		{
			pushIndex(vBL); pushIndex(vML); pushIndex(vBM);
			pushIndex(vML); pushIndex(vMM); pushIndex(vBM);

			pushIndex(vBM); pushIndex(vMM); pushIndex(vBR);
			pushIndex(vBR); pushIndex(vMM); pushIndex(vMR);

			pushIndex(vML); pushIndex(vTL); pushIndex(vMM);
			pushIndex(vTL); pushIndex(vTM); pushIndex(vMM);

			pushIndex(vMM); pushIndex(vTM); pushIndex(vMR);
			pushIndex(vTM); pushIndex(vTR); pushIndex(vMR);
		}
		else if (edgeConfig.South == alm::gfx::Heightmap::EdgeMode::Normal && edgeConfig.West == alm::gfx::Heightmap::EdgeMode::Low)
		{
			// West Low: discard vML
			pushIndex(vBL); pushIndex(vMM); pushIndex(vBM);

			pushIndex(vBM); pushIndex(vMM); pushIndex(vBR);
			pushIndex(vBR); pushIndex(vMM); pushIndex(vMR);

			pushIndex(vBL); pushIndex(vTL); pushIndex(vMM);
			pushIndex(vTL); pushIndex(vTM); pushIndex(vMM);

			pushIndex(vMM); pushIndex(vTM); pushIndex(vMR);
			pushIndex(vTM); pushIndex(vTR); pushIndex(vMR);
		}
		else if (edgeConfig.South == alm::gfx::Heightmap::EdgeMode::Low && edgeConfig.West == alm::gfx::Heightmap::EdgeMode::Normal)
		{
			// South Low: discard vBM
			pushIndex(vBL); pushIndex(vML); pushIndex(vMM);
			pushIndex(vBL); pushIndex(vMM); pushIndex(vBR);
			pushIndex(vBR); pushIndex(vMM); pushIndex(vMR);

			pushIndex(vML); pushIndex(vTL); pushIndex(vMM);
			pushIndex(vTL); pushIndex(vTM); pushIndex(vMM);

			pushIndex(vMM); pushIndex(vTM); pushIndex(vMR);
			pushIndex(vTM); pushIndex(vTR); pushIndex(vMR);
		}
		else // Both Low
		{
			// Discard vBM and vML
			pushIndex(vBL); pushIndex(vMM); pushIndex(vBR);
			pushIndex(vBR); pushIndex(vMM); pushIndex(vMR);

			pushIndex(vBL); pushIndex(vTL); pushIndex(vMM);
			pushIndex(vTL); pushIndex(vTM); pushIndex(vMM);

			pushIndex(vMM); pushIndex(vTM); pushIndex(vMR);
			pushIndex(vTM); pushIndex(vTR); pushIndex(vMR);
		}
	}
}

alm::gfx::Heightmap::Heightmap(DeviceManager* deviceManager) : m_DeviceManager{ deviceManager }
{}

alm::gfx::Heightmap::~Heightmap()
{}

void alm::gfx::Heightmap::Init(
	std::shared_ptr<IHeightmapSource> source,
	std::shared_ptr<TerrainMaterial> material,
	const uint2& textureResolution,
	uint32_t patchResolution)
{
	m_Source = source;
	m_Material = material;
	m_TextureResolution = textureResolution;
	m_PatchResolution = patchResolution;

	// Max number of patches for desired resolution (per axis)
	// Needs to be power of two to make sure subdivisiones are multiple of patch resolution
	m_BottomLevelPatchesCount = NextPowerOf2(std::max(m_TextureResolution.x, m_TextureResolution.y) / m_PatchResolution);
	m_MaxDepthLevel = static_cast<uint32_t>(std::floor(std::log2(m_BottomLevelPatchesCount)));

	m_ActualSize = float2{ (float)m_TextureResolution.x, (float)m_TextureResolution.y } * GetCellSize();
	m_VirtualSize = GetCellSize() * GetVirtualCellsCount();

	m_HeightsTexture = BuildTexture();
	BuildPatchVariants();
	ComputeBounds();

	BuildErrorPyramid();
}

void alm::gfx::Heightmap::SetTextureResolution(const uint2& textureResolution)
{
	m_TextureResolution = textureResolution;
	RefreshHeightsTexture();
}

void alm::gfx::Heightmap::RefreshHeightsTexture()
{
	auto newTexture = BuildTexture();
	m_HeightsTexture->Swap(*newTexture);
}

uint32_t alm::gfx::Heightmap::GetPatchIndicesCount(uint32_t variantIdx) const
{
	return m_PatchMeshVariants[variantIdx]->GetIndexCount();
}

float alm::gfx::Heightmap::GetHeight(const uint2& c) const
{
	if (m_Source->InfiniteDataResolution())
	{
		return m_Source->Sample(float2{ (float)c.x / m_TextureResolution.x, (float)c.y / m_TextureResolution.y });
	}
	else
	{
		return m_Source->GetHeight(c);
	}
}

uint32_t alm::gfx::Heightmap::GetVirtualCellsCount() const
{
	return m_BottomLevelPatchesCount * m_PatchResolution;
}

float2 alm::gfx::Heightmap::GetUVScale() const
{
	const uint32_t virtualCells = GetVirtualCellsCount();
	// Relation between virtual number of cells and data size
	return float2{ (float)virtualCells / m_TextureResolution.x, (float)virtualCells / m_TextureResolution.y };
}

float alm::gfx::Heightmap::GetCellSize() const
{
	if (m_Source->HasCellSize())
	{
		return m_Source->GetCellSize();
	}
	else
	{
		return 1.f / m_PatchResolution;
	}
}

float alm::gfx::Heightmap::GetPatchErrorValue(uint32_t level, const uint2& c)
{
	const auto patchIndex = GetErrorPyramidIndex(level, c);
	return m_ErrorPyramid[patchIndex];
}

uint32_t alm::gfx::Heightmap::EdgeConfigToVariantIndex(const PatchEdgeConfig& config)
{
	return (uint32_t)config.North + (uint32_t)config.South * 2 + (uint32_t)config.East * 4 + (uint32_t)config.West * 8;
}

void alm::gfx::Heightmap::ComputeBounds()
{
	const float2 heightRange = m_Source->GetHeightRange();

	// Actually if m_Source->IsTileable bounds in x/y are infinite?
	m_Bounds.min = float3{ 0.f, heightRange.x, 0.f };
	m_Bounds.max = float3{ m_ActualSize.x, heightRange.y, m_ActualSize.y };
}

alm::rhi::TextureOwner alm::gfx::Heightmap::BuildTexture()
{
	auto* dataUploader = m_DeviceManager->GetDataUploader();
	auto* device = m_DeviceManager->GetDevice();

	rhi::TextureDesc texDesc{
		.width = m_TextureResolution.x,
		.height = m_TextureResolution.y,
		.mipLevels = (uint32_t)floor(log2(std::max(m_TextureResolution.x, m_TextureResolution.y))) + 1,
		.format = rhi::Format::R16_FLOAT,
		.shaderUsage = rhi::TextureShaderUsage::Sampled | rhi::TextureShaderUsage::Storage };

	auto requestResult = dataUploader->RequestUploadTicket(texDesc);
	assert(requestResult);

	const rhi::SubresourceCopyableRequirements copyReq = device->GetSubresourceCopyableRequirements(texDesc, 0, 0);
	for (uint32_t y = 0; y < texDesc.height; ++y)
	{
		uint16_t* rowData = (uint16_t*)((uint8_t*)requestResult->GetPtr() + y * (copyReq.rowStride));
		for (uint32_t x = 0; x < texDesc.width; ++x)
		{
			rowData[x] = glm::packHalf1x16(GetHeight({ x, y }));
		}
	}

	rhi::TextureOwner texture = device->CreateTexture(texDesc, rhi::ResourceState::COPY_DST, m_Source->GetName());
	assert(texture);

	auto uploadResult = dataUploader->CommitUploadTextureTicket(
		std::move(*requestResult),
		texture.get_weak(),
		rhi::ResourceState::COPY_DST,
		rhi::ResourceState::SHADER_RESOURCE,
		rhi::TextureSubresourceSet{ 0, 1, 0, 1 }, // first mip only
		DataUploader::GenMipsMethod::GenMips_Color
	);
	assert(uploadResult);

	uploadResult->Wait();
	return texture;
}

void alm::gfx::Heightmap::BuildPatchVariants()
{
	auto* dataUploader = m_DeviceManager->GetDataUploader();
	auto* device = m_DeviceManager->GetDevice();

	// Generate vertices
	// Position : float2
	const uint32_t vertexSize = sizeof(float2);
	auto vertexBuffer = std::make_shared<rhi::BufferOwner>();
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

		*vertexBuffer = device->CreateBuffer(vertexBufferDesc, alm::rhi::ResourceState::COPY_DST, "HeightmapPatch VB");
		auto verticesUploadResult = dataUploader->CommitUploadBufferTicket(
			std::move(*verticesRequestResult),
			vertexBuffer->get_weak(),
			alm::rhi::ResourceState::COPY_DST,
			alm::rhi::ResourceState::SHADER_RESOURCE);
		assert(verticesUploadResult);
		vbUploadSignal = *verticesUploadResult;
	}

	// Generate index buffers
	const bool idx32bits = true;
	std::array<std::shared_ptr<rhi::BufferOwner>, kPatchMeshVariantsCount> indexBuffers;
	for (auto north : { EdgeMode::Normal, EdgeMode::Low })
	{
		for (auto south : { EdgeMode::Normal, EdgeMode::Low })
		{
			for (auto east : { EdgeMode::Normal, EdgeMode::Low })
			{
				for (auto west : { EdgeMode::Normal, EdgeMode::Low })
				{
					PatchEdgeConfig edgeConfig{ .North = north, .South = south, .East = east, .West = west };
					const uint32_t variantIndex = EdgeConfigToVariantIndex(edgeConfig);

					indexBuffers[variantIndex] = CreateIndexBufferVariant(edgeConfig, idx32bits);
				}
			}
		}
	}

	// Create meshes
	for (uint32_t variantIndex = 0; variantIndex < kPatchMeshVariantsCount; ++variantIndex)
	{
		m_PatchMeshVariants[variantIndex] = alm::make_unique_with_weak<Mesh>(device, "HeightmapPatch", "[generated]");

		Mesh::VertexFormat vertexFormat{
			.VertexStride = vertexSize,
			.PositionOffset = 0 };

		m_PatchMeshVariants[variantIndex]->SetVertexBuffer(
			vertexBuffer, vertexFormat);
		m_PatchMeshVariants[variantIndex]->SetIndexBuffer(
			indexBuffers[variantIndex], rhi::PrimitiveTopology::TriangleList, idx32bits ? sizeof(uint32_t) : sizeof(uint16_t));

		m_PatchMeshVariants[variantIndex]->SetTerrainMaterial(m_Material);

		// Actually, this is irrelevant
		m_PatchMeshVariants[variantIndex]->SetBounds(aabox3f{ float3(0.f, 0.f, 0.f), float3(1.f, 1.f, 1.f) });
	}
}

std::shared_ptr<alm::rhi::BufferOwner> alm::gfx::Heightmap::CreateIndexBufferVariant(const PatchEdgeConfig& edgeConfig, const bool indices32Bits) const
{
/*
		┌───┬──────────┬───┐
		│ NW│  North   │ NE│
		├───┼──────────┼───┤
		│ W │ Interior │ E │
		│   │          │   │
		│   │          │   │
		├───┼──────────┼───┤
		│ SW│  South   │ SE│
		└───┴──────────┴───┘
*/

	auto* dataUploader = m_DeviceManager->GetDataUploader();
	auto* device = m_DeviceManager->GetDevice();

	std::vector<uint32_t> indices;
	const uint32_t stride = m_PatchResolution + 1;

	auto pushIndex = [&](const uint32_t idx)
	{
		indices.push_back(idx);
	};

	auto triangulateNormalCell = [&](uint32_t x, uint32_t y)
	{
		uint32_t v00 = y * stride + x;	// bottom-left
		uint32_t v10 = v00 + 1;			// bottom-right
		uint32_t v01 = v00 + stride;	// top-left
		uint32_t v11 = v01 + 1;			// top-right

		// Front face CCW
		pushIndex(v00); pushIndex(v01); pushIndex(v10);
		pushIndex(v10); pushIndex(v01); pushIndex(v11);
	};

	auto triangulateLowNorthCell = [&](uint32_t k)
	{
		uint32_t y0 = m_PatchResolution - 1;
		uint32_t y1 = m_PatchResolution;

		uint32_t vA = y0 * stride + (2 * k);		// bottom-left
		uint32_t vB = y0 * stride + (2 * k + 1);	// bottom-middle
		uint32_t vC = y0 * stride + (2 * k + 2);	// bottom-right
		uint32_t vD = y1 * stride + (2 * k);		// top-left
		uint32_t vF = y1 * stride + (2 * k + 2);	// top-right

		pushIndex(vA); pushIndex(vD); pushIndex(vB);
		pushIndex(vB); pushIndex(vD); pushIndex(vF);
		pushIndex(vB); pushIndex(vF); pushIndex(vC);
	};

	auto triangulateLowSouthCell = [&](uint32_t k)
	{
		uint32_t y0 = 0;
		uint32_t y1 = 1;

		uint32_t vA = y0 * stride + (2 * k);		// bottom-left
		uint32_t vC = y0 * stride + (2 * k + 2);	// bottom-right
		uint32_t vD = y1 * stride + (2 * k);		// top-left
		uint32_t vE = y1 * stride + (2 * k + 1);	// top-middle
		uint32_t vF = y1 * stride + (2 * k + 2);	// top-right

		pushIndex(vA); pushIndex(vD); pushIndex(vE);
		pushIndex(vA); pushIndex(vE); pushIndex(vC);
		pushIndex(vC); pushIndex(vE); pushIndex(vF);
	};

	auto triangulateLowEastCell = [&](uint32_t k)
	{
		// Neighbour is right border (x=N)
		uint32_t x0 = m_PatchResolution - 1;
		uint32_t x1 = m_PatchResolution;

		uint32_t vA = (2 * k)	  * stride + x0;	// bottom-left
		uint32_t vB = (2 * k + 1) * stride + x0;	// mid-left
		uint32_t vC = (2 * k + 2) * stride + x0;	// top-left
		uint32_t vD = (2 * k)	  * stride + x1;	// bottom-right
		uint32_t vE = (2 * k + 2) * stride + x1;	// top-right

		pushIndex(vA); pushIndex(vB); pushIndex(vD);
		pushIndex(vD); pushIndex(vB); pushIndex(vE);
		pushIndex(vB); pushIndex(vC); pushIndex(vE);
	};

	auto triangulateLowWestCell = [&](uint32_t k)
	{
		// Neighbour is left border (x=N)
		uint32_t x0 = 0;
		uint32_t x1 = 1;

		uint32_t vA = (2 * k)     * stride + x0;	// bottom-left
		uint32_t vC = (2 * k + 2) * stride + x0;	// top-left
		uint32_t vD = (2 * k)     * stride + x1;	// bottom-right
		uint32_t vE = (2 * k + 1) *	stride + x1;	// mid-right
		uint32_t vF = (2 * k + 2) * stride + x1;	// top-right
		
		pushIndex(vA); pushIndex(vE); pushIndex(vD);
		pushIndex(vA); pushIndex(vC); pushIndex(vE);
		pushIndex(vC); pushIndex(vF); pushIndex(vE);
	};

	for (uint32_t y = 1; y < m_PatchResolution - 1; ++y)
	{
		for (uint32_t x = 1; x < m_PatchResolution - 1; ++x)
			triangulateNormalCell(x, y);
	}

	if (edgeConfig.North == EdgeMode::Normal)
	{
		for (uint32_t x = 2; x < m_PatchResolution - 2; ++x)
			triangulateNormalCell(x, m_PatchResolution - 1);
	}
	else
	{
		for (uint32_t x = 1; x < (m_PatchResolution / 2) - 1; ++x)
			triangulateLowNorthCell(x);
	}

	if (edgeConfig.South == EdgeMode::Normal)
	{
		for (uint32_t x = 2; x < m_PatchResolution - 2; ++x)
			triangulateNormalCell(x, 0);
	}
	else
	{
		for (uint32_t x = 1; x < (m_PatchResolution / 2) - 1; ++x)
			triangulateLowSouthCell(x);
	}

	if (edgeConfig.East == EdgeMode::Normal)
	{
		for (uint32_t y = 2; y < m_PatchResolution - 2; ++y)
			triangulateNormalCell(m_PatchResolution - 1, y);
	}
	else
	{
		for (uint32_t y = 1; y < (m_PatchResolution / 2) - 1; ++y)
			triangulateLowEastCell(y);
	}

	if (edgeConfig.West == EdgeMode::Normal)
	{
		for (uint32_t y = 2; y < m_PatchResolution - 2; ++y)
			triangulateNormalCell(0, y);
	}
	else
	{
		for (uint32_t y = 1; y < (m_PatchResolution / 2) - 1; ++y)
			triangulateLowWestCell(y);
	}

	TriangulateNorthEastCorner(edgeConfig, m_PatchResolution, pushIndex);
	TriangulateNorthWestCorner(edgeConfig, m_PatchResolution, pushIndex);
	TriangulateSouthEastCorner(edgeConfig, m_PatchResolution, pushIndex);
	TriangulateSouthWestCorner(edgeConfig, m_PatchResolution, pushIndex);

	alm::rhi::BufferDesc indexBufferDesc;
	indexBufferDesc.memoryAccess = alm::rhi::MemoryAccess::Default;
	indexBufferDesc.shaderUsage = alm::rhi::BufferShaderUsage::ReadOnly;
	indexBufferDesc.sizeBytes = indices.size() * sizeof(uint32_t);
	indexBufferDesc.stride = sizeof(int32_t);

	std::stringstream ss;
	ss << "HeightmapPatchIB[North:"
		<< GetEdgeModeString(edgeConfig.North) << "][South:"
		<< GetEdgeModeString(edgeConfig.South) << "][East:"
		<< GetEdgeModeString(edgeConfig.East) << "][West:"
		<< GetEdgeModeString(edgeConfig.West) << "]";

	auto indexBuffer = std::make_shared<rhi::BufferOwner>();
	*indexBuffer = device->CreateBuffer(indexBufferDesc, rhi::ResourceState::COPY_DST, ss.str());

	auto uploadResult = dataUploader->UploadBufferData(
		WeakBlob{ (uint8_t*)indices.data(), indices.size() * sizeof(uint32_t) },
		indexBuffer->get_weak(),
		rhi::ResourceState::COPY_DST,
		rhi::ResourceState::SHADER_RESOURCE);

	assert(uploadResult);	
	uploadResult->Wait();

	return indexBuffer;
}

// Error is "max absolute deviation from mean"
void alm::gfx::Heightmap::BuildErrorPyramid()
{
	// We have to store the error for each node of each level
	size_t size = 0;
	for (int i = 0; i <= m_MaxDepthLevel; ++i)
	{
		size += square(m_BottomLevelPatchesCount >> (m_MaxDepthLevel - i));
	}
	m_ErrorPyramid.resize(size);

	// Start with the bottom level.
	for (int patchX = 0; patchX < m_BottomLevelPatchesCount; ++patchX)
	{
		for (int patchY = 0; patchY < m_BottomLevelPatchesCount; ++patchY)
		{
			const auto patchIndex = GetErrorPyramidIndex(m_MaxDepthLevel, { patchX, patchY });

			// Cache samples
			std::vector<float> samples;
			samples.reserve(square(m_PatchResolution));

			for (int x = patchX * m_PatchResolution; x < (patchX + 1) * m_PatchResolution; ++x)
			{
				for (int y = patchY * m_PatchResolution; y < (patchY + 1) * m_PatchResolution; ++y)
				{
					if (x < m_TextureResolution.x && y < m_TextureResolution.y)
					{
						samples.push_back(GetHeight({ x, y }));
					}
				}
			}

			if (samples.empty())
			{
				m_ErrorPyramid[patchIndex] = 0.f;
				continue;
			}

			// Calc mean
			float sumH = std::accumulate(samples.begin(), samples.end(), 0.f);
			float mean = sumH / samples.size();

			// Calc max deviation from mean
			float maxDev = 0.f;
			for (float H : samples)
			{
				maxDev = std::max(maxDev, std::abs(H - mean));
			}

			m_ErrorPyramid[patchIndex] = maxDev;
		}
	}

	// Propagate to upper levels

	if (m_MaxDepthLevel == 0)
		return;

	for (int level = m_MaxDepthLevel - 1; level >= 0; --level)
	{
		int patchesInLevel = m_BottomLevelPatchesCount >> (m_MaxDepthLevel - level);
		for (int patchX = 0; patchX < patchesInLevel; ++patchX)
		{
			for (int patchY = 0; patchY < patchesInLevel; ++patchY)
			{
				float maxDev = 0.f;
				for (int childIdx = 0; childIdx < 4; ++childIdx)
				{
					const auto childPatchIndex = GetErrorPyramidIndex(level + 1,
						{ patchX * 2 + childIdx % 2,
						  patchY * 2 + childIdx / 2 });

					maxDev = std::max(maxDev, m_ErrorPyramid[childPatchIndex]);
				}

				const auto patchIndex = GetErrorPyramidIndex(level, { patchX, patchY });
				m_ErrorPyramid[patchIndex] = maxDev;
			}
		}
	}
}

uint32_t alm::gfx::Heightmap::GetErrorPyramidIndex(uint32_t level, const uint2& coords)
{
	const uint64_t offset = (square(m_BottomLevelPatchesCount >> (m_MaxDepthLevel - level)) - 1) / 3;
	const uint64_t stride = m_BottomLevelPatchesCount >> (m_MaxDepthLevel - level);

	return offset + coords.y * stride + coords.x;
}