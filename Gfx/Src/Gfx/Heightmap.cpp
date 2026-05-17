#include "Gfx/GfxPCH.h"
#include "Gfx/Heightmap.h"
#include "Gfx/HeightmapSource.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/DataUploader.h"
#include "RHI/Device.h"
#include <glm/gtc/packing.hpp>

alm::gfx::Heightmap::Heightmap(
	DeviceManager* deviceManager,
	std::shared_ptr<IHeightmapSource> source,
	const float2& uvScale,
	const float2& uvOffset,
	float heightScale,
	float heightOffset) : 
	m_DeviceManager{ deviceManager },
	m_Source{ source },
	m_uvScale{ uvScale },
	m_uvOffset{ uvOffset },
	m_HeightScale{ heightScale },
	m_HeightOffset{ heightOffset }
{
	BuildTexture();
	ComputeBounds();
}

alm::gfx::Heightmap::~Heightmap()
{}

float alm::gfx::Heightmap::Sample(const float2& uv) const
{
	const float2 sourceUV = uv * m_uvScale + m_uvOffset;
	return m_Source->Sample(sourceUV) * m_HeightScale + m_HeightOffset;
}

void alm::gfx::Heightmap::ComputeBounds()
{
	const float2 heightRange = m_Source->GetHeightRange();
	const float2 normSize = m_Source->GetNormalizedSize();

	// Actually if m_Source->IsTileable bounds in x/y are infinite?
	m_Bounds.min = float3{ 0.f, heightRange.x * m_HeightScale + m_HeightOffset, 0.f };
	m_Bounds.max = float3{ normSize.x, heightRange.y * m_HeightScale + m_HeightOffset, normSize.y };
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