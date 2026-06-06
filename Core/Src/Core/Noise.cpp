#include "Core/CorePCH.h"
#include "Core/Noise.h"

namespace
{
	// Hash by David_Hoskins
	constexpr uint32_t UI0 = 1597334673;
	constexpr uint32_t UI1 = 3812015801;
	constexpr uint3 UI3 = uint3{ UI0, UI1, 2798796415 };
	constexpr float UIF = 1.0f / float(0xffffffffU);

	float3 hash33(float3 p)
	{
		uint3 q = uint3(int3(p)) * UI3;
		q = (q.x ^ q.y ^ q.z) * UI3;
		return -1.0f + 2.0f * float3(q) * UIF;
	}
} // anonymous namespace

// Gradient noise by iq (modified to be tileable)
float alm::GradientNoise(const float3& x, float freq)
{
	// grid
	float3 p = floor(x);
	float3 w = fract(x);

	// quintic interpolant
	float3 u = w * w * w * (w * (w * 6.f - 15.f) + 10.f);

	// gradients
	float3 ga = hash33(mod(p + float3(0.f, 0.f, 0.f), freq));
	float3 gb = hash33(mod(p + float3(1.f, 0.f, 0.f), freq));
	float3 gc = hash33(mod(p + float3(0.f, 1.f, 0.f), freq));
	float3 gd = hash33(mod(p + float3(1.f, 1.f, 0.f), freq));
	float3 ge = hash33(mod(p + float3(0.f, 0.f, 1.f), freq));
	float3 gf = hash33(mod(p + float3(1.f, 0.f, 1.f), freq));
	float3 gg = hash33(mod(p + float3(0.f, 1.f, 1.f), freq));
	float3 gh = hash33(mod(p + float3(1.f, 1.f, 1.f), freq));

	// projections
	float va = dot(ga, w - float3(0.f, 0.f, 0.f));
	float vb = dot(gb, w - float3(1.f, 0.f, 0.f));
	float vc = dot(gc, w - float3(0.f, 1.f, 0.f));
	float vd = dot(gd, w - float3(1.f, 1.f, 0.f));
	float ve = dot(ge, w - float3(0.f, 0.f, 1.f));
	float vf = dot(gf, w - float3(1.f, 0.f, 1.f));
	float vg = dot(gg, w - float3(0.f, 1.f, 1.f));
	float vh = dot(gh, w - float3(1.f, 1.f, 1.f));

	// interpolation
	return va +
		u.x * (vb - va) +
		u.y * (vc - va) +
		u.z * (ve - va) +
		u.x * u.y * (va - vb - vc + vd) +
		u.y * u.z * (va - vc - ve + vg) +
		u.z * u.x * (va - vb - ve + vf) +
		u.x * u.y * u.z * (-va + vb + vc - vd + ve - vf - vg + vh);
}

// Tileable 3D worley noise
float alm::WorleyNoise(const float3& uv, float freq)
{
	float3 id = floor(uv);
	float3 p = fract(uv);

	float minDist = 10000.;
	for (float x = -1.; x <= 1.; ++x)
	{
		for (float y = -1.; y <= 1.; ++y)
		{
			for (float z = -1.; z <= 1.; ++z)
			{
				float3 offset = float3(x, y, z);
				float3 h = hash33(mod(id + offset, float3(freq)));
				h = h * 0.5f + 0.5f;
				h += offset;
				float3 d = p - h;
				minDist = std::min(minDist, dot(d, d));
			}
		}
	}

	return 1.0f - alm::Saturate(minDist);
}

// Fbm for Perlin noise based on iq's blog
float alm::PerlinFbm(const float3& p, float freq, int octaves)
{
	float G = exp2(-.85);
	float amp = 1.;
	float noise = 0.;
	for (int i = 0; i < octaves; ++i)
	{
		noise += amp * GradientNoise(p * freq, freq);
		freq *= 2.;
		amp *= G;
	}

	return noise;
}