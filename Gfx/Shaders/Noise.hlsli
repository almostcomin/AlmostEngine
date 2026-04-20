#ifndef __NOISE_HLSLI__
#define __NOISE_HLSLI__

// From Morgan McGuire @morgan3d https://www.shadertoy.com/view/4dS3Wd

// Range [0, 1]
float hash(float p)
{
    p = frac(p * 0.011);
    p *= p + 7.5;
    p *= p + p;
    return frac(p);
}

// Range [0, 1]
float hash(float2 p2)
{
    p2 = 50.0 * frac(p2 * 0.3183099);
    return frac(p2.x * p2.y * (p2.x + p2.y));
}

float hash(float3 p3)
{
    p3 = frac(p3 * 1031.1031);
    p3 += dot(p3, p3.yzx + 19.19);
    return frac((p3.x + p3.y) * p3.z);
}

// Range [0, 1]
float2 hash2(float2 p)
{
    return float2(
        hash(p),
        hash(p + float2(127.1, 311.7))
    );
}

// Range [0, 1]
float ValueNoise(float x)
{
    float i = floor(x);
    float f = frac(x);
    float u = f * f * (3.0 - 2.0 * f);
    return lerp(hash(i), hash(i + 1.0), u);
}

// Range [0, 1]
float ValueNoise(float2 x)
{
    float2 i = floor(x);
    float2 f = frac(x);

	// Four corners in 2D of a tile
    float a = hash(i);
    float b = hash(i + float2(1.0, 0.0));
    float c = hash(i + float2(0.0, 1.0));
    float d = hash(i + float2(1.0, 1.0));

    // Simple 2D lerp using smoothstep envelope between the values.
	// return vec3(lerp(lerp(a, b, smoothstep(0.0, 1.0, f.x)),
	//			lerp(c, d, smoothstep(0.0, 1.0, f.x)),
	//			smoothstep(0.0, 1.0, f.y)));

	// Same code, with the clamps in smoothstep and common subexpressions
	// optimized away.
    float2 u = f * f * (3.0 - 2.0 * f);
    return lerp(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

// Gradient Noise by Inigo Quilez - iq/2013
// https://www.shadertoy.com/view/XdXGW8
// Range [-1, 1]
float GradientNoise(float2 st)
{
    float2 i = floor(st);
    float2 f = frac(st);

    // Gradients for each corner
    float2 g00 = hash2(i + float2(0.0, 0.0)) * 2.0 - 1.0;
    float2 g10 = hash2(i + float2(1.0, 0.0)) * 2.0 - 1.0;
    float2 g01 = hash2(i + float2(0.0, 1.0)) * 2.0 - 1.0;
    float2 g11 = hash2(i + float2(1.0, 1.0)) * 2.0 - 1.0;
    
    // Offset vectors
    float2 d00 = f - float2(0.0, 0.0);
    float2 d10 = f - float2(1.0, 0.0);
    float2 d01 = f - float2(0.0, 1.0);
    float2 d11 = f - float2(1.0, 1.0);
    
    // Dot products gradient.offset
    float n00 = dot(g00, d00);
    float n10 = dot(g10, d10);
    float n01 = dot(g01, d01);
    float n11 = dot(g11, d11);
    
    float2 u = smoothstep(0, 1, f);
    
    return lerp(lerp(n00, n10, u.x),
                lerp(n01, n11, u.x), u.y);
}

// Calcs the GradientNoise gradient using finite differences
float2 GradientNoiseGradient(float2 st, float h)
{
    float2 grad;
    grad.x = (GradientNoise(st + float2(h, 0.0)) - GradientNoise(st - float2(h, 0.0))) / (2.0 * h);
    grad.y = (GradientNoise(st + float2(0.0, h)) - GradientNoise(st - float2(0.0, h))) / (2.0 * h);
    return grad;
}

// velocity: flow direction & magnitude
// flowAmount: intensity of the deformation, typically [0.1, 0.5]
// time: time to perform the animation
// h: needed to compute finite differences, typically around 0.001
float FlowNoise(float2 st, float2 velocity, float flowAmount, float time, float h)
{
    float2 displaced = st - velocity * time;
    float2 grad = GradientNoiseGradient(displaced, h);
    displaced += grad * flowAmount;
    return GradientNoise(displaced);
}

float GradientFbm(float2 st, uint octaves, float frequency, float lacunarity, float amplitude, float gain)
{
    float v = 0.0;
    for (int i = 0; i < octaves; i++)
    {
        v += amplitude * GradientNoise(frequency * st);
        frequency *= lacunarity;
        amplitude *= gain;
    }
    return v;
}

float GradientFbm(float2 st, uint octaves)
{
    static const float FREQUENCY = 1.0;
    static const float LACUNARITY = 2.0;
    static const float AMPLITUDE = 0.5;
    static const float GAIN = 0.5;
    
    return GradientFbm(st, octaves, FREQUENCY, LACUNARITY, AMPLITUDE, GAIN);
}

float FlowFbm(float2 st, uint octaves, float frequency, float lacunarity, float amplitude, float gain, float2 velocity, float flowAmount,
              float time, float h)
{
    float v = 0.0;
    for (int i = 0; i < octaves; i++)
    {
        float2 displaced = st * frequency - velocity * time;
        float2 grad = GradientNoiseGradient(displaced, h);
        displaced += grad * flowAmount;
        
        v += amplitude * GradientNoise(displaced);
        
        frequency *= lacunarity;
        amplitude *= gain;
    }
    return v;
}

float FlowFbm(float2 st, uint octaves, float2 velocity, float flowAmount, float time)
{
    static const float FREQUENCY = 1.0;
    static const float LACUNARITY = 2.0;
    static const float AMPLITUDE = 0.5;
    static const float GAIN = 0.5;
    static const float H = 0.001;
    
    return FlowFbm(st, octaves, FREQUENCY, LACUNARITY, AMPLITUDE, GAIN, velocity, flowAmount, time, H);
}

float Turbulence(float2 st, uint octaves, float amplitude)
{
    float v = 0.0;
    for (int i = 0; i < octaves; i++)
    {
        v += amplitude * abs(GradientNoise(st));
        st *= 2.0;
        amplitude *= 0.5;
    }
    
    return v;
}

float Ridge(float2 st, uint octaves, float amplitude, float offset)
{
    float v = 0.0;
    for (int i = 0; i < octaves; i++)
    {
        float noise = offset - abs(GradientNoise(st));
        noise = noise * noise;
        v += noise * amplitude;
        st *= 2.0;
        amplitude *= 0.5;
    }
    
    return v;
}

float NormalizeNoise(float v)
{
    static const float NOISE_MIN = -0.6;
    static const float NOISE_MAX = 0.6;
    
    v = (v - NOISE_MIN) / (NOISE_MAX - NOISE_MIN);
    v = saturate(v);
    //v = smoothstep(0.0, 1.0, v);
    return v;
}

#endif