#pragma once

#include "RHI/Descriptors.h"

namespace alm::rhi
{

template<TextureViewType T>
class TextureView
{
public:
	TextureView() : m_Idx{ c_InvalidDescriptorIndex } {};
	TextureView(DescriptorIndex idx) : m_Idx{ idx } {};

	DescriptorIndex GetIdx() const { return m_Idx; }
	bool IsValid() const { return m_Idx != c_InvalidDescriptorIndex; }

	void Invalidate() { m_Idx = c_InvalidDescriptorIndex; }

private:
	DescriptorIndex m_Idx;
};

template<BufferViewType T>
class BufferView
{
public:
	BufferView() : m_Idx{ c_InvalidDescriptorIndex } {};
	BufferView(DescriptorIndex idx) : m_Idx{ idx } {};

	DescriptorIndex GetIdx() const { return m_Idx; }
	bool IsValid() const { return m_Idx != c_InvalidDescriptorIndex; }

	void Invalidate() { m_Idx = c_InvalidDescriptorIndex; }

private:
	DescriptorIndex m_Idx;
};

using TextureSampledView = TextureView<TextureViewType::Sampled>;
using TextureStorageView = TextureView<TextureViewType::Storage>;
using TextureColorTargetView = TextureView<TextureViewType::ColorTarget>;
using TextureDepthTargetView = TextureView<TextureViewType::DepthTarget>;

using BufferUniformView = BufferView<BufferViewType::Uniform>;
using BufferReadOnlyView = BufferView<BufferViewType::ReadOnly>;
using BufferReadWriteView = BufferView<BufferViewType::ReadWrite>;

} // namespace st::rhi