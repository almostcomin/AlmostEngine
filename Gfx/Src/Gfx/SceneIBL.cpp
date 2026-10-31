#include "Gfx/GfxPCH.h"
#include "Gfx/SceneIBL.h"
#include "Gfx/LoadedTexture.h"
#include "Gfx/TextureCache.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/IBLBaker.h"
#include "Gfx/IBLCache.h"
#include "Core/Log.h"

#include <chrono>
#include <thread>

alm::gfx::SceneIBL::SceneIBL(DeviceManager* deviceManager) :
	m_DeviceManager{ deviceManager }
{
}

std::expected<void, std::string> alm::gfx::SceneIBL::SetPanorama(const std::string& hdrFilePath)
{
	TextureCache* textureCache = m_DeviceManager ? m_DeviceManager->GetTextureCache() : nullptr;
	if (!textureCache)
	{
		std::string error = "IBL: no texture cache available.";
		LOG_WARNING("{}", error);
		return std::unexpected{ std::move(error) };
	}

	IBLBakeParams params;
	auto entry = IBLCache::TryLoad(hdrFilePath, params);
	if (!entry)
	{
		LOG_INFO("IBL: cache miss for '{}' ({}), baking...", hdrFilePath, entry.error());
		entry = IBLCache::Generate(hdrFilePath, params);
		if (!entry)
		{
			std::string error = std::format("IBL: bake of '{}' failed: {}", hdrFilePath, entry.error());
			LOG_WARNING("{}", error);
			return std::unexpected{ std::move(error) };
		}
	}
	else
	{
		LOG_INFO("IBL: cache hit for '{}'.", hdrFilePath);
	}

	std::string error;
	auto commit = [&](const std::string& ddsPath) -> std::shared_ptr<LoadedTexture>
	{
		auto loadResult = textureCache->Load(ddsPath, TextureLoadFlags::None);
		if (!loadResult)
		{
			error = std::format("IBL: load of '{}' failed: {}", ddsPath, loadResult.error());
			LOG_WARNING("{}", error);
			return {};
		}

		auto [handle, event] = *loadResult;
		event.Wait();

		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
		while (handle->state != LoadedTexture::Ready)
		{
			textureCache->Update();
			if (handle->state == LoadedTexture::Ready)
				break;

			if (std::chrono::steady_clock::now() > deadline)
			{
				error = std::format("IBL: texture '{}' did not become ready.", ddsPath);
				LOG_WARNING("{}", error);
				return {};
			}
			std::this_thread::sleep_for(std::chrono::microseconds(200));
		}

		return handle;
	};

	auto lambert = commit(entry->LambertDds);
	auto ggx = commit(entry->GgxDds);
	auto sky = commit(entry->SkyDds);
	auto dfg = commit(entry->DfgDds);

	if (!lambert || !ggx || !sky || !dfg)
		return std::unexpected{ std::move(error) };

	m_LambertIrradiance = std::move(lambert);
	m_GGXPrefiltered = std::move(ggx);
	m_Sky = std::move(sky);
	m_DfgLut = std::move(dfg);
	m_Sh = entry->Sh;
	m_Enabled = true;

	m_PanoramaPath = hdrFilePath;

	LOG_INFO("IBL environment '{}' ready.", hdrFilePath);
	return {};
}

bool alm::gfx::SceneIBL::IsValid() const
{
	return m_Enabled
		&& m_LambertIrradiance && m_LambertIrradiance->IsValid()
		&& m_GGXPrefiltered && m_GGXPrefiltered->IsValid();
}

alm::rhi::TextureSampledView alm::gfx::SceneIBL::GetLambertSampledView() const
{
	return m_LambertIrradiance && m_LambertIrradiance->IsValid() ?
		m_LambertIrradiance->texture->GetSampledView() : alm::rhi::TextureSampledView{};
}

alm::rhi::TextureSampledView alm::gfx::SceneIBL::GetGGXSampledView() const
{
	return m_GGXPrefiltered && m_GGXPrefiltered->IsValid() ?
		m_GGXPrefiltered->texture->GetSampledView() : alm::rhi::TextureSampledView{};
}

uint32_t alm::gfx::SceneIBL::GetGGXTextureMipCount() const
{
	if (m_GGXPrefiltered && m_GGXPrefiltered->IsValid())
		return m_GGXPrefiltered->texture->GetDesc().mipLevels;

	return 0;
}

alm::rhi::TextureSampledView alm::gfx::SceneIBL::GetSkySampledView() const
{
	return m_Sky && m_Sky->IsValid() ?
		m_Sky->texture->GetSampledView() : alm::rhi::TextureSampledView{};
}

alm::rhi::TextureSampledView alm::gfx::SceneIBL::GetDfgSampledView() const
{
	return m_DfgLut && m_DfgLut->IsValid() ?
		m_DfgLut->texture->GetSampledView() : alm::rhi::TextureSampledView{};
}
