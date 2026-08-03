#include "Gfx/GfxPCH.h"
#include "Gfx/AtmosphereConfig.h"
#include "Gfx/TextureCache.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/DataUploader.h"
#include "Gfx/LoadedTexture.h"
#include "Gfx/TextureLoader.h"
#include "RHI/Texture.h"
#include "RHI/Device.h"
#include "Core/Noise.h"

std::expected<std::pair<alm::rhi::TextureOwner, alm::SignalListener>, std::string>
static CreateCloudsShapeTexture(alm::gfx::DeviceManager* deviceManager)
{
	auto* device = deviceManager->GetDevice();
	auto* dataUploader = deviceManager->GetDataUploader();
	constexpr int SIZE = 128;

	alm::rhi::TextureDesc desc{
		.width = SIZE,
		.height = SIZE,
		.depth = SIZE,
		.format = alm::rhi::Format::RGBA8_UNORM,
		.dimension = alm::rhi::TextureDimension::Texture3D,
		.shaderUsage = alm::rhi::TextureShaderUsage::Sampled };

	alm::rhi::TextureOwner texture = device->CreateTexture(desc, alm::rhi::ResourceState::COPY_DST, "CloudsShapeTexture");

	auto requestResult = dataUploader->RequestUploadTicket(desc, alm::rhi::AllSubresources);
	assert(requestResult);
	auto& ticket = *requestResult;

	const auto copyReq = device->GetSubresourceCopyableRequirements(desc, 0, 0);
	const uint32_t layerSize = copyReq.rowStride * copyReq.numRows;

	for (int z = 0; z < SIZE; ++z)
	{
		char* layerStart = (char*)ticket.GetPtr() + copyReq.offset + (z * layerSize);

		for (int y = 0; y < SIZE; ++y)
		{
			uint32_t* data = (uint32_t*)(layerStart + (y * copyReq.rowStride));
			for (int x = 0; x < SIZE; ++x)
			{
				float3 st = float3{ x, y, z } / (float)SIZE;
				float3 stG = st;
				float3 stB = st + float3(0.5f, 0.5f, 0.5f);
				float3 stA = st + float3(0.25f, 0.75f, 0.5f);

				float g = alm::WorleyNoise(stG * 4.f, 4.f);
				float b = alm::WorleyNoise(stB * 9.f, 9.f);
				float a = alm::WorleyNoise(stA * 19.f, 19.f);

				float pfbm = alm::PerlinFbm(st, 4.f, 7);
				pfbm = std::lerp(pfbm, 1.0f, 0.5f);

				float perlin = std::lerp(1.0f, pfbm, 0.9f);
				float worley = std::lerp(1.0f, g, 0.7f);
				float r = perlin * worley;

				*data = alm::MakeRGBA(r * 255.f, g * 255.f, b * 255.f, a * 255.f);
				++data;
			}
		}
	}

	auto commitResult = dataUploader->CommitUploadTextureTicket(std::move(ticket), texture.get_weak(),
		alm::rhi::ResourceState::COPY_DST, alm::rhi::ResourceState::SHADER_RESOURCE);
	if (!commitResult)
	{
		return std::unexpected(commitResult.error());
	}

	return std::make_pair(std::move(texture), *commitResult);
}

std::expected<std::pair<alm::rhi::TextureOwner, alm::SignalListener>, std::string>
static CreateCloudsDetailTexture(alm::gfx::DeviceManager* deviceManager)
{
	auto* device = deviceManager->GetDevice();
	auto* dataUploader = deviceManager->GetDataUploader();

	constexpr int SIZE = 32;
	alm::rhi::TextureDesc desc{
		.width = SIZE, .height = SIZE, .depth = SIZE,
		.format = alm::rhi::Format::RGBA8_UNORM,
		.dimension = alm::rhi::TextureDimension::Texture3D,
		.shaderUsage = alm::rhi::TextureShaderUsage::Sampled };

	alm::rhi::TextureOwner texture = device->CreateTexture(desc,
		alm::rhi::ResourceState::COPY_DST, "CloudsDetailTexture");

	auto requestResult = dataUploader->RequestUploadTicket(desc, alm::rhi::AllSubresources);
	assert(requestResult);
	auto& ticket = *requestResult;

	const auto copyReq = device->GetSubresourceCopyableRequirements(desc, 0, 0);
	const uint32_t layerSize = copyReq.rowStride * copyReq.numRows;

	for (int z = 0; z < SIZE; ++z)
	{
		char* layerStart = (char*)ticket.GetPtr() + copyReq.offset + (z * layerSize);

		for (int y = 0; y < SIZE; ++y)
		{
			uint32_t* data = (uint32_t*)(layerStart + (y * copyReq.rowStride));
			for (int x = 0; x < SIZE; ++x)
			{
				float3 st = float3{ x, y, z } / (float)SIZE;

				float r = alm::WorleyNoise(st * 8.f, 8.f); // DETAIL_LOW_FREQ in the shader
				float g = alm::WorleyNoise(st * 14.f, 14.f);
				float b = alm::WorleyNoise(st * 22.f, 22.f);

				// Alpha to 1 (placeholder)
				*data = alm::MakeRGBA(r * 255.f, g * 255.f, b * 255.f, 255);
				++data;
			}
		}
	}

	auto commitResult = dataUploader->CommitUploadTextureTicket(std::move(ticket), texture.get_weak(),
		alm::rhi::ResourceState::COPY_DST, alm::rhi::ResourceState::SHADER_RESOURCE);
	if (!commitResult)
		return std::unexpected(commitResult.error());

	return std::make_pair(std::move(texture), *commitResult);
}

float3 alm::gfx::AtmosphereConfig::GetSunDirection() const
{
	const float3 sunDir = alm::ElevationAzimuthRadToDir(
		glm::radians(Sun.ElevationDeg), glm::radians(Sun.AzimuthDeg));

	return sunDir;
}

alm::rhi::TextureHandle alm::gfx::AtmosphereConfig::GetCloudsShapeTexture() const
{
	return m_CloudsShapeTexture.valid() ? m_CloudsShapeTexture.get_weak() : nullptr;
}

alm::rhi::TextureHandle alm::gfx::AtmosphereConfig::GetCloudsDetailTexture() const
{
	return m_CloudsDetailTexture.valid() ? m_CloudsDetailTexture.get_weak() : nullptr;
}

void alm::gfx::AtmosphereConfig::InitCloudsTextures(bool forceNew, bool cache, alm::gfx::DeviceManager* deviceManager)
{
	if (std::filesystem::exists("_generated/CloudShape.dds"))
	{
		alm::gfx::TextureCache* textureCache = deviceManager->GetTextureCache();
		auto loadResult = textureCache->Load("_generated/CloudShape.dds", alm::gfx::TextureCache::Flags::None);
		if (!loadResult)
		{
			LOG_ERROR("Failed loading CloudsShape.dds\n{}", loadResult.error());
		}
		else
		{
			loadResult->second.Wait();
			m_CloudsShapeTexture = std::move(loadResult->first->texture);
		}
	}
	else
	{
		auto createResult = CreateCloudsShapeTexture(deviceManager);
		assert(createResult);
		createResult->second.Wait();
		alm::rhi::TextureOwner& cloudsTexture = createResult->first;

		alm::gfx::SaveDDSTexture(cloudsTexture.get_weak(), alm::rhi::ResourceState::SHADER_RESOURCE, alm::rhi::ResourceState::SHADER_RESOURCE,
			deviceManager->GetDevice(), "_generated/CloudShape.dds");

		m_CloudsShapeTexture = std::move(cloudsTexture);
	}

	if (std::filesystem::exists("_generated/CloudsDetail.dds"))
	{
		alm::gfx::TextureCache* textureCache = deviceManager->GetTextureCache();
		auto loadResult = textureCache->Load("_generated/CloudsDetail.dds", alm::gfx::TextureCache::Flags::None);
		if (!loadResult)
		{
			LOG_ERROR("Failed loading CloudsDetail.dds\n{}", loadResult.error());
		}
		else
		{
			loadResult->second.Wait();
			m_CloudsDetailTexture = std::move(loadResult->first->texture);
		}
	}
	else
	{
		auto createResult = CreateCloudsDetailTexture(deviceManager);
		assert(createResult);
		createResult->second.Wait();
		alm::rhi::TextureOwner& cloudsTexture = createResult->first;

		alm::gfx::SaveDDSTexture(cloudsTexture.get_weak(), alm::rhi::ResourceState::SHADER_RESOURCE, alm::rhi::ResourceState::SHADER_RESOURCE,
			deviceManager->GetDevice(), "_generated/CloudsDetail.dds");

		m_CloudsDetailTexture = std::move(cloudsTexture);
	}
}
