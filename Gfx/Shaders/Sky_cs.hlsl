#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Common.hlsli"

// Physically-based atmospheric scattering using the Rayleigh + Mie model.
//
// The core idea: a ray from the camera travels through the atmosphere.
// At each point along the ray, sunlight scatters toward the camera.
// How much sunlight reaches each point depends on how much atmosphere
// the sun ray traverses to get there (inner raymarch).
// How much of that scattered light reaches the camera depends on how much
// atmosphere the camera ray traverses (outer raymarch).
// Both are computed via Beer-Lambert exponential attenuation.
//
// Rayleigh scattering: caused by air molecules (N2, O2).
//   - Scattering intensity proportional to 1/lambda^4 -- blue light scatters much more than red.
//   - Responsible for the blue sky and red/orange sunsets.
//   - Molecules distributed with scale height Hr (~8km) -- most are in the lower atmosphere.
//
// Mie scattering: caused by larger particles (aerosols, dust, water droplets).
//   - Scattering roughly wavelength-independent -- produces white/grey haze.
//   - Strongly forward-scattering -- responsible for the bright halo around the sun.
//   - Particles distributed with scale height Hm (~1.2km) -- concentrated near the surface.
//
// References:
//   Scratchapixel -- Simulating Colors of the Sky
//   https://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds/simulating-sky/simulating-colors-of-the-sky.html
//   StillTravelling -- Shadertoy sky + clouds shader
//   https://www.shadertoy.com/view/tdSXzD

ConstantBuffer<interop::SkyConstants> Constants : register(b0);

// Output of the scattering raymarch.
// Separating inscattering and transmittance allows the caller to compose
// the sky correctly over scene geometry (aerial perspective):
//   finalColor = sceneColor * transmittance + inscattering
struct ScatterResult
{
    float3 inscattering;  // Light scattered toward the camera along the ray
    float3 transmittance; // Fraction of scene color that survives atmospheric absorption
                          // (1,1,1) = fully transparent atmosphere, (0,0,0) = fully opaque
};

// Computes Rayleigh + Mie atmospheric scattering along a ray.
//
// rayOriginLocal: camera position in world space
// rayDir:         normalized ray direction in world space
// sceneDepth:     distance to the nearest scene geometry along the ray (from depth buffer)
//                 used to limit the raymarch and enable aerial perspective over geometry
// skyData:        per-frame atmospheric parameters (radii, scattering coefficients, sun, etc.)
ScatterResult Scatter(float3 rayOriginLocal, float3 rayDir, float sceneDepth, ConstantBuffer<interop::SkyData> skyData)
{
    ScatterResult result;
    result.inscattering  = float3(0, 0, 0); // No light scattered by default
    result.transmittance = float3(1, 1, 1); // Fully transparent atmosphere by default

    // Translate ray origin to Earth-centered coordinates.
    // All sphere intersections assume the Earth center is at the origin.
    float3 rayOrigin = rayOriginLocal - skyData.EarthCenter;

    // Find where the ray enters and exits the atmosphere sphere.
    // atmosHit.x = entry distance (negative if camera is already inside atmosphere)
    // atmosHit.y = exit distance (negative if ray misses atmosphere entirely)
    float2 atmosHit = RaySphereIntersection(rayOrigin, rayDir, float3(0, 0, 0), skyData.AtmosRadius);
    if (atmosHit.y < 0.0)
        return result; // Ray misses the atmosphere -- looking at deep space

    // Raymarch range along the view ray:
    //   tStart: atmosphere entry. max(atmosHit.x, 0) handles the case where the camera
    //           is already inside the atmosphere (atmosHit.x would be negative).
    //   tEnd:   atmosphere exit, clamped to scene geometry distance.
    //           The depth buffer already accounts for all opaque geometry (terrain, buildings, etc.)
    //           so no separate Earth sphere check is needed for the view ray.
    float tStart = max(atmosHit.x, 0.0);
    float tEnd   = min(atmosHit.y, sceneDepth);

    // Guard against degenerate cases where geometry is in front of the atmosphere entry
    // (e.g. camera in space looking at a nearby object that occludes the atmosphere).
    if (tEnd <= tStart)
        return result;

    float L  = tEnd - tStart;                    // Total raymarch length
    float dL = L / float(skyData.NumSteps);      // Step size

    float3 accumR = 0.0; // Accumulated Rayleigh in-scattering (float3: per RGB channel)
    float3 accumM = 0.0; // Accumulated Mie in-scattering (float3: per RGB channel)
    float  depthR = 0.0; // Rayleigh optical depth from camera to current sample point
    float  depthM = 0.0; // Mie optical depth from camera to current sample point

    // --- Outer raymarch: integrate scattering along the view ray ---
    for (uint i = 0; i < skyData.NumSteps; ++i)
    {
        // Sample at the center of each segment for better numerical accuracy
        // (avoids bias toward the start of each step)
        float  t = tStart + dL * (i + 0.5);
        float3 p = rayOrigin + rayDir * t;

        // Altitude of this sample point above the Earth surface (in meters)
        float h = length(p) - skyData.EarthRadius;

        // Density of scattering particles at altitude h.
        // Both Rayleigh and Mie densities follow an exponential falloff with altitude,
        // governed by their respective scale heights (Hr and Hm).
        // dR and dM are the optical depth contributions for this step segment.
        float dR = exp(-h / skyData.Hr) * dL;
        float dM = exp(-h / skyData.Hm) * dL;

        // Accumulate optical depth along the view ray up to this point.
        // This represents how much atmosphere the camera ray has traversed so far.
        depthR += dR;
        depthM += dM;

        // --- Earth shadow check for inner sun ray ---
        // Use an ideal sphere as an approximation for the shadow terminator.
        // This is the only place where the Earth sphere is used, because the inner
        // sun ray does not correspond to any camera pixel and has no depth buffer.
        // The error is negligible at atmospheric scale vs terrain scale
        // (e.g. Everest at 8km vs Earth radius at 6360km, ~0.1% error).
        float2 lightEarthHit = RaySphereIntersection(p, skyData.ToSunDirection, float3(0, 0, 0), skyData.EarthRadius);
        if (lightEarthHit.x > 0.0)
            continue; // Point p is in Earth's shadow -- no direct sunlight reaches it
                      // This naturally produces the red/orange colors at sunset

        // --- Inner raymarch: integrate optical depth from p toward the sun ---
        // Computes how much sunlight is attenuated by the atmosphere before reaching p.
        // lightAtmosHit.y is the distance from p to the top of the atmosphere along the sun ray.
        float2 lightAtmosHit = RaySphereIntersection(p, skyData.ToSunDirection, float3(0, 0, 0), skyData.AtmosRadius);
        float  dLs   = lightAtmosHit.y / float(skyData.NumLightSteps);
        float  depthRs = 0.0; // Rayleigh optical depth from p to top of atmosphere (sun ray)
        float  depthMs = 0.0; // Mie optical depth from p to top of atmosphere (sun ray)

        for (uint j = 0; j < skyData.NumLightSteps; ++j)
        {
            // Sample at center of each light ray segment
            float  ts = dLs * (j + 0.5);
            float3 ps = p + skyData.ToSunDirection * ts;
            float  hs = length(ps) - skyData.EarthRadius;

            depthRs += exp(-hs / skyData.Hr) * dLs;
            depthMs += exp(-hs / skyData.Hm) * dLs;
        }

        // Total transmittance from the sun to the camera passing through point p.
        // Combines optical depth along the sun ray (depthRs, depthMs) and the
        // view ray up to p (depthR, depthM) -- Beer-Lambert law applied to both paths.
        float3 T = exp(-(skyData.bR * (depthR + depthRs) + skyData.bM * (depthM + depthMs)));

        // Accumulate weighted in-scattering contributions.
        // T weights each sample by how much light survives both paths (sun->p->camera).
        accumR += T * dR;
        accumM += T * dM;
    }

    // --- Phase functions ---
    // Phase functions describe the angular distribution of scattered light --
    // how much light is scattered toward the camera as a function of the angle
    // between the view ray and the sun direction.
    //
    // mu: cosine of the angle between view ray and sun direction
    //   mu = 1.0  -> looking directly at the sun (forward scattering)
    //   mu = 0.0  -> looking perpendicular to the sun
    //   mu = -1.0 -> looking directly away from the sun (back scattering)
    float mu    = dot(rayDir, skyData.ToSunDirection);
    float opmu2 = 1.0 + mu * mu; // Shared factor (1 + mu²) used by both phase functions

    // Rayleigh phase function: nearly isotropic with a slight forward/backward bias.
    // Constant factor: 3 / (16 * PI) = 0.0596831
    float phaseR = 0.0596831 * opmu2;

    // Mie phase function: Henyey-Greenstein approximation, strongly forward-scattering.
    // G (anisotropy factor): 0 = isotropic, 0.76 = typical clear atmosphere, ~0.99 = laser-like.
    // Higher G produces a tighter, brighter halo around the sun.
    // Constant factor: 3 / (8 * PI) = 0.1193662
    float g2     = square(skyData.G);
    float phaseM = 0.1193662 * ((1.0 - g2) * opmu2) /
                   ((2.0 + g2) * pow(1.0 + g2 - 2.0 * skyData.G * mu, 1.5));

    // Final sky color: sum of Rayleigh and Mie in-scattering, weighted by sun intensity
    // and their respective scattering coefficients (bR, bM) and phase functions.
    float3 color = skyData.SunIntensity * (skyData.bR * phaseR * accumR + skyData.bM * phaseM * accumM);

    // Atmospheric transmittance along the full view ray.
    // Computed once and reused for both the sun disk and the ScatterResult output.
    // Represents how much of the scene color survives atmospheric absorption --
    // objects behind more atmosphere appear darker and more tinted (aerial perspective).
    float3 transmittance = exp(-(skyData.bR * depthR + skyData.bM * depthM));

    // --- Sun disk ---
    // The sun disk is visible only if the view ray reached the atmosphere exit
    // without hitting any geometry. sceneDepth >= atmosHit.y means the depth buffer
    // reported no geometry within the atmosphere along this ray.
    // No Earth sphere check needed -- the depth buffer covers all opaque geometry.
    bool sunVisible = sceneDepth >= atmosHit.y;
    if (sunVisible)
    {
        // Optical illusion: the sun appears larger near the horizon due to
        // the brain comparing it against the visible terrain and horizon.
        // This is a deliberate artistic approximation -- not physically correct.
        // At zenith (sunElevation = 1): horizonBoost = 1 (real angular size)
        // At horizon (sunElevation = 0): horizonBoost = 10 (10x larger)
        float sunElevation       = skyData.ToSunDirection.y;
        float horizonBoost       = 1.0 + 9.0 * pow(saturate(1.0 - sunElevation), 8.0);
        float effectiveRadius    = skyData.SunAngularRadius * horizonBoost;
        float effectiveRadiusCos = cos(effectiveRadius);

        // Check if this ray points inside the (boosted) sun disk
        if (mu > effectiveRadiusCos)
        {
            // Normalized radial position on the sun disk.
            // diskPos = 0 at the center, diskPos = 1 at the edge.
            float angle   = acos(mu);
            float diskPos = saturate(angle / effectiveRadius);

            // muSun: cosine of the angle from the disk center, derived from diskPos.
            // Equivalent to projecting diskPos onto the surface of a unit hemisphere --
            // the same geometry used in limb darkening models for stellar surfaces.
            float muSun = sqrt(1.0 - square(diskPos));

            // Quadratic limb darkening model.
            // Models the apparent brightness variation across the solar disk:
            // the sun's limb (edge) appears darker than the center because the
            // line of sight through the solar atmosphere is longer at the edge,
            // traversing cooler, less luminous layers.
            // At center (muSun = 1): limbDarkening = 1.0 (full brightness)
            // At edge   (muSun = 0): limbDarkening = 0.0 (dark limb)
            float oneMinusMuSun = 1.0 - muSun;
            float limbDarkening = 1.0 - 0.6 * oneMinusMuSun - 0.4 * square(oneMinusMuSun);

            // Anti-aliased edge: fade to zero over approximately one pixel at the disk boundary.
            // SunEdgeAAFalloff = 1 / sunRadiusInPixels, precomputed on the CPU.
            // Without this, the hard cutoff at the disk edge causes aliasing artifacts
            // (jagged edge, brightness flickering) as the camera rotates.
            float edgeAA = saturate((1.0 - diskPos) / skyData.SunEdgeAAFalloff);

            // Add sun disk contribution, attenuated by atmospheric transmittance.
            // transmittance here causes the sun to appear redder and dimmer near the horizon,
            // where the view ray traverses more atmosphere (Beer-Lambert).
            color += skyData.SunRadiance * transmittance * limbDarkening * edgeAA;
        }
    }

    result.inscattering  = color;
    result.transmittance = transmittance;
    return result;
}

[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID)
{
    ConstantBuffer<interop::SkyData> skyData    = ResourceDescriptorHeap[Constants.SkyDataDI];
    Texture2D<float>     linearDepthTex         = ResourceDescriptorHeap[skyData.LinearDepthTexDI];
    RWTexture2D<float4>  colorTex               = ResourceDescriptorHeap[skyData.SceneColorDI];

    uint2 pixelPos = dispatchID.xy;

    // Bounds check: discard threads outside the render target dimensions
    if (any(pixelPos >= skyData.SceneColorTexSize))
        return;

    // UV coordinates for this pixel, centered within the pixel (+ 0.5)
    float2 uv = (float2(pixelPos) + 0.5) / float2(skyData.SceneColorTexSize);

    // Reconstruct world-space ray direction from clip-space coordinates.
    // matClipToTranslatedWorld transforms from clip space to world space
    // with the camera at the origin (translated world), so no camera translation
    // is baked into the matrix -- avoids floating point precision issues at large distances.
    float4 clipPos;
    clipPos.x = uv.x * 2.0 - 1.0;
    clipPos.y = 1.0 - uv.y * 2.0; // flip Y: UV origin is top-left, clip space origin is bottom-left
    clipPos.z = 1.0;
    clipPos.w = 1.0;

    float4 rayDirH = mul(Constants.matClipToTranslatedWorld, clipPos);
    float3 rayDir  = normalize(rayDirH.xyz);

    // Read linear depth (distance along the camera forward axis) and convert
    // to scene distance along the ray direction.
    // viewZ: depth along camera forward vector (Z component in view space)
    // sceneDepth: actual distance along rayDir to the geometry
    // The conversion accounts for the angle between rayDir and the forward vector.
    float viewZ      = linearDepthTex.SampleLevel(pointClampSampler, uv, 0.0);
    float cosAngle   = dot(rayDir, skyData.CameraForward);
    float sceneDepth = viewZ / max(cosAngle, 0.0001); // max avoids division by zero at 90 degrees

    // Run atmospheric scattering raymarch
    ScatterResult scatter = Scatter(Constants.CameraPosition, rayDir, sceneDepth, skyData);

    // Compose atmospheric scattering over the existing scene color (aerial perspective):
    //   finalColor = sceneColor * transmittance + inscattering
    //
    // For sky pixels (no geometry): sceneColor = 0 (cleared to black), so finalColor = inscattering
    // For geometry pixels: sceneColor is attenuated by transmittance (distant objects appear
    // darker and tinted) and inscattering adds the atmospheric haze on top
    float3 sceneColor = colorTex[pixelPos].rgb;
    float3 finalColor = sceneColor * scatter.transmittance + scatter.inscattering;

    colorTex[pixelPos] = float4(finalColor, 1.0);
}