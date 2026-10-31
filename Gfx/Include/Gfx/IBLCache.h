#pragma once

#include "Core/Math.h"
#include "Gfx/IBLBaker.h"
#include <array>
#include <expected>
#include <string>

namespace alm::gfx
{

// Disk cache for baked IBL environments, following the ShaderFactory pattern.
//
// Layout (cwd relative):
//   _generated/IBL/<Stem>_<pathHash8>/   one dir per panorama path
//       <Stem>_Sky.dds                   cubemap RGBA16F, full mip chain
//       <Stem>_Lambert.dds               cubemap RGBA16F, 1 mip
//       <Stem>_GGX.dds                   cubemap RGBA16F, baked mip chain
//       cache.meta                       text (key=value): version, panorama path,
//                                        size, mtime, content hash, bake params, SH
//   _generated/IBL/DFG/                  panorama independent, shared by all environments
//       DFG_<size>_<splitsum|multiscatter>_<charlie|nocharlie>.dds + .meta
//
// Validation: cache version + bake params + panorama path + file size/mtime. If size or
// mtime changed (eg. git checkout refreshes mtimes) the panorama content is hashed before
// declaring the cache stale.

struct IBLCacheEntry
{
	std::string SkyDds;
	std::string LambertDds;
	std::string GgxDds;
	std::string DfgDds;
	std::array<float3, 9> Sh{};
};

class IBLCache
{
public:

	// Returns the cached textures for this panorama if the cache is valid.
	static std::expected<IBLCacheEntry, std::string> TryLoad(const std::string& hdrFilePath, const IBLBakeParams& params);

	// Bakes the panorama on the CPU and writes the cache files (DDS + meta).
	static std::expected<IBLCacheEntry, std::string> Generate(const std::string& hdrFilePath, const IBLBakeParams& params);
};

} // namespace alm::gfx
