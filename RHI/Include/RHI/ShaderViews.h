#pragma once

#include "RHI/Descriptors.h"

namespace st::rhi
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

using TextureSampledView = TextureView<TextureViewType::Sampled>;
using TextureStorageView = TextureView<TextureViewType::Storage>;
using TextureColorTargetView = TextureView<TextureViewType::ColorTarget>;
using TextureDepthTargetView = TextureView<TextureViewType::DepthTarget>;

} // namespace st::rhi