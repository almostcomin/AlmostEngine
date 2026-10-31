#include "Gfx/GfxPCH.h"
#include "Gfx/IBLBaker.h"

#include <bit>
#include <cmath>
#include <cstring>
#include <future>
#include <utility>

namespace alm::gfx
{

namespace
{

constexpr float kPi = glm::pi<float>();
constexpr float kOneOverPi = 1.0f / kPi;
constexpr float kTwoOverSqrtPi = 1.1283791670955126f;
constexpr float kSqrtPi = 1.7724538509055159f;
constexpr float kSqrt2 = 1.4142135623730951f;
constexpr float kSqrt3 = 1.7320508075688772f;
constexpr float kSqrt5 = 2.2360679774997897f;
constexpr float kSqrt15 = 3.8729833462074170f;
constexpr float kOneOverSqrt2 = 0.7071067811865475f;

constexpr float HalfMax = 65504.0f;

struct EmptyState
{
};

class Image
{
public:
	Image() = default;
	Image(size_t w, size_t h, size_t stride = 0)
		: m_Bpr((stride ? stride : w) * sizeof(float3))
		, m_Width(w)
		, m_Height(h)
		, m_OwnedData(new uint8_t[m_Bpr * h])
		, m_Data(m_OwnedData.get())
	{
	}

	void Reset()
	{
		m_OwnedData.reset();
		m_Bpr = 0;
		m_Width = 0;
		m_Height = 0;
		m_Data = nullptr;
	}

	void Set(const Image& image)
	{
		m_OwnedData = nullptr;
		m_Bpr = image.m_Bpr;
		m_Width = image.m_Width;
		m_Height = image.m_Height;
		m_Data = image.m_Data;
	}

	void Subset(const Image& image, size_t x, size_t y, size_t w, size_t h)
	{
		m_OwnedData = nullptr;
		m_Bpr = image.m_Bpr;
		m_Width = w;
		m_Height = h;
		m_Data = static_cast<uint8_t*>(image.m_Data) + x * sizeof(float3) + y * m_Bpr;
	}

	bool IsValid() const { return m_Data != nullptr; }
	size_t GetWidth() const { return m_Width; }
	size_t GetStride() const { return m_Bpr / sizeof(float3); }
	size_t GetHeight() const { return m_Height; }
	size_t GetBytesPerRow() const { return m_Bpr; }
	size_t GetBytesPerPixel() const { return sizeof(float3); }
	void* GetData() const { return m_Data; }
	size_t GetSize() const { return m_Bpr * m_Height; }
	void* GetPixelRef(size_t x, size_t y) const { return static_cast<uint8_t*>(m_Data) + y * m_Bpr + x * sizeof(float3); }

private:
	size_t m_Bpr = 0;
	size_t m_Width = 0;
	size_t m_Height = 0;
	std::unique_ptr<uint8_t[]> m_OwnedData;
	void* m_Data = nullptr;
};

class Cubemap
{
public:
	enum class Face : uint8_t
	{
		PX = 0,
		NX,
		PY,
		NY,
		PZ,
		NZ
	};

	struct Address
	{
		Face Face;
		float S = 0;
		float T = 0;
	};

	using Texel = float3;

	explicit Cubemap(size_t dim) { ResetDimensions(dim); }

	size_t GetDimensions() const { return m_Dimensions; }

	void ResetDimensions(size_t dim)
	{
		m_Dimensions = dim;
		m_Scale = 2.0f / dim;
		m_UpperBound = std::nextafter(float(dim), 0.0f);
		for (Image& face : m_Faces)
			face.Reset();
	}

	void SetImageForFace(Face face, const Image& image) { m_Faces[size_t(face)].Set(image); }
	const Image& GetImageForFace(Face face) const { return m_Faces[size_t(face)]; }
	Image& GetImageForFace(Face face) { return m_Faces[size_t(face)]; }

	static float2 Center(size_t x, size_t y) { return { x + 0.5f, y + 0.5f }; }

	static void WriteAt(void* data, const Texel& texel) { *static_cast<Texel*>(data) = texel; }
	static const Texel& SampleAt(const void* data) { return *static_cast<const Texel*>(data); }

	float3 GetDirectionFor(Face face, size_t x, size_t y) const { return GetDirectionFor(face, x + 0.5f, y + 0.5f); }

	float3 GetDirectionFor(Face face, float x, float y) const
	{
		const float cx = (x * m_Scale) - 1.0f;
		const float cy = 1.0f - (y * m_Scale);

		float3 dir;
		const float l = std::sqrt(cx * cx + cy * cy + 1.0f);
		switch (face)
		{
		case Face::PX: dir = { 1.0f, cy, -cx }; break;
		case Face::NX: dir = { -1.0f, cy, cx }; break;
		case Face::PY: dir = { cx, 1.0f, -cy }; break;
		case Face::NY: dir = { cx, -1.0f, cy }; break;
		case Face::PZ: dir = { cx, cy, 1.0f }; break;
		case Face::NZ: dir = { -cx, cy, -1.0f }; break;
		}
		return dir * (1.0f / l);
	}

	Texel const& SampleAt(const float3& direction) const
	{
		const Address addr = GetAddressFor(direction);
		const size_t x = std::min(size_t(addr.S * m_Dimensions), m_Dimensions - 1);
		const size_t y = std::min(size_t(addr.T * m_Dimensions), m_Dimensions - 1);
		return SampleAt(GetImageForFace(addr.Face).GetPixelRef(x, y));
	}

	Texel FilterAt(const float3& direction) const
	{
		Address addr = GetAddressFor(direction);
		addr.S = std::min(addr.S * m_Dimensions, m_UpperBound);
		addr.T = std::min(addr.T * m_Dimensions, m_UpperBound);
		return FilterAt(GetImageForFace(addr.Face), addr.S, addr.T);
	}

	static Texel FilterAt(const Image& image, float x, float y)
	{
		const size_t x0 = size_t(x);
		const size_t y0 = size_t(y);
		const size_t x1 = x0 + 1;
		const size_t y1 = y0 + 1;

		const float u = float(x - x0);
		const float v = float(y - y0);
		const float oneMinusU = 1.0f - u;
		const float oneMinusV = 1.0f - v;
		const Texel& c0 = SampleAt(image.GetPixelRef(x0, y0));
		const Texel& c1 = SampleAt(image.GetPixelRef(x1, y0));
		const Texel& c2 = SampleAt(image.GetPixelRef(x0, y1));
		const Texel& c3 = SampleAt(image.GetPixelRef(x1, y1));
		return (oneMinusU * oneMinusV) * c0 + (u * oneMinusV) * c1 + (oneMinusU * v) * c2 + (u * v) * c3;
	}

	static Texel FilterAtCenter(const Image& image, size_t x0, size_t y0)
	{
		const size_t x1 = x0 + 1;
		const size_t y1 = y0 + 1;
		const Texel& c0 = SampleAt(image.GetPixelRef(x0, y0));
		const Texel& c1 = SampleAt(image.GetPixelRef(x1, y0));
		const Texel& c2 = SampleAt(image.GetPixelRef(x0, y1));
		const Texel& c3 = SampleAt(image.GetPixelRef(x1, y1));
		return (c0 + c1 + c2 + c3) * 0.25f;
	}

	static Texel TrilinearFilterAt(const Cubemap& l0, const Cubemap& l1, float lerp, const float3& L)
	{
		const Address addr = GetAddressFor(L);
		const Image& i0 = l0.GetImageForFace(addr.Face);
		const Image& i1 = l1.GetImageForFace(addr.Face);
		const float x0 = std::min(addr.S * l0.m_Dimensions, l0.m_UpperBound);
		const float y0 = std::min(addr.T * l0.m_Dimensions, l0.m_UpperBound);
		const float x1 = std::min(addr.S * l1.m_Dimensions, l1.m_UpperBound);
		const float y1 = std::min(addr.T * l1.m_Dimensions, l1.m_UpperBound);
		float3 c0 = FilterAt(i0, x0, y0);
		c0 += lerp * (FilterAt(i1, x1, y1) - c0);
		return c0;
	}

	static Address GetAddressFor(const float3& r)
	{
		Address addr;
		float sc, tc, ma;
		const float rx = std::abs(r.x);
		const float ry = std::abs(r.y);
		const float rz = std::abs(r.z);
		if (rx >= ry && rx >= rz)
		{
			ma = 1.0f / rx;
			if (r.x >= 0)
			{
				addr.Face = Face::PX;
				sc = -r.z;
				tc = -r.y;
			}
			else
			{
				addr.Face = Face::NX;
				sc = r.z;
				tc = -r.y;
			}
		}
		else if (ry >= rx && ry >= rz)
		{
			ma = 1.0f / ry;
			if (r.y >= 0)
			{
				addr.Face = Face::PY;
				sc = r.x;
				tc = r.z;
			}
			else
			{
				addr.Face = Face::NY;
				sc = r.x;
				tc = -r.z;
			}
		}
		else
		{
			ma = 1.0f / rz;
			if (r.z >= 0)
			{
				addr.Face = Face::PZ;
				sc = r.x;
				tc = -r.y;
			}
			else
			{
				addr.Face = Face::NZ;
				sc = -r.x;
				tc = -r.y;
			}
		}
		addr.S = (sc * ma + 1.0f) * 0.5f;
		addr.T = (tc * ma + 1.0f) * 0.5f;
		return addr;
	}

	void MakeSeamless()
	{
		const size_t dim = GetDimensions();
		const size_t D = dim;
		const size_t bpr = GetImageForFace(Face::NX).GetBytesPerRow();
		const size_t bpp = GetImageForFace(Face::NX).GetBytesPerPixel();

		auto getTexel = [](Image& image, ptrdiff_t x, ptrdiff_t y) -> Texel*
		{
			return (Texel*)((uint8_t*)image.GetData() + x * image.GetBytesPerPixel() + y * image.GetBytesPerRow());
		};

		auto stitch = [&](Face faceDst, ptrdiff_t xdst, ptrdiff_t ydst, ptrdiff_t incDst,
			Face faceSrc, size_t xsrc, size_t ysrc, ptrdiff_t incSrc)
		{
			Image& imageDst = GetImageForFace(faceDst);
			Image& imageSrc = GetImageForFace(faceSrc);
			Texel* dst = getTexel(imageDst, xdst, ydst);
			Texel* src = getTexel(imageSrc, ptrdiff_t(xsrc), ptrdiff_t(ysrc));
			for (size_t i = 0; i < dim; ++i)
			{
				*dst = *src;
				dst = (Texel*)((uint8_t*)dst + incDst);
				src = (Texel*)((uint8_t*)src + incSrc);
			}
		};

		auto corners = [&](Face face)
		{
			const size_t L = D - 1;
			Image& image = GetImageForFace(face);
			*getTexel(image, -1, -1) = (*getTexel(image, 0, 0) + *getTexel(image, -1, 0) + *getTexel(image, 0, -1)) / 3.0f;
			*getTexel(image, ptrdiff_t(L + 1), -1) = (*getTexel(image, L, 0) + *getTexel(image, L, -1) + *getTexel(image, ptrdiff_t(L + 1), 0)) / 3.0f;
			*getTexel(image, -1, ptrdiff_t(L + 1)) = (*getTexel(image, 0, L) + *getTexel(image, -1, L) + *getTexel(image, 0, ptrdiff_t(L + 1))) / 3.0f;
			*getTexel(image, ptrdiff_t(L + 1), ptrdiff_t(L + 1)) = (*getTexel(image, L, L) + *getTexel(image, ptrdiff_t(L + 1), L) + *getTexel(image, L, ptrdiff_t(L + 1))) / 3.0f;
		};

		stitch(Face::PY, -1, 0, ptrdiff_t(bpr), Face::NX, 0, 0, ptrdiff_t(bpp));
		stitch(Face::PY, 0, -1, ptrdiff_t(bpp), Face::NZ, D - 1, 0, ptrdiff_t(-bpp));
		stitch(Face::PY, ptrdiff_t(D), 0, ptrdiff_t(bpr), Face::PX, D - 1, 0, ptrdiff_t(-bpp));
		stitch(Face::PY, 0, ptrdiff_t(D), ptrdiff_t(bpp), Face::PZ, 0, 0, ptrdiff_t(bpp));
		corners(Face::PY);

		stitch(Face::NX, -1, 0, ptrdiff_t(bpr), Face::NZ, D - 1, 0, ptrdiff_t(bpr));
		stitch(Face::NX, 0, -1, ptrdiff_t(bpp), Face::PY, 0, 0, ptrdiff_t(bpr));
		stitch(Face::NX, ptrdiff_t(D), 0, ptrdiff_t(bpr), Face::PZ, 0, 0, ptrdiff_t(bpr));
		stitch(Face::NX, 0, ptrdiff_t(D), ptrdiff_t(bpp), Face::NY, 0, D - 1, ptrdiff_t(-bpr));
		corners(Face::NX);

		stitch(Face::PZ, -1, 0, ptrdiff_t(bpr), Face::NX, D - 1, 0, ptrdiff_t(bpr));
		stitch(Face::PZ, 0, -1, ptrdiff_t(bpp), Face::PY, 0, D - 1, ptrdiff_t(bpp));
		stitch(Face::PZ, ptrdiff_t(D), 0, ptrdiff_t(bpr), Face::PX, 0, 0, ptrdiff_t(bpr));
		stitch(Face::PZ, 0, ptrdiff_t(D), ptrdiff_t(bpp), Face::NY, 0, 0, ptrdiff_t(bpp));
		corners(Face::PZ);

		stitch(Face::PX, -1, 0, ptrdiff_t(bpr), Face::PZ, D - 1, 0, ptrdiff_t(bpr));
		stitch(Face::PX, 0, -1, ptrdiff_t(bpp), Face::PY, D - 1, D - 1, ptrdiff_t(-bpr));
		stitch(Face::PX, ptrdiff_t(D), 0, ptrdiff_t(bpr), Face::NZ, 0, 0, ptrdiff_t(bpr));
		stitch(Face::PX, 0, ptrdiff_t(D), ptrdiff_t(bpp), Face::NY, D - 1, 0, ptrdiff_t(bpr));
		corners(Face::PX);

		stitch(Face::NZ, -1, 0, ptrdiff_t(bpr), Face::PX, D - 1, 0, ptrdiff_t(bpr));
		stitch(Face::NZ, 0, -1, ptrdiff_t(bpp), Face::PY, D - 1, 0, ptrdiff_t(-bpp));
		stitch(Face::NZ, ptrdiff_t(D), 0, ptrdiff_t(bpr), Face::NX, 0, 0, ptrdiff_t(bpr));
		stitch(Face::NZ, 0, ptrdiff_t(D), ptrdiff_t(bpp), Face::NY, D - 1, D - 1, ptrdiff_t(-bpp));
		corners(Face::NZ);

		stitch(Face::NY, -1, 0, ptrdiff_t(bpr), Face::NX, D - 1, D - 1, ptrdiff_t(-bpp));
		stitch(Face::NY, 0, -1, ptrdiff_t(bpp), Face::PZ, 0, D - 1, ptrdiff_t(bpp));
		stitch(Face::NY, ptrdiff_t(D), 0, ptrdiff_t(bpr), Face::PX, 0, D - 1, ptrdiff_t(bpp));
		stitch(Face::NY, 0, ptrdiff_t(D), ptrdiff_t(bpp), Face::NZ, D - 1, D - 1, ptrdiff_t(-bpp));
		corners(Face::NY);
	}

private:
	size_t m_Dimensions = 0;
	float m_Scale = 1;
	float m_UpperBound = 0;
	std::array<Image, 6> m_Faces;
};

using BakedLevel = std::pair<std::unique_ptr<Image>, std::unique_ptr<Cubemap>>;

BakedLevel CreateEmptyLevel(size_t dim)
{
	size_t width = 4 * (dim + 2);
	size_t height = 3 * (dim + 2);
	auto storage = std::make_unique<Image>(width, height);
	memset(storage->GetData(), 0, storage->GetBytesPerRow() * height);
	auto cubemap = std::make_unique<Cubemap>(dim);

	const size_t padded = dim + 2;
	auto setFaceFromCross = [&](Cubemap::Face face)
	{
		size_t x = 0;
		size_t y = 0;
		switch (face)
		{
		case Cubemap::Face::NX: x = 0, y = padded; break;
		case Cubemap::Face::PX: x = 2 * padded, y = padded; break;
		case Cubemap::Face::NY: x = padded, y = 2 * padded; break;
		case Cubemap::Face::PY: x = padded, y = 0; break;
		case Cubemap::Face::NZ: x = 3 * padded, y = padded; break;
		case Cubemap::Face::PZ: x = padded, y = padded; break;
		}
		Image subImage;
		subImage.Subset(*storage, x + 1, y + 1, dim, dim);
		cubemap->SetImageForFace(face, subImage);
	};

	for (size_t i = 0; i < 6; ++i)
		setFaceFromCross(Cubemap::Face(i));

	return { std::move(storage), std::move(cubemap) };
}

template<typename Fn>
void ParallelFor(size_t count, size_t minBatch, Fn fn)
{
	if (count == 0)
		return;
	if (count <= minBatch)
	{
		fn(0, count);
		return;
	}

	const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
	const size_t taskCount = std::min<size_t>(hw, count / minBatch);
	if (taskCount <= 1)
	{
		fn(0, count);
		return;
	}

	const size_t chunk = (count + taskCount - 1) / taskCount;
	std::vector<std::future<void>> futures;
	size_t start = 0;
	while (start + chunk < count)
	{
		const size_t end = std::min(count, start + chunk);
		futures.push_back(std::async(std::launch::async, [&fn, start, end] { fn(start, end - start); }));
		start = end;
	}
	if (start < count)
		fn(start, count - start);
	for (std::future<void>& future : futures)
		future.get();
}

template<typename STATE, typename Proc, typename Reduce>
void ProcessFaces(Cubemap& cm, const STATE& prototype, Proc proc, Reduce reduce)
{
	const size_t dim = cm.GetDimensions();
	std::array<STATE, 6> states;
	for (STATE& s : states)
		s = prototype;

	std::vector<std::future<void>> futures;
	for (size_t f = 0; f < 6; ++f)
	{
		futures.push_back(std::async(std::launch::async,
			[&states, &cm, &proc, dim, f]
			{
				Image& image = cm.GetImageForFace((Cubemap::Face)f);
				for (size_t y = 0; y < dim; ++y)
				{
					Cubemap::Texel* data = static_cast<Cubemap::Texel*>(image.GetPixelRef(0, y));
					proc(states[f], y, (Cubemap::Face)f, data, dim);
				}
			}));
	}
	for (std::future<void>& future : futures)
		future.get();
	for (STATE& s : states)
		reduce(s);
}

template<typename STATE, typename Proc>
void ProcessFaces(Cubemap& cm, const STATE& prototype, Proc proc)
{
	ProcessFaces(cm, prototype, std::move(proc), [](STATE&) {});
}

float2 Hammersley(uint32_t i, float iN)
{
	constexpr float tof = 0.5f / 0x80000000U;
	uint32_t bits = i;
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return { i * iN, bits * tof };
}

float Log4(float x) { return std::log2(x) * 0.5f; }

float SolidAngle(size_t dim, size_t u, size_t v)
{
	const float iDim = 1.0f / dim;
	float s = ((u + 0.5f) * 2 * iDim) - 1;
	float t = ((v + 0.5f) * 2 * iDim) - 1;
	const float x0 = s - iDim;
	const float y0 = t - iDim;
	const float x1 = s + iDim;
	const float y1 = t + iDim;

	auto sphereQuadrantArea = [](float x, float y)
	{
		return std::atan2(x * y, std::sqrt(x * x + y * y + 1));
	};

	return sphereQuadrantArea(x0, y0) - sphereQuadrantArea(x0, y1) - sphereQuadrantArea(x1, y0) + sphereQuadrantArea(x1, y1);
}

void ClampImage(Image& src)
{
	auto compress = [](float3 color, float linear, float compressed)
	{
		const float luma = dot(color, float3{ 0.2126f, 0.7152f, 0.0722f });
		return luma <= linear ? color : (color / luma) * ((linear * linear - compressed * luma) / (2 * linear - compressed - luma));
	};

	const size_t width = src.GetWidth();
	const size_t height = src.GetHeight();
	for (size_t y = 0; y < height; ++y)
	{
		for (size_t x = 0; x < width; ++x)
		{
			float3& c = *static_cast<float3*>(src.GetPixelRef(x, y));
			c = compress(c, 4096.0f, 16384.0f);
		}
	}
}

void EquirectangularToCubemap(Cubemap& dst, const Image& src)
{
	const size_t width = src.GetWidth();
	const size_t height = src.GetHeight();

	auto toRectilinear = [width, height](float3 s) -> float2
	{
		float xf = std::atan2(s.x, s.z) * kOneOverPi;
		float yf = std::asin(s.y) * (2 * kOneOverPi);
		xf = (xf + 1.0f) * 0.5f * (width - 1);
		yf = (1.0f - yf) * 0.5f * (height - 1);
		return { xf, yf };
	};

	ProcessFaces<EmptyState>(dst, EmptyState{},
		[&](EmptyState&, size_t y, Cubemap::Face f, Cubemap::Texel* data, size_t dim)
		{
			for (size_t x = 0; x < dim; ++x, ++data)
			{
				const float2 pos0 = toRectilinear(dst.GetDirectionFor(f, x + 0.0f, y + 0.0f));
				const float2 pos1 = toRectilinear(dst.GetDirectionFor(f, x + 1.0f, y + 0.0f));
				const float2 pos2 = toRectilinear(dst.GetDirectionFor(f, x + 0.0f, y + 1.0f));
				const float2 pos3 = toRectilinear(dst.GetDirectionFor(f, x + 1.0f, y + 1.0f));
				const float minx = std::min(pos0.x, std::min(pos1.x, std::min(pos2.x, pos3.x)));
				const float maxx = std::max(pos0.x, std::max(pos1.x, std::max(pos2.x, pos3.x)));
				const float miny = std::min(pos0.y, std::min(pos1.y, std::min(pos2.y, pos3.y)));
				const float maxy = std::max(pos0.y, std::max(pos1.y, std::max(pos2.y, pos3.y)));
				const float dx = std::max(1.0f, maxx - minx);
				const float dy = std::max(1.0f, maxy - miny);
				const size_t numSamples = size_t(dx * dy);

				const float iNumSamples = 1.0f / numSamples;
				float3 c(0.0f);
				for (size_t sample = 0; sample < numSamples; sample++)
				{
					const float2 h = Hammersley(uint32_t(sample), iNumSamples);
					const float3 s = dst.GetDirectionFor(f, x + h.x, y + h.y);
					const float2 pos = toRectilinear(s);
					c += Cubemap::SampleAt(src.GetPixelRef((uint32_t)pos.x, (uint32_t)pos.y));
				}
				c *= iNumSamples;

				Cubemap::WriteAt(data, c);
			}
		});

	dst.MakeSeamless();
}

void DownsampleCubemapLevelBoxFilter(Cubemap& dst, const Cubemap& src)
{
	const size_t scale = src.GetDimensions() / dst.GetDimensions();

	ProcessFaces<EmptyState>(dst, EmptyState{},
		[&](EmptyState&, size_t y, Cubemap::Face f, Cubemap::Texel* data, size_t dim)
		{
			const Image& image = src.GetImageForFace(f);
			for (size_t x = 0; x < dim; ++x, ++data)
				Cubemap::WriteAt(data, Cubemap::FilterAtCenter(image, x * scale, y * scale));
		});

	dst.MakeSeamless();
}

float3 HemisphereImportanceSampleDggx(float2 u, float a)
{
	const float phi = 2.0f * kPi * u.x;
	const float cosTheta2 = (1 - u.y) / (1 + (a + 1) * ((a - 1) * u.y));
	const float cosTheta = std::sqrt(cosTheta2);
	const float sinTheta = std::sqrt(1 - cosTheta2);
	return { sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta };
}

float3 HemisphereCosSample(float2 u)
{
	const float phi = 2.0f * kPi * u.x;
	const float cosTheta2 = 1 - u.y;
	const float cosTheta = std::sqrt(cosTheta2);
	const float sinTheta = std::sqrt(1 - cosTheta2);
	return { sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta };
}

float DistributionGGX(float NoH, float linearRoughness)
{
	const float a = linearRoughness;
	const float f = (a - 1) * ((a + 1) * (NoH * NoH)) + 1;
	return (a * a) / (kPi * f * f);
}

float Visibility(float NoV, float NoL, float a)
{
	const float a2 = a * a;
	const float GGXL = NoL * std::sqrt((NoL - NoL * a2) * NoL + a2);
	const float GGXV = NoL * std::sqrt((NoV - NoV * a2) * NoV + a2);
	return 0.5f / (GGXV + GGXL);
}

float VisibilityAshikhmin(float NoV, float NoL, float)
{
	return 1 / (4 * (NoL + NoV - NoL * NoV));
}

float DistributionCharlie(float NoH, float linearRoughness)
{
	const float a = linearRoughness;
	const float invAlpha = 1 / a;
	const float cos2h = NoH * NoH;
	const float sin2h = 1 - cos2h;
	return (2.0f + invAlpha) * std::pow(sin2h, invAlpha * 0.5f) / (2.0f * kPi);
}

float pow5(float x)
{
	const float x2 = x * x;
	return x2 * x2 * x;
}

std::vector<const Cubemap*> LevelPtrs(const std::vector<BakedLevel>& chain)
{
	std::vector<const Cubemap*> ptrs;
	ptrs.reserve(chain.size());
	for (const BakedLevel& level : chain)
		ptrs.push_back(level.second.get());
	return ptrs;
}

void RoughnessFilter(Cubemap& dst, const std::vector<const Cubemap*>& levels,
	float linearRoughness, size_t maxNumSamples, bool prefilter)
{
	const float numSamples = float(maxNumSamples);
	const float inumSamples = 1.0f / numSamples;
	const size_t maxLevel = levels.size() - 1;
	const float maxLevelf = float(maxLevel);
	const Cubemap& base = *levels[0];
	const size_t dim0 = base.GetDimensions();
	const float omegaP = (4.0f * kPi) / float(6 * dim0 * dim0);

	if (linearRoughness == 0)
	{
		ProcessFaces<EmptyState>(dst, EmptyState{},
			[&](EmptyState&, size_t y, Cubemap::Face f, Cubemap::Texel* data, size_t dim)
			{
				const Cubemap& cm = *levels[0];
				for (size_t x = 0; x < dim; ++x, ++data)
				{
					const float2 p = Cubemap::Center(x, y);
					const float3 N = dst.GetDirectionFor(f, p.x, p.y);
					Cubemap::WriteAt(data, cm.SampleAt(N));
				}
			});
		return;
	}

	struct CacheEntry
	{
		float3 L;
		float BrdfNoL;
		float Lerp;
		uint8_t L0;
		uint8_t L1;
	};

	std::vector<CacheEntry> cache;
	cache.reserve(maxNumSamples);

	float weight = 0;
	for (size_t sampleIndex = 0; sampleIndex < maxNumSamples; sampleIndex++)
	{
		const float2 u = Hammersley(uint32_t(sampleIndex), inumSamples);
		const float3 H = HemisphereImportanceSampleDggx(u, linearRoughness);

		const float NoH = H.z;
		const float NoH2 = H.z * H.z;
		const float NoL = 2 * NoH2 - 1;

		if (NoL > 0)
		{
			const float pdf = DistributionGGX(NoH, linearRoughness) / 4;
			constexpr float K = 4;
			const float omegaS = 1 / (numSamples * pdf);
			const float l = float(Log4(omegaS) - Log4(omegaP) + Log4(K));
			const float mipLevel = prefilter ? glm::clamp(l, 0.0f, maxLevelf) : 0.0f;

			const float brdfNoL = float(NoL);
			weight += brdfNoL;

			const uint8_t l0 = uint8_t(mipLevel);
			const uint8_t l1 = uint8_t(std::min(maxLevel, size_t(l0 + 1)));
			const float lerp = mipLevel - float(l0);

			cache.push_back({ { 2 * NoH * H.x, 2 * NoH * H.y, NoL }, brdfNoL, lerp, l0, l1 });
		}
	}

	for (CacheEntry& entry : cache)
		entry.BrdfNoL *= 1.0f / weight;

	std::sort(cache.begin(), cache.end(), [](const CacheEntry& lhs, const CacheEntry& rhs)
		{ return lhs.BrdfNoL < rhs.BrdfNoL; });

	struct State
	{
		std::default_random_engine Gen;
		std::uniform_real_distribution<float> Distribution{ -kPi, kPi };
	};

	auto scanline = [&](State& state, size_t y, Cubemap::Face f, Cubemap::Texel* data, size_t dim)
	{
		float3x3 R;
		const size_t numSamples = cache.size();
		for (size_t x = 0; x < dim; ++x, ++data)
		{
			const float2 p = Cubemap::Center(x, y);
			const float3 N = dst.GetDirectionFor(f, p.x, p.y);

			const float3 up = std::abs(N.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
			R[0] = glm::normalize(glm::cross(up, N));
			R[1] = glm::cross(N, R[0]);
			R[2] = N;

			const float angle = state.Distribution(state.Gen);
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			const float3x3 Rz(c, s, 0.0f, -s, c, 0.0f, 0.0f, 0.0f, 1.0f);
			R = R * Rz;

			float3 Li(0.0f);
			for (size_t sample = 0; sample < numSamples; sample++)
			{
				const CacheEntry& e = cache[sample];
				const float3 L = R * e.L;
				const Cubemap& cmBase = *levels[e.L0];
				const Cubemap& next = *levels[e.L1];
				const float3 c0 = Cubemap::TrilinearFilterAt(cmBase, next, e.Lerp, L);
				Li += c0 * e.BrdfNoL;
			}
			Cubemap::WriteAt(data, Li);
		}
	};

	ProcessFaces(dst, State{}, scanline, [](State&) {});
}

void DiffuseIrradiance(Cubemap& dst, const std::vector<const Cubemap*>& levels, size_t maxNumSamples)
{
	const float numSamples = float(maxNumSamples);
	const float inumSamples = 1.0f / numSamples;
	const size_t maxLevel = levels.size() - 1;
	const float maxLevelf = float(maxLevel);
	const Cubemap& base = *levels[0];
	const size_t dim0 = base.GetDimensions();
	const float omegaP = (4.0f * kPi) / float(6 * dim0 * dim0);

	struct CacheEntry
	{
		float3 L;
		float Lerp;
		uint8_t L0;
		uint8_t L1;
	};

	std::vector<CacheEntry> cache;
	cache.reserve(maxNumSamples);

	for (size_t sampleIndex = 0; sampleIndex < maxNumSamples; sampleIndex++)
	{
		const float2 u = Hammersley(uint32_t(sampleIndex), inumSamples);
		const float3 L = HemisphereCosSample(u);
		const float NoL = L.z;

		if (NoL > 0)
		{
			const float pdf = NoL * kOneOverPi;
			constexpr float K = 4;
			const float omegaS = 1.0f / (numSamples * pdf);
			const float l = float(Log4(omegaS) - Log4(omegaP) + Log4(K));
			const float mipLevel = glm::clamp(l, 0.0f, maxLevelf);

			const uint8_t l0 = uint8_t(mipLevel);
			const uint8_t l1 = uint8_t(std::min(maxLevel, size_t(l0 + 1)));
			const float lerp = mipLevel - float(l0);

			cache.push_back({ L, lerp, l0, l1 });
		}
	}

	ProcessFaces<EmptyState>(dst, EmptyState{},
		[&](EmptyState&, size_t y, Cubemap::Face f, Cubemap::Texel* data, size_t dim)
		{
			float3x3 R;
			const size_t numSamples = cache.size();
			for (size_t x = 0; x < dim; ++x, ++data)
			{
				const float2 p = Cubemap::Center(x, y);
				const float3 N = dst.GetDirectionFor(f, p.x, p.y);

				const float3 up = std::abs(N.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
				R[0] = glm::normalize(glm::cross(up, N));
				R[1] = glm::cross(N, R[0]);
				R[2] = N;

				float3 Li(0.0f);
				for (size_t sample = 0; sample < numSamples; sample++)
				{
					const CacheEntry& e = cache[sample];
					const float3 L = R * e.L;
					const Cubemap& cmBase = *levels[e.L0];
					const Cubemap& next = *levels[e.L1];
					const float3 c0 = Cubemap::TrilinearFilterAt(cmBase, next, e.Lerp, L);
					Li += c0;
				}
				Cubemap::WriteAt(data, Li * inumSamples);
			}
		});
}

float2 DFVSplitSum(float NoV, float linearRoughness, size_t numSamples)
{
	float2 r(0.0f);
	const float3 V(std::sqrt(1 - NoV * NoV), 0, NoV);
	for (size_t i = 0; i < numSamples; i++)
	{
		const float2 u = Hammersley(uint32_t(i), 1.0f / numSamples);
		const float3 H = HemisphereImportanceSampleDggx(u, linearRoughness);
		const float3 L = 2 * dot(V, H) * H - V;
		const float VoH = saturate(dot(V, H));
		const float NoL = saturate(L.z);
		const float NoH = saturate(H.z);
		if (NoL > 0)
		{
			const float v = Visibility(NoV, NoL, linearRoughness) * NoL * (VoH / NoH);
			const float Fc = pow5(1 - VoH);
			r.x += v * (1.0f - Fc);
			r.y += v * Fc;
		}
	}
	return r * (4.0f / numSamples);
}

float2 DFVMultiscatter(float NoV, float linearRoughness, size_t numSamples)
{
	float2 r(0.0f);
	const float3 V(std::sqrt(1 - NoV * NoV), 0, NoV);
	for (size_t i = 0; i < numSamples; i++)
	{
		const float2 u = Hammersley(uint32_t(i), 1.0f / numSamples);
		const float3 H = HemisphereImportanceSampleDggx(u, linearRoughness);
		const float3 L = 2 * dot(V, H) * H - V;
		const float VoH = saturate(dot(V, H));
		const float NoL = saturate(L.z);
		const float NoH = saturate(H.z);
		if (NoL > 0)
		{
			const float v = Visibility(NoV, NoL, linearRoughness) * NoL * (VoH / NoH);
			const float Fc = pow5(1 - VoH);
			r.x += v * Fc;
			r.y += v;
		}
	}
	return r * (4.0f / numSamples);
}

float DFVCharlieUniform(float NoV, float linearRoughness, size_t numSamples)
{
	if (linearRoughness <= 0.0f)
		return 0.0f;

	float r = 0.0f;
	const float3 V(std::sqrt(1 - NoV * NoV), 0, NoV);
	for (size_t i = 0; i < numSamples; i++)
	{
		const float2 u = Hammersley(uint32_t(i), 1.0f / numSamples);
		const float phi = 2.0f * kPi * u.x;
		const float cosTheta = 1 - u.y;
		const float sinTheta = std::sqrt(1 - cosTheta * cosTheta);
		const float3 H{ sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta };
		const float3 L = 2 * dot(V, H) * H - V;
		const float VoH = saturate(dot(V, H));
		const float NoL = saturate(L.z);
		const float NoH = saturate(H.z);
		if (NoL > 0)
		{
			const float v = VisibilityAshikhmin(NoV, NoL, linearRoughness);
			const float d = DistributionCharlie(NoH, linearRoughness);
			r += v * d * NoL * VoH;
		}
	}
	return r * (4.0f * 2.0f * kPi / numSamples);
}

void ComputeDFGLut(std::vector<float3>& texels, uint32_t size, bool multiscatter, bool cloth)
{
	texels.assign(size_t(size) * size, float3(0.0f));
	const auto dfv = multiscatter ? DFVMultiscatter : DFVSplitSum;
	const float h = float(size);

	ParallelFor(size, 8, [&](size_t y0, size_t count)
		{
			for (size_t y = y0; y < y0 + count; y++)
			{
				const float coord = saturate((h - float(y) + 0.5f) / h);
				const float linearRoughness = coord * coord;
				for (uint32_t x = 0; x < size; x++)
				{
					const float NoV = saturate((float(x) + 0.5f) / float(size));
					const float2 d = dfv(NoV, linearRoughness, 1024);
					float3 r{ d.x, d.y, 0.0f };
					if (cloth)
						r.z = DFVCharlieUniform(NoV, linearRoughness, 4096);
					texels[y * size + x] = r;
				}
			}
		});
}

constexpr float Factorial(size_t n, size_t d = 1)
{
	d = std::max(size_t(1), d);
	n = std::max(size_t(1), n);
	float r = 1.0;
	if (n == d)
	{
	}
	else if (n > d)
	{
		for (; n > d; n--)
			r *= n;
	}
	else
	{
		for (; d > n; d--)
			r *= d;
		r = 1.0f / r;
	}
	return r;
}

constexpr size_t SHindex(ptrdiff_t m, size_t l) { return l * (l + 1) + size_t(m); }

float Kml(ptrdiff_t m, size_t l)
{
	m = m < 0 ? -m : m;
	const size_t mu = size_t(m);
	const float K = (2 * l + 1) * Factorial(l - mu, l + mu);
	return std::sqrt(K) * (kTwoOverSqrtPi * 0.25f);
}

std::vector<float> Ki(size_t numBands)
{
	const size_t numCoefs = numBands * numBands;
	std::vector<float> K(numCoefs);
	for (size_t l = 0; l < numBands; l++)
	{
		K[SHindex(0, l)] = Kml(0, l);
		for (size_t m = 1; m <= l; m++)
		{
			K[SHindex(ptrdiff_t(m), l)] = kSqrt2 * Kml(ptrdiff_t(m), l);
			K[SHindex(-ptrdiff_t(m), l)] = kSqrt2 * Kml(ptrdiff_t(m), l);
		}
	}
	return K;
}

constexpr float ComputeTruncatedCosSH(size_t l)
{
	if (l == 0)
	{
		return kPi;
	}
	else if (l == 1)
	{
		return 2 * kPi / 3;
	}
	else if (l & 1u)
	{
		return 0;
	}
	const size_t l2 = l / 2;
	const float A0 = ((l2 & 1u) ? 1.0f : -1.0f) / ((l + 2) * (l - 1));
	const float A1 = Factorial(l, l2) / (Factorial(l2) * float(1 << l));
	return 2 * kPi * A0 * A1;
}

void ComputeShBasis(float* SHb, size_t numBands, const float3& s)
{
	float Pml2 = 0;
	float Pml1 = 1;
	SHb[0] = Pml1;
	for (size_t l = 1; l < numBands; l++)
	{
		const float Pml = ((2 * l - 1.0f) * Pml1 * s.z - (l - 1.0f) * Pml2) / l;
		Pml2 = Pml1;
		Pml1 = Pml;
		SHb[SHindex(0, l)] = Pml;
	}
	float Pmm = 1;
	for (size_t m = 1; m < numBands; m++)
	{
		Pmm = (1.0f - 2 * m) * Pmm;
		Pml2 = Pmm;
		Pml1 = (2 * m + 1.0f) * Pmm * s.z;
		SHb[SHindex(-ptrdiff_t(m), m)] = Pml2;
		SHb[SHindex(ptrdiff_t(m), m)] = Pml2;
		if (m + 1 < numBands)
		{
			SHb[SHindex(-ptrdiff_t(m), m + 1)] = Pml1;
			SHb[SHindex(ptrdiff_t(m), m + 1)] = Pml1;
			for (size_t l = m + 2; l < numBands; l++)
			{
				const float Pml = ((2 * l - 1.0f) * Pml1 * s.z - (l + m - 1.0f) * Pml2) / (l - m);
				Pml2 = Pml1;
				Pml1 = Pml;
				SHb[SHindex(-ptrdiff_t(m), l)] = Pml;
				SHb[SHindex(ptrdiff_t(m), l)] = Pml;
			}
		}
	}

	float Cm = s.x;
	float Sm = s.y;
	for (size_t m = 1; m <= numBands; m++)
	{
		for (size_t l = m; l < numBands; l++)
		{
			SHb[SHindex(-ptrdiff_t(m), l)] *= Sm;
			SHb[SHindex(ptrdiff_t(m), l)] *= Cm;
		}
		const float Cm1 = Cm * s.x - Sm * s.y;
		const float Sm1 = Sm * s.x + Cm * s.y;
		Cm = Cm1;
		Sm = Sm1;
	}
}

float SincWindow(size_t l, float w)
{
	if (l == 0)
	{
		return 1.0f;
	}
	else if (l >= w)
	{
		return 0.0f;
	}

	float x = (kPi * l) / w;
	x = std::sin(x) / x;
	return std::pow(x, 4);
}

struct float5
{
	float V[5] = {};

	constexpr float5() = default;
	constexpr float5(float a, float b, float c, float d, float e)
		: V{ a, b, c, d, e }
	{
	}
	constexpr float operator[](size_t i) const { return V[i]; }
	float& operator[](size_t i) { return V[i]; }
};

float5 Multiply(const float5 M[5], float5 x)
{
	return float5{
		M[0][0] * x[0] + M[1][0] * x[1] + M[2][0] * x[2] + M[3][0] * x[3] + M[4][0] * x[4],
		M[0][1] * x[0] + M[1][1] * x[1] + M[2][1] * x[2] + M[3][1] * x[3] + M[4][1] * x[4],
		M[0][2] * x[0] + M[1][2] * x[1] + M[2][2] * x[2] + M[3][2] * x[3] + M[4][2] * x[4],
		M[0][3] * x[0] + M[1][3] * x[1] + M[2][3] * x[2] + M[3][3] * x[3] + M[4][3] * x[4],
		M[0][4] * x[0] + M[1][4] * x[1] + M[2][4] * x[2] + M[3][4] * x[3] + M[4][4] * x[4]
	};
}

float3 RotateSphericalHarmonicBand1(float3 band1, const float3x3& M)
{
	constexpr float3x3 invA1TimesK = {
		{ 0.0f, -1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ -1.0f, 0.0f, 0.0f }
	};

	const float3 MN0 = M[0];
	const float3 MN1 = M[1];
	const float3 MN2 = M[2];
	const float3x3 R1OverK = {
		{ -MN0.y, MN0.z, -MN0.x },
		{ -MN1.y, MN1.z, -MN1.x },
		{ -MN2.y, MN2.z, -MN2.x }
	};

	return R1OverK * (invA1TimesK * band1);
}

float5 RotateSphericalHarmonicBand2(const float5& band2, const float3x3& M)
{
	constexpr float n = kOneOverSqrt2;

	constexpr float5 invATimesK[5] = {
		{ 0, 1, 2, 0, 0 },
		{ -1, 0, 0, 0, -2 },
		{ 0, kSqrt3, 0, 0, 0 },
		{ 1, 1, 0, -2, 0 },
		{ 2, 1, 0, 0, 0 }
	};

	auto project = [](float3 s) -> float5
	{
		return {
			s.y * s.x,
			-(s.y * s.z),
			1 / (2 * kSqrt3) * ((3 * s.z * s.z - 1)),
			-(s.z * s.x),
			0.5f * ((s.x * s.x - s.y * s.y))
		};
	};

	const float5 invATimesKTimesBand2 = Multiply(invATimesK, band2);

	const float5 ROverK[5] = {
		project(M[0]),
		project(M[2]),
		project(n * (M[0] + M[1])),
		project(n * (M[0] + M[2])),
		project(n * (M[1] + M[2]))
	};

	return Multiply(ROverK, invATimesKTimesBand2);
}

float SHMin(std::array<float, 9> f)
{
	constexpr float M_SQRT_PI = kSqrtPi;
	constexpr float M_SQRT_3 = kSqrt3;
	constexpr float M_SQRT_5 = kSqrt5;
	constexpr float M_SQRT_15 = kSqrt15;
	constexpr float A[9] = {
		1.0f / (2.0f * M_SQRT_PI),
		-M_SQRT_3 / (2.0f * M_SQRT_PI),
		M_SQRT_3 / (2.0f * M_SQRT_PI),
		-M_SQRT_3 / (2.0f * M_SQRT_PI),
		M_SQRT_15 / (2.0f * M_SQRT_PI),
		-M_SQRT_15 / (2.0f * M_SQRT_PI),
		M_SQRT_5 / (4.0f * M_SQRT_PI),
		-M_SQRT_15 / (2.0f * M_SQRT_PI),
		M_SQRT_15 / (4.0f * M_SQRT_PI)
	};

	auto rotateSh3Bands = [](const std::array<float, 9>& sh, const float3x3& M) -> std::array<float, 9>
	{
		const float b0 = sh[0];
		const float3 band1{ sh[1], sh[2], sh[3] };
		const float3 b1 = RotateSphericalHarmonicBand1(band1, M);
		const float5 band2{ sh[4], sh[5], sh[6], sh[7], sh[8] };
		const float5 b2 = RotateSphericalHarmonicBand2(band2, M);
		return { b0, b1[0], b1[1], b1[2], b2[0], b2[1], b2[2], b2[3], b2[4] };
	};

	const float3 dir = normalize(float3{ -f[3], -f[1], f[2] });
	const float3 zAxis = -dir;
	const float3 xAxis = normalize(cross(zAxis, float3{ 0, 1, 0 }));
	const float3 yAxis = cross(xAxis, zAxis);
	const float3x3 M = transpose(float3x3{ xAxis, yAxis, -zAxis });

	f = rotateSh3Bands(f, M);

	const float m2max = A[8] * std::sqrt(f[8] * f[8] + f[4] * f[4]);

	const float a = 3 * A[6] * f[6] + m2max;
	const float b = A[2] * f[2];
	const float c = A[0] * f[0] - A[6] * f[6] - m2max;

	const float zmin = -b / (2.0f * a);
	const float m0minZ = a * zmin * zmin + b * zmin + c;
	const float m0minB = std::min(a + b + c, a - b + c);

	const float m0min = (a > 0 && zmin >= -1 && zmin <= 1) ? m0minZ : m0minB;

	const float d = A[4] * std::sqrt(f[5] * f[5] + f[7] * f[7]);

	float minimum = m0min - 0.5f * d;
	if (minimum < 0)
	{
		auto func = [=](float x)
		{
			return (a * x * x + b * x + c) + (d * x * std::sqrt(1 - x * x));
		};

		auto increment = [=](float x)
		{
			return (x * x - 1) * (d - 2 * d * x * x + (b + 2 * a * x) * std::sqrt(1 - x * x))
				/ (3 * d * x - 2 * d * x * x * x - 2 * a * std::pow(1 - x * x, 1.5f));
		};

		float dz;
		float z = -kOneOverSqrt2;
		do
		{
			minimum = func(z);
			dz = increment(z);
			z = z - dz;
		} while (std::abs(z) <= 1 && std::abs(dz) > 1e-5f);

		if (std::abs(z) > 1)
			minimum = std::min(func(1), func(-1));
	}
	return minimum;
}

void WindowSH(float3* sh, size_t numBands, float cutoff)
{
	using SH3 = std::array<float, 9>;

	auto windowing = [numBands](SH3 f, float w) -> SH3
	{
		for (size_t l = 0; l < numBands; l++)
		{
			const float window = SincWindow(l, w);
			f[SHindex(0, l)] *= window;
			for (size_t m = 1; m <= l; m++)
			{
				f[SHindex(-ptrdiff_t(m), l)] *= window;
				f[SHindex(ptrdiff_t(m), l)] *= window;
			}
		}
		return f;
	};

	if (cutoff == 0)
	{
		cutoff = float(numBands * 4 + 1);
		SH3 SH = {};
		for (size_t channel = 0; channel < 3; channel++)
		{
			for (size_t i = 0; i < numBands * numBands; i++)
				SH[i] = sh[i][channel];

			float l = float(numBands);
			float r = cutoff;
			for (size_t i = 0; i < 16 && l + 0.1f < r; i++)
			{
				const float m = 0.5f * (l + r);
				if (SHMin(windowing(SH, m)) < 0)
					r = m;
				else
					l = m;
			}
			cutoff = std::min(cutoff, l);
		}
	}

	for (size_t l = 0; l < numBands; l++)
	{
		const float w = SincWindow(l, cutoff);
		sh[SHindex(0, l)] *= w;
		for (size_t m = 1; m <= l; m++)
		{
			sh[SHindex(-ptrdiff_t(m), l)] *= w;
			sh[SHindex(ptrdiff_t(m), l)] *= w;
		}
	}
}

std::vector<float3> ComputeSH(const Cubemap& cm, size_t numBands, bool irradiance)
{
	const size_t numCoefs = numBands * numBands;
	std::vector<float3> SH(numCoefs, float3(0.0f));

	struct State
	{
		size_t NumCoefs = 0;
		std::unique_ptr<float3[]> Sh;
		std::unique_ptr<float[]> Shb;

		State() = default;
		explicit State(size_t c)
			: NumCoefs(c)
			, Sh(new float3[c]{})
			, Shb(new float[c]{})
		{
		}
		State& operator=(const State& rhs)
		{
			NumCoefs = rhs.NumCoefs;
			Sh.reset(new float3[rhs.NumCoefs]{});
			Shb.reset(new float[rhs.NumCoefs]{});
			return *this;
		}
	};

	ProcessFaces(const_cast<Cubemap&>(cm), State(numCoefs),
		[&](State& state, size_t y, Cubemap::Face f, Cubemap::Texel* data, size_t dim)
		{
			for (size_t x = 0; x < dim; ++x, ++data)
			{
				const float3 s = cm.GetDirectionFor(f, x, y);
				float3 color = Cubemap::SampleAt(data);
				color *= SolidAngle(dim, x, y);
				ComputeShBasis(state.Shb.get(), numBands, s);
				for (size_t i = 0; i < numCoefs; i++)
					state.Sh[i] += color * state.Shb[i];
			}
		},
		[&](State& state)
		{
			for (size_t i = 0; i < numCoefs; i++)
				SH[i] += state.Sh[i];
		});

	std::vector<float> K = Ki(numBands);

	if (irradiance)
	{
		for (size_t l = 0; l < numBands; l++)
		{
			const float truncatedCosSH = ComputeTruncatedCosSH(l);
			K[SHindex(0, l)] *= truncatedCosSH;
			for (size_t m = 1; m <= l; m++)
			{
				K[SHindex(ptrdiff_t(m), l)] *= truncatedCosSH;
				K[SHindex(-ptrdiff_t(m), l)] *= truncatedCosSH;
			}
		}
	}

	for (size_t i = 0; i < numCoefs; i++)
		SH[i] *= K[i];

	return SH;
}

IBLCubemapLevel ExportLevel(const Cubemap& cm)
{
	const size_t dim = cm.GetDimensions();
	IBLCubemapLevel out;
	out.FaceSize = uint32_t(dim);
	for (size_t f = 0; f < 6; ++f)
	{
		out.Faces[f].resize(dim * dim);
		const Image& image = cm.GetImageForFace((Cubemap::Face)f);
		for (size_t y = 0; y < dim; ++y)
		{
			for (size_t x = 0; x < dim; ++x)
				out.Faces[f][y * dim + x] = Cubemap::SampleAt(image.GetPixelRef(x, y));
		}
	}
	return out;
}

std::vector<BakedLevel> BuildMipChain(BakedLevel base)
{
	std::vector<BakedLevel> levels;
	levels.push_back(std::move(base));
	while (levels.back().second->GetDimensions() > 1)
	{
		const size_t dim = levels.back().second->GetDimensions() / 2;
		BakedLevel level = CreateEmptyLevel(dim);
		DownsampleCubemapLevelBoxFilter(*level.second, *levels.back().second);
		levels.push_back(std::move(level));
	}
	return levels;
}

} // namespace

std::expected<IBLBakeOutput, std::string> IBLBaker::Bake(
	const float* rgba, uint32_t width, uint32_t height, const IBLBakeParams& params)
{
	ZoneScoped;

	if (!rgba)
		return std::unexpected("IBLBaker: null pixel buffer");

	if (width < 2 || height < 1 || width != 2 * height)
		return std::unexpected(std::format("IBLBaker: expected equirectangular image (width == 2 * height), got {}x{}", width, height));

	auto isPOT = [](uint32_t v) { return v != 0 && (v & (v - 1)) == 0; };
	if (!isPOT(params.SkySize) || !isPOT(params.LambertSize) || !isPOT(params.GgxSize) || !isPOT(params.GgxMinLodSize))
		return std::unexpected("IBLBaker: bake sizes must be powers of two");
	if (params.SkySize < 4 || params.LambertSize < 4 || params.GgxSize < 4 || params.GgxMinLodSize > params.GgxSize)
		return std::unexpected("IBLBaker: invalid bake sizes");
	if (params.GgxSampleCount == 0 || params.IrradianceSampleCount == 0 || params.DfgLutSize == 0)
		return std::unexpected("IBLBaker: sample counts and LUT size must be non-zero");

	const size_t numBands = glm::clamp(params.ShBands, 0u, 3u);

	Image source(width, height);
	for (uint32_t y = 0; y < height; ++y)
	{
		auto* row = static_cast<float3*>(source.GetPixelRef(0, y));
		for (uint32_t x = 0; x < width; ++x)
		{
			const float* src = rgba + (size_t(y) * width + x) * 4;
			const uint32_t dstX = params.MirrorX ? width - 1 - x : x;
			row[dstX] = glm::min(float3{ src[0], src[1], src[2] }, float3(HalfMax));
		}
	}

	if (params.KarisClamp)
		ClampImage(source);

	IBLBakeOutput output;

	{
		BakedLevel skyBase = CreateEmptyLevel(params.SkySize);
		EquirectangularToCubemap(*skyBase.second, source);
		std::vector<BakedLevel> skyChain = BuildMipChain(std::move(skyBase));

		output.Sky.reserve(skyChain.size());
		for (const BakedLevel& level : skyChain)
			output.Sky.push_back(ExportLevel(*level.second));

		if (numBands > 0)
		{
			std::vector<float3> sh = ComputeSH(*skyChain.front().second, numBands, true);
			WindowSH(sh.data(), numBands, 0.0f);
			for (size_t i = 0; i < output.Sh.size(); ++i)
				output.Sh[i] = i < sh.size() ? sh[i] : float3(0.0f);
		}
	}

	{
		BakedLevel lambertBase = CreateEmptyLevel(params.LambertSize);
		EquirectangularToCubemap(*lambertBase.second, source);
		std::vector<BakedLevel> lambertChain = BuildMipChain(std::move(lambertBase));
		const std::vector<const Cubemap*> lambertLevels = LevelPtrs(lambertChain);

		const BakedLevel irradiance = CreateEmptyLevel(params.LambertSize);
		DiffuseIrradiance(*irradiance.second, lambertLevels, params.IrradianceSampleCount);
		output.Lambert.push_back(ExportLevel(*irradiance.second));
	}

	{
		BakedLevel ggxBase = CreateEmptyLevel(params.GgxSize);
		EquirectangularToCubemap(*ggxBase.second, source);
		std::vector<BakedLevel> ggxChain = BuildMipChain(std::move(ggxBase));
		const std::vector<const Cubemap*> ggxLevels = LevelPtrs(ggxChain);

		const uint32_t maxMip = std::countr_zero(params.GgxSize);
		const uint32_t minMip = std::countr_zero(params.GgxMinLodSize);
		const uint32_t ggxMipCount = maxMip - minMip + 1;
		output.Ggx.reserve(ggxMipCount);
		for (uint32_t l = 0; l < ggxMipCount; ++l)
		{
			const BakedLevel level = CreateEmptyLevel(params.GgxSize >> l);
			const float perceptualRoughness = ggxMipCount > 1 ? float(l) / float(ggxMipCount - 1) : 0.0f;
			const float linearRoughness = perceptualRoughness * perceptualRoughness;
			RoughnessFilter(*level.second, ggxLevels, linearRoughness, params.GgxSampleCount, true);
			output.Ggx.push_back(ExportLevel(*level.second));
		}
	}

	output.Dfg.Size = params.DfgLutSize;
	output.Dfg.HasCharlie = params.DfgCharlie;
	ComputeDFGLut(output.Dfg.Texels, params.DfgLutSize, params.DfgMultiscatter, params.DfgCharlie);

	return output;
}

void IBLBaker::PreprocessSHForShader(std::array<float3, 9>& sh)
{
	constexpr float A[9] = {
		1.0f / (2.0f * kSqrtPi),
		-kSqrt3 / (2.0f * kSqrtPi),
		kSqrt3 / (2.0f * kSqrtPi),
		-kSqrt3 / (2.0f * kSqrtPi),
		kSqrt15 / (2.0f * kSqrtPi),
		-kSqrt15 / (2.0f * kSqrtPi),
		kSqrt5 / (4.0f * kSqrtPi),
		-kSqrt15 / (2.0f * kSqrtPi),
		kSqrt15 / (4.0f * kSqrtPi)
	};

	for (size_t i = 0; i < 9; i++)
		sh[i] *= A[i] * kOneOverPi;
}

} // namespace alm::gfx
