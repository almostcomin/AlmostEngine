#include "Gfx/GfxPCH.h"
#include "Gfx/IBLCache.h"
#include "Gfx/IBLBaker.h"
#include "Gfx/TextureData.h"
#include "Gfx/TextureLoader.h"
#include "Core/File.h"
#include "Core/Log.h"

#include <DirectXPackedVector.h>
#include <DirectXTex.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_map>

namespace
{

	using namespace alm::gfx;

	constexpr const char* kCacheRoot = "_generated/IBL";
	constexpr uint32_t kCacheVersion = 1;

	uint64_t Fnv1a64(const uint8_t* data, size_t size)
	{
		uint64_t hash = 14695981039346656037ull;
		for (size_t i = 0; i < size; ++i)
		{
			hash ^= data[i];
			hash *= 1099511628211ull;
		}
		return hash;
	}

	std::string HashPathToHex8(const std::string& path)
	{
		std::error_code ec;
		std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
		const std::string canonicalStr = ec ? path : canonical.string();

		const uint64_t hash = Fnv1a64(reinterpret_cast<const uint8_t*>(canonicalStr.data()), canonicalStr.size());
		return std::format("{:08x}", uint32_t(hash & 0xFFFFFFFFull));
	}

	std::string HashValueToHex16(uint64_t hash)
	{
		return std::format("{:016x}", hash);
	}

	std::string GetStem(const std::string& path)
	{
		return std::filesystem::path(path).stem().string();
	}

	struct PanoramaStats
	{
		bool exists = false;
		uint64_t fileSize = 0;
		int64_t mtimeTicks = 0;
		std::string canonicalPath;
	};

	PanoramaStats StatPanorama(const std::string& path)
	{
		PanoramaStats stats;
		std::error_code ec;
		std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
		stats.canonicalPath = ec ? path : canonical.string();

		stats.exists = std::filesystem::exists(path, ec);
		if (stats.exists)
		{
			stats.fileSize = uint64_t(std::filesystem::file_size(path, ec));
			stats.mtimeTicks = int64_t(std::filesystem::last_write_time(path, ec).time_since_epoch().count());
		}
		return stats;
	}

	using MetaMap = std::unordered_map<std::string, std::string>;

	std::expected<uint64_t, std::string> HashFileContent(const std::string& path)
	{
		alm::fs::File file{ path };
		if (!file.IsOpen())
			return std::unexpected(std::format("could not open '{}'.", path));

		auto readResult = file.Read();
		if (!readResult)
			return std::unexpected(std::format("could not read '{}'.", path));

		return Fnv1a64(readResult->data(), readResult->size());
	}

	bool ReadMetaFile(const std::string& path, MetaMap& outMeta)
	{
		std::ifstream in(path);
		if (!in)
			return false;

		std::string line;
		while (std::getline(in, line))
		{
			const size_t eq = line.find('=');
			if (eq == std::string::npos)
				continue;
			outMeta.emplace(line.substr(0, eq), line.substr(eq + 1));
		}
		return true;
	}

	bool WriteMetaFile(const std::string& path, const std::vector<std::pair<std::string, std::string>>& entries)
	{
		std::ofstream out(path);
		if (!out)
			return false;

		for (const auto& [key, value] : entries)
			out << key << '=' << value << '\n';
		return true;
	}

	bool MetaHasValue(const MetaMap& meta, const std::string& key, const std::string& expected)
	{
		auto it = meta.find(key);
		return it != meta.end() && it->second == expected;
	}

	std::optional<uint64_t> MetaGetHexU64(const MetaMap& meta, const std::string& key)
	{
		auto it = meta.find(key);
		if (it == meta.end() || it->second.size() < 2)
			return std::nullopt;
		return std::strtoull(it->second.c_str(), nullptr, 16);
	}

	struct EnvCachePaths
	{
		std::string dir;
		std::string stem;
		std::string skyDds;
		std::string lambertDds;
		std::string ggxDds;
		std::string metaPath;
	};

	EnvCachePaths GetEnvCachePaths(const std::string& hdrFilePath)
	{
		EnvCachePaths info;
		info.stem = GetStem(hdrFilePath);
		info.dir = std::format("{}/{}_{}", kCacheRoot, info.stem, HashPathToHex8(hdrFilePath));
		info.skyDds = std::format("{}/{}_Sky.dds", info.dir, info.stem);
		info.lambertDds = std::format("{}/{}_Lambert.dds", info.dir, info.stem);
		info.ggxDds = std::format("{}/{}_GGX.dds", info.dir, info.stem);
		info.metaPath = info.dir + "/cache.meta";
		return info;
	}

	std::string GetDfgFileName(const IBLBakeParams& params)
	{
		return std::format("DFG_{}_{}_{}.dds", params.DfgLutSize,
			params.DfgMultiscatter ? "multiscatter" : "splitsum",
			params.DfgCharlie ? "charlie" : "nocharlie");
	}

	std::string GetDfgDdsPath(const IBLBakeParams& params)
	{
		return std::format("{}/DFG/{}", kCacheRoot, GetDfgFileName(params));
	}

	uint16_t FloatToHalf(float v)
	{
		return DirectX::PackedVector::XMConvertFloatToHalf(std::min(v, 65504.0f));
	}

	void PackFaceToRgba16F(const std::vector<float3>& face, uint32_t dim, const DirectX::Image& img)
	{
		for (uint32_t y = 0; y < dim; ++y)
		{
			uint16_t* row = reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(img.pixels) + size_t(y) * img.rowPitch);
			const float3* src = face.data() + size_t(y) * dim;
			for (uint32_t x = 0; x < dim; ++x)
			{
				const float3& c = src[x];
				row[x * 4 + 0] = FloatToHalf(c.x);
				row[x * 4 + 1] = FloatToHalf(c.y);
				row[x * 4 + 2] = FloatToHalf(c.z);
				row[x * 4 + 3] = 0x3C00; // 1.0
			}
		}
	}

	void PackDfgToRgba16F(const IBLDFGLut& lut, const DirectX::Image& img)
	{
		for (uint32_t y = 0; y < lut.Size; ++y)
		{
			uint16_t* row = reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(img.pixels) + size_t(y) * img.rowPitch);
			const float3* src = lut.Texels.data() + size_t(y) * lut.Size;
			for (uint32_t x = 0; x < lut.Size; ++x)
			{
				const float3& c = src[x];
				row[x * 4 + 0] = FloatToHalf(c.x);
				row[x * 4 + 1] = FloatToHalf(c.y);
				row[x * 4 + 2] = FloatToHalf(c.z);
				row[x * 4 + 3] = 0x3C00; // 1.0
			}
		}
	}

	bool WriteCubemapDDS(const std::vector<IBLCubemapLevel>& chain, const std::string& path)
	{
		const uint32_t dim = chain.front().FaceSize;

		DirectX::ScratchImage scratch;
		HRESULT hr = scratch.InitializeCube(DXGI_FORMAT_R16G16B16A16_FLOAT, dim, dim, 1, chain.size());
		if (FAILED(hr))
			return false;

		for (uint32_t mip = 0; mip < chain.size(); ++mip)
		{
			const IBLCubemapLevel& level = chain[mip];
			for (uint32_t face = 0; face < 6; ++face)
			{
				const DirectX::Image* img = scratch.GetImage(mip, face, 0);
				if (!img)
					return false;
				PackFaceToRgba16F(level.Faces[face], level.FaceSize, *img);
			}
		}

		return SaveScratchImageToDDS(scratch, path);
	}

	bool WriteDfgDds(const IBLDFGLut& lut, const std::string& path)
	{
		DirectX::ScratchImage scratch;
		HRESULT hr = scratch.Initialize2D(DXGI_FORMAT_R16G16B16A16_FLOAT, lut.Size, lut.Size, 1, 1);
		if (FAILED(hr))
			return false;

		const DirectX::Image* img = scratch.GetImage(0, 0, 0);
		if (!img)
			return false;
		PackDfgToRgba16F(lut, *img);

		return SaveScratchImageToDDS(scratch, path);
	}

	std::string ShToString(const std::array<float3, 9>& sh)
	{
		std::string result;
		for (size_t i = 0; i < 9; ++i)
		{
			for (size_t k = 0; k < 3; ++k)
			{
				if (!result.empty())
					result += ',';
				result += std::format("{:.9g}", sh[i][k]);
			}
		}
		return result;
	}

	bool ShFromString(const std::string& str, std::array<float3, 9>& outSh)
	{
		std::istringstream stream(str);
		for (size_t i = 0; i < 9; ++i)
		{
			for (size_t k = 0; k < 3; ++k)
			{
				std::string token;
				if (!std::getline(stream, token, ','))
					return false;
				char* end = nullptr;
				outSh[i][k] = std::strtof(token.c_str(), &end);
				if (end == token.c_str() || *end != '\0')
					return false;
			}
		}
		return true;
	}

	bool EnvMetaMatchesParams(const MetaMap& meta, const IBLBakeParams& params)
	{
		return MetaHasValue(meta, "version", std::to_string(kCacheVersion))
			&& MetaHasValue(meta, "skySize", std::to_string(params.SkySize))
			&& MetaHasValue(meta, "lambertSize", std::to_string(params.LambertSize))
			&& MetaHasValue(meta, "ggxSize", std::to_string(params.GgxSize))
			&& MetaHasValue(meta, "ggxMinLodSize", std::to_string(params.GgxMinLodSize))
			&& MetaHasValue(meta, "ggxSampleCount", std::to_string(params.GgxSampleCount))
			&& MetaHasValue(meta, "irradianceSampleCount", std::to_string(params.IrradianceSampleCount))
			&& MetaHasValue(meta, "mirrorX", std::to_string(uint32_t(params.MirrorX)))
			&& MetaHasValue(meta, "karisClamp", std::to_string(uint32_t(params.KarisClamp)))
			&& MetaHasValue(meta, "shBands", std::to_string(params.ShBands));
	}

	bool DfgMetaMatchesParams(const MetaMap& meta, const IBLBakeParams& params)
	{
		return MetaHasValue(meta, "version", std::to_string(kCacheVersion))
			&& MetaHasValue(meta, "dfgLutSize", std::to_string(params.DfgLutSize))
			&& MetaHasValue(meta, "dfgMultiscatter", std::to_string(uint32_t(params.DfgMultiscatter)))
			&& MetaHasValue(meta, "dfgCharlie", std::to_string(uint32_t(params.DfgCharlie)));
	}

	// Validates the shared DFG cache (panorama independent). Returns the DDS path.
	std::expected<std::string, std::string> TryLoadDfg(const IBLBakeParams& params)
	{
		const std::string dfgDds = GetDfgDdsPath(params);
		const std::string dfgMetaPath = dfgDds + ".meta";

		MetaMap meta;
		if (!ReadMetaFile(dfgMetaPath, meta))
			return std::unexpected("DFG meta not found.");
		if (!DfgMetaMatchesParams(meta, params))
			return std::unexpected("DFG meta params mismatch.");
		if (!std::filesystem::exists(dfgDds))
			return std::unexpected("DFG dds not found.");

		return dfgDds;
	}

} // anonymous namespace

std::expected<IBLCacheEntry, std::string> alm::gfx::IBLCache::TryLoad(const std::string& hdrFilePath, const IBLBakeParams& params)
{
	const PanoramaStats stats = StatPanorama(hdrFilePath);
	if (!stats.exists)
		return std::unexpected("panorama file does not exist.");

	const EnvCachePaths env = GetEnvCachePaths(hdrFilePath);

	MetaMap meta;
	if (!ReadMetaFile(env.metaPath, meta))
		return std::unexpected("cache meta not found.");

	if (!EnvMetaMatchesParams(meta, params))
		return std::unexpected("cache meta params mismatch.");
	if (!MetaHasValue(meta, "panorama", stats.canonicalPath))
		return std::unexpected("cache meta panorama path mismatch.");

	// fast path: file untouched since the cache was written
	if (meta.find("fileSize") == meta.end() || meta.find("mtimeTicks") == meta.end()
		|| !MetaHasValue(meta, "fileSize", std::to_string(stats.fileSize))
		|| !MetaHasValue(meta, "mtimeTicks", std::to_string(stats.mtimeTicks)))
	{
		// slow path: mtimes change on git checkouts without content changes, so compare
		// the content hash before regenerating
		auto expectedHash = MetaGetHexU64(meta, "contentHash");
		if (!expectedHash)
			return std::unexpected("cache meta has no content hash.");

		auto hashResult = HashFileContent(hdrFilePath);
		if (!hashResult)
			return std::unexpected(hashResult.error());
		if (*hashResult != *expectedHash)
			return std::unexpected("panorama content changed.");
	}

	auto dfgResult = TryLoadDfg(params);
	if (!dfgResult)
		return std::unexpected(dfgResult.error());

	if (!std::filesystem::exists(env.skyDds)
		|| !std::filesystem::exists(env.lambertDds)
		|| !std::filesystem::exists(env.ggxDds))
		return std::unexpected("cache dds files missing.");

	IBLCacheEntry entry;
	entry.SkyDds = env.skyDds;
	entry.LambertDds = env.lambertDds;
	entry.GgxDds = env.ggxDds;
	entry.DfgDds = *dfgResult;

	auto shIt = meta.find("sh");
	if (shIt == meta.end() || !ShFromString(shIt->second, entry.Sh))
		return std::unexpected("cache meta SH data invalid.");

	return entry;
}

std::expected<IBLCacheEntry, std::string> alm::gfx::IBLCache::Generate(const std::string& hdrFilePath, const IBLBakeParams& params)
{
	alm::fs::File file{ hdrFilePath };
	if (!file.IsOpen())
		return std::unexpected(std::format("could not open hdr file '{}'.", hdrFilePath));

	auto fileData = file.Read();
	if (!fileData)
		return std::unexpected(std::format("could not read hdr file '{}'.", hdrFilePath));

	auto hdrResult = LoadImageTexture(alm::WeakBlob{ *fileData }, false);
	if (!hdrResult || hdrResult->format != alm::rhi::Format::RGBA32_FLOAT
		|| hdrResult->Dimension != alm::rhi::TextureDimension::Texture2D)
	{
		return std::unexpected(std::format("could not decode '{}' as an RGB hdr image.", hdrFilePath));
	}

	auto bakeResult = IBLBaker::Bake(reinterpret_cast<const float*>(hdrResult->Data.data()), hdrResult->Width, hdrResult->Height, params);
	if (!bakeResult)
		return std::unexpected(std::move(bakeResult.error()));
	IBLBakeOutput& baked = *bakeResult;

	const PanoramaStats stats = StatPanorama(hdrFilePath);
	const EnvCachePaths env = GetEnvCachePaths(hdrFilePath);
	const std::string dfgDds = GetDfgDdsPath(params);

	std::error_code ec;
	std::filesystem::create_directories(env.dir, ec);
	std::filesystem::create_directories(std::filesystem::path(dfgDds).parent_path(), ec);

	if (!WriteCubemapDDS(baked.Sky, env.skyDds))
		return std::unexpected(std::format("failed writing '{}'.", env.skyDds));
	if (!WriteCubemapDDS(baked.Lambert, env.lambertDds))
		return std::unexpected(std::format("failed writing '{}'.", env.lambertDds));
	if (!WriteCubemapDDS(baked.Ggx, env.ggxDds))
		return std::unexpected(std::format("failed writing '{}'.", env.ggxDds));
	if (!WriteDfgDds(baked.Dfg, dfgDds))
		return std::unexpected(std::format("failed writing '{}'.", dfgDds));

	const uint64_t contentHash = Fnv1a64(fileData->data(), fileData->size());
	if (!WriteMetaFile(env.metaPath, {
		{ "version", std::to_string(kCacheVersion) },
		{ "panorama", stats.canonicalPath },
		{ "fileSize", std::to_string(stats.fileSize) },
		{ "mtimeTicks", std::to_string(stats.mtimeTicks) },
		{ "contentHash", HashValueToHex16(contentHash) },
		{ "skySize", std::to_string(params.SkySize) },
		{ "lambertSize", std::to_string(params.LambertSize) },
		{ "ggxSize", std::to_string(params.GgxSize) },
		{ "ggxMinLodSize", std::to_string(params.GgxMinLodSize) },
		{ "ggxSampleCount", std::to_string(params.GgxSampleCount) },
		{ "irradianceSampleCount", std::to_string(params.IrradianceSampleCount) },
		{ "mirrorX", std::to_string(uint32_t(params.MirrorX)) },
		{ "karisClamp", std::to_string(uint32_t(params.KarisClamp)) },
		{ "shBands", std::to_string(params.ShBands) },
		{ "sh", ShToString(baked.Sh) },
		}))
	{
		return std::unexpected(std::format("failed writing '{}'.", env.metaPath));
	}

	if (!WriteMetaFile(dfgDds + ".meta", {
		{ "version", std::to_string(kCacheVersion) },
		{ "dfgLutSize", std::to_string(params.DfgLutSize) },
		{ "dfgMultiscatter", std::to_string(uint32_t(params.DfgMultiscatter)) },
		{ "dfgCharlie", std::to_string(uint32_t(params.DfgCharlie)) },
		}))
	{
		return std::unexpected(std::format("failed writing '{}'.", dfgDds + ".meta"));
	}

	IBLCacheEntry entry;
	entry.SkyDds = env.skyDds;
	entry.LambertDds = env.lambertDds;
	entry.GgxDds = env.ggxDds;
	entry.DfgDds = dfgDds;
	entry.Sh = baked.Sh;
	return entry;
}
