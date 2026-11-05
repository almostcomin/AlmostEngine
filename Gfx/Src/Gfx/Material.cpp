#include "Gfx/GfxPCH.h"
#include "Gfx/Material.h"

alm::gfx::Material::Material(const std::string& name, const std::string& filename) :
	Name{ name.empty() ? "<null>" : name },
	SourceFileName{ filename.empty() ? "<null>" : filename }
{}

bool alm::gfx::Material::operator==(const Material& other) const
{
	return
		BaseColorTexture == other.BaseColorTexture &&
		MetalRoughTexture == other.MetalRoughTexture &&
		NormalTexture == other.NormalTexture &&
		EmissiveTexture == other.EmissiveTexture &&
		OcclusionTexture == other.OcclusionTexture &&
		TransmissionTexture == other.TransmissionTexture &&
		AlmostEqual(BaseColor, other.BaseColor) &&
		AlmostEqual(Opacity, other.Opacity) &&
		AlmostEqual(EmissiveColor, other.EmissiveColor) &&
		AlmostEqual(MetallicFactor, other.MetallicFactor) &&
		AlmostEqual(RoughnessFactor, other.RoughnessFactor) &&
		AlmostEqual(NormalTextureScale, other.NormalTextureScale) &&
		AlmostEqual(OcclusionStrengh, other.OcclusionStrengh) &&
		AlmostEqual(AlphaCutoff, other.AlphaCutoff) &&
		AlmostEqual(TransmissionFactor, other.TransmissionFactor) &&
		CullMode == other.CullMode &&
		Domain == other.Domain;
}