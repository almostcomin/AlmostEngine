#pragma once

#ifdef __cplusplus 

#include "RHI/ShaderViews.h"

namespace interop
{
	template<alm::rhi::TextureViewType T>
	class TextureViewIndex
	{
	public:
		TextureViewIndex() : m_Idx{ 0xFFFFFFFFu } {};
		TextureViewIndex(const alm::rhi::TextureView<T>& v) { m_Idx = v.GetIdx(); }
	private:
		uint32_t m_Idx;
	};

	template<alm::rhi::BufferViewType T>
	class BufferIndex
	{
	public:
		BufferIndex() : m_Idx{ 0xFFFFFFFFu } {};
		BufferIndex(const alm::rhi::BufferView<T>& v) { m_Idx = v.GetIdx(); }
	private:
		uint32_t m_Idx;
	};

	using TextureSampledViewIndex = TextureViewIndex<alm::rhi::TextureViewType::Sampled>;
	using TextureStorageViewIndex = TextureViewIndex<alm::rhi::TextureViewType::Storage>;
	using TextureColorTargetViewIndex = TextureViewIndex<alm::rhi::TextureViewType::ColorTarget>;
	using TextureDepthTargetViewIndex = TextureViewIndex<alm::rhi::TextureViewType::DepthTarget>;

	using BufferUniformIndex = BufferIndex<alm::rhi::BufferViewType::Uniform>;
	using BufferReadOnlyIndex = BufferIndex<alm::rhi::BufferViewType::ReadOnly>;
	using BufferReadWriteIndex = BufferIndex<alm::rhi::BufferViewType::ReadWrite>;

} // namespace interop

#endif