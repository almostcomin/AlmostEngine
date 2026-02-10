#pragma once

#ifdef __cplusplus 

#include "RHI/ShaderViews.h"

namespace interop
{

	template<st::rhi::TextureViewType T>
	class TextureViewIndex
	{
	public:
		TextureViewIndex() : m_Idx{ 0xFFFFFFFFu } {};
		TextureViewIndex(const st::rhi::TextureView<T>& v) { m_Idx = v.GetIdx(); }
	private:
		uint32_t m_Idx;
	};

	using TextureSampledViewIndex = TextureViewIndex<st::rhi::TextureViewType::Sampled>;
	using TextureStorageViewIndex = TextureViewIndex<st::rhi::TextureViewType::Storage>;
	using TextureColorTargetViewIndex = TextureViewIndex<st::rhi::TextureViewType::ColorTarget>;
	using TextureDepthTargetViewIndex = TextureViewIndex<st::rhi::TextureViewType::DepthTarget>;

} // namespace interop

#endif