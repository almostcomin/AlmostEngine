#include "Interop/RenderResources.h"
#include "BindlessRS.hlsli"
#include "Noise.hlsli"
#include "Common.hlsli"

// Keep in sync with alm::gfx::CloudsRenderStage::DebugChannel
static const uint DebugChannel_Disabled = 0;
static const uint DebugChannel_Clouds_Transmitance = 1;

static const float DETAIL_LOW_FREQ = 8.0;   // Must be the lowest frequency of the detail texture

ConstantBuffer<interop::CloudsConstants> Constants : register(b0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct ShadowResult
{
    float lightEnergy;
    float opticalDepth;
};

struct CloudResult
{
    float3 color;
    float transmittance;
    float weightedDistance; // Density-weighted average distance
    float tEntry;
    float tExit;
};

// Step-decorrelated jitter.
// pixelPos in pixels, step = sample ID. Returns [0, 1].
float StepJitter(float2 pixelPos, uint step, float salt)
{
    // Golden ratio breaks step-to-step correlation.
    // Salt prevents main/shadow jitter periodicity conflicts.
    return frac(InterleavedGradientNoise(pixelPos + float2(step, step * 1.61803398875)) + salt);
}

// Stratus: thin band near base.
float StratusProfile(float norY)
{
    float rampIn = smoothstep(0.05, 0.10, norY);
    float rampOut = 1.0 - smoothstep(0.20, 0.25, norY);
    return saturate(rampIn * rampOut);
}

// Cumulus: base lifts from 0.05 to 0.20, wispy top fade 0.55-0.85.
float CumulusProfile(float norY)
{
    float rampIn = smoothstep(0.05, 0.20, norY);
    float rampOut = 1.0 - smoothstep(0.55, 0.70, norY);
    return saturate(rampIn * rampOut);
}

// Cumulonimbus: sharper base lift 0.05-0.15, top fade only at 0.90-1.00.
float CumulonimbusProfile(float norY)
{
    float rampIn = smoothstep(0.05, 0.15, norY);
    float rampOut = 1.0 - smoothstep(0.90, 1.00, norY);
    return saturate(rampIn * rampOut);
}

// Weighted blend of the three profiles
float CloudCoverageShape(float norY, float stratusWeight, float cumulusWeight, float cumulonimbusWeight)
{
    float s = StratusProfile(norY) * stratusWeight
            + CumulusProfile(norY) * cumulusWeight
            + CumulonimbusProfile(norY) * cumulonimbusWeight;
    return saturate(s);
}

// Density 1 at the base, 0.3 at the top.
float GlobalHeightGradient(float norY)
{
    return lerp(1.0, 0.55, smoothstep(0.0, 1.0, norY));
}

// Henyey-Greenstein phase function, forward-scattering anisotropy.
// g in [-1, 1]: g > 0 favors forward scatter, g < 0 backward.
// For water droplets g ~ 0.6-0.8 (very forward).
float HenyeyGreenstein(float cosTheta, float g)
{
    float g2 = square(g);
    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * M_PI * denom * sqrt(denom));
}

// Dual-lobe HG with directional weighting.
// Forward lobe dominates when looking AT the sun (cosTheta → 1).
// Backward lobe dominates when looking AWAY from the sun (cosTheta → -1).
// At side scatter (cosTheta = 0), it's a 50/50 blend.
float DualLobeHG(float cosTheta, float forwardG, float backwardG)
{
    float forward = HenyeyGreenstein(cosTheta, forwardG);
    float backward = HenyeyGreenstein(cosTheta, backwardG);
    // Directional weight: 1 at cosTheta=1 (forward), 0 at cosTheta=-1 (backward).
    float forwardWeight = saturate(cosTheta * 0.5 + 0.5);
    return forward * forwardWeight + backward * (1.0 - forwardWeight);
}

// This models the "dark edges facing the light" and the in-scattering concentration in dense regions.
float PowderEffect(float density, float width, float strength)
{
    float edge = smoothstep(0.0, width, density);
    return lerp(1.0, edge, strength);
}

float SampleCloudDensity(float3 pos, float norY, Texture3D cloudsTexture, Texture3D cloudsDetailTexture,
    ConstantBuffer <interop::CloudsData> cloudsData)
{    
    // Si norY está fuera del rango válido [0,1], no hay densidad
    if (norY < 0.0 || norY > 1.0)
        return 0.0;

    // --- BASE SHAPE

    float3 uvw1;
    uvw1.xy = pos.xz * cloudsData.cloudsScale;
    uvw1.xy += cloudsData.windOffset;
    uvw1.z = norY;
    float3 uvw2;
    uvw2.xy = pos.xz * cloudsData.cloudsScale * 0.37; // prime number
    uvw2.xy += cloudsData.windOffset * 0.73;
    uvw2.z = frac(norY + 0.5);
        
    float4 noise1 = cloudsTexture.SampleLevel(linearWrapSampler, uvw1, 0.0);
    float4 noise2 = cloudsTexture.SampleLevel(linearWrapSampler, uvw2, 0.0);
    float4 noise = lerp(noise1, noise2, 0.3);

    float perlinWorley = noise.r;
    float3 worley = noise.gba;
    
    // cloud shape modeled after the GPU Pro 7 chapter
    float wfbm = worley.r * 0.625 + worley.g * 0.25 + worley.b * 0.125;
    float coverage = remap(perlinWorley, wfbm - 1.0, 1.0, 0.0, 1.0);
    coverage = remap(coverage, 1.0 - cloudsData.coverage, 1.0, 0.0, 1.0);

    if (coverage <= 0.0)
        return 0.0;

    coverage *= CloudCoverageShape(norY, cloudsData.stratusWeight, cloudsData.cumulusWeight, cloudsData.cumulonimbusWeight);
    coverage *= GlobalHeightGradient(norY);
    coverage = saturate(coverage);

    // -- DETAIL EROSION

    float3 detailUVW;
    detailUVW.xy = pos.xz * cloudsData.cloudsScale * cloudsData.detailScale;
    detailUVW.xy += cloudsData.windOffset * cloudsData.detailScale;
    detailUVW.z  = norY;

    float3 detail = cloudsDetailTexture.SampleLevel(linearWrapSampler, detailUVW, 0.0).rgb;
    float hfbm = detail.r * 0.625 + detail.g * 0.25 + detail.b * 0.125;

    // Per paper: in the lower half (near the layer floor),
    // Worley is inverted -> produces wispy/transparent edges.
    // norY E [0,1]: lower values -> wispy, higher values -> billowy (cumulus-like).
    hfbm = lerp(1.0 - hfbm, hfbm, smoothstep(0.0, 0.5, norY));

    // Dynamic threshold driven by detail strength.
    // Any density value below `hfbm*strength` is clamped to zero.
    float threshold = hfbm * cloudsData.detailErosionStrength;
    float eroded = remap(coverage, threshold, 1.0, 0.0, 1.0);

    return saturate(eroded);
}

ShadowResult VolumetricShadow(float3 from, float2 pixelPos,
                              Texture3D cloudsTexture, Texture3D cloudsDetailTexture,
                              ConstantBuffer<interop::CloudsData> cloudsData)
{
    // Distance to layer's outer surface along sun direction.
    float2 hit = RaySphereIntersection(
        from, cloudsData.toSunDirection, float3(0.0, 0.0, 0.0), cloudsData.earthRadius + cloudsData.cloudLayerMax);
    float tMax = max(0.0, hit.y);

    // EARTH OCCLUSION: if the sun is below the horizon, the shadow ray
    // would hit the Earth. Clamp tMax so the march stops at the Earth surface.
    float2 earthHit = RaySphereIntersection(from, cloudsData.toSunDirection, float3(0.0, 0.0, 0.0), cloudsData.earthRadius);
    if(earthHit.x > 0.0)
        tMax = min(tMax, earthHit.x);

    if (tMax < 0.5)
    {
        ShadowResult r;
        r.lightEnergy = 1.0;
        r.opticalDepth = 0.0;
        return r;
    }

    float2 frameOffset = float2(
        frac(Constants.frameCounter * 0.61803398875), // golden ratio
        frac(Constants.frameCounter * 0.41421356237)  // silver ratio (sqrt(2)-1)
    );

    float step = tMax / (float)cloudsData.lightSteps;
    float opticalDepth = 0.0;
    for (int j = 0; j < cloudsData.lightSteps; ++j)
    {
        float stepJ = StepJitter(pixelPos + float2(7.0, 13.0) + frameOffset, (uint)j, 0.37);
        float sampleD = (float)j * step + stepJ * step;
        float3 pos = from + cloudsData.toSunDirection * sampleD;
        float alt = length(pos) - cloudsData.earthRadius;
        float norY = (alt - cloudsData.cloudLayerMin) * cloudsData.invCloudLayerThickness;
        if (norY > 1.0 || norY < 0.0)
            continue;

        float density = SampleCloudDensity(pos, norY, cloudsTexture, cloudsDetailTexture, cloudsData);
        opticalDepth += density * step;
    }

    ShadowResult r;
    r.lightEnergy = exp(-opticalDepth * cloudsData.muT);
    r.opticalDepth = opticalDepth;   // weighted OD from the sun
    return r;
}

bool GetCloudsLayerIntersectionPoints(
    float3 rayOrigin,
    float3 rayDir,
    float earthRadius,          // Solid Earth radius
    float innerSphereRadius,    // Cloud base altitude (earthRadius + cloudMinHeight)
    float outerSphereRadius,    // Cloud top altitude (earthRadius + cloudMaxHeight)
    float tMax,                 // Additional solid occluder (e.g., depth buffer for mountains/buildings)
    out float tEntry,
    out float tExit)
{
    tEntry = 0.0;
    tExit = 0.0;

    // Intersections with cloud layer boundaries
    float2 innerHit = RaySphereIntersection(rayOrigin, rayDir, float3(0, 0, 0), innerSphereRadius);
    float2 outerHit = RaySphereIntersection(rayOrigin, rayDir, float3(0, 0, 0), outerSphereRadius);

    // If the outer sphere is entirely behind the origin or misses it, no clouds ahead
    if (outerHit.y < 0.0) 
        return false;

    // Entry: start at outerHit.x or 0 if inside, or innerHit.y if inside inner sphere
    tEntry = max(max(outerHit.x, 0.0), (innerHit.x < 0.0 && innerHit.y > 0.0) ? innerHit.y : 0.0);

    // Exit: always go to outer boundary, let density be zero inside inner sphere
    tExit = outerHit.y;

    // Solid Earth occlusion
    float2 earthHit = RaySphereIntersection(rayOrigin, rayDir, float3(0, 0, 0), earthRadius);
    if (length(rayOrigin) >= earthRadius)
    {
        if (earthHit.x > 0.0)
            tExit = min(tExit, earthHit.x);
    }
    else
    {
        if (earthHit.y > 0.0)
            tEntry = max(tEntry, earthHit.y);
        else
            return false;
    }

    // Solid geometry occlusion
    tExit = min(tExit, tMax);

    if (tEntry >= tExit)
        return false;

    return true;
}

// rayOriginLocal: camera position in world space
// rayDir:         normalized ray direction in world space
CloudResult GetCloudsColorRayMarch(float3 rayOriginLocal, float3 rayDir, Texture3D cloudsTexture, Texture3D cloudsDetailTexture,
    ConstantBuffer<interop::CloudsData> cloudsData, float sceneDist, float2 pixelPos)
{
    CloudResult result;
    result.color = 0.0;
    result.transmittance = 1.0;
    result.weightedDistance = 0.0;
    result.tEntry = 0.0;
    result.tExit = 0.0;
    
    // Translate ray origin to Earth-centered coordinates.
    float3 rayOrigin = rayOriginLocal - cloudsData.earthCenter;
    
    float3 hitWorld = Constants.cameraPosition + rayDir * sceneDist;
    float3 hitEarth = hitWorld - cloudsData.earthCenter;
    float sceneDistEarth = length(hitEarth - rayOrigin);
    
    float tEntry, tExit;
    if (!GetCloudsLayerIntersectionPoints(rayOrigin, rayDir, cloudsData.earthRadius, cloudsData.earthRadius + cloudsData.cloudLayerMin,
        cloudsData.earthRadius + cloudsData.cloudLayerMax, sceneDistEarth, tEntry, tExit))
    {
        return result;
    }
    float marchExit = min(tExit, tEntry + cloudsData.cloudFadeDistance);
    float rayLength = marchExit - tEntry;

    // Resolution relative to freq of erosion detail.
    float detailSpatialFreq = cloudsData.cloudsScale * cloudsData.detailScale * DETAIL_LOW_FREQ;
    float targetStepSize = (detailSpatialFreq > 0.0001) ? 0.5 / detailSpatialFreq : 1000.0;
        
    // Dynamic steps, cap by maxSteps
    uint effectiveSteps = (uint)ceil(rayLength / max(targetStepSize, 0.001));
    effectiveSteps = clamp(effectiveSteps, cloudsData.maxSteps / 2, cloudsData.maxSteps);

    float stepSize = rayLength / max(effectiveSteps, 1u);
    float t = tEntry;                
    float cosTheta = dot(rayDir, cloudsData.toSunDirection);
    float phase = DualLobeHG(cosTheta, cloudsData.phaseGForward, cloudsData.phaseGBackward);

    float2 frameOffset = float2(
        frac(Constants.frameCounter * 0.61803398875), // golden ratio
        frac(Constants.frameCounter * 0.41421356237) // silver ratio (sqrt(2)-1)
    );
    float totalDensity = 0.0;
    
    for (uint step = 0; step < effectiveSteps; ++step)
    {
        float stepJ = StepJitter(pixelPos + frameOffset, step, 0.0);
        float sampleT = t + stepJ * stepSize;
        
        float3 pos = rayOrigin + rayDir * sampleT;
        float altitude = length(pos) - cloudsData.earthRadius;
        float norY = (altitude - cloudsData.cloudLayerMin) * cloudsData.invCloudLayerThickness;
        float density = SampleCloudDensity(pos, norY, cloudsTexture, cloudsDetailTexture, cloudsData);
        
        float normT = sampleT * cloudsData.invCloudFadeDistance;
        float fadeFactor = saturate(1.0 - square(normT));
        density *= fadeFactor;
                
        if (density > 0.0)
        {
            float lightEnergy = 1.0;
            float sunOD = 0.0;
            if(cloudsData.volumetricShadows)
            {
                ShadowResult shadowResult = VolumetricShadow(pos, pixelPos, cloudsTexture, cloudsDetailTexture, cloudsData);
                lightEnergy = shadowResult.lightEnergy;
                sunOD = shadowResult.opticalDepth;
            }

            // Single scatter: HG phase + powder + Beer-Lambert on sun path.
            float powder = PowderEffect(density, cloudsData.powderEdgeWidth, cloudsData.powderStrength);
            float3 S_direct = cloudsData.sunRadiance * cloudsData.muS * lightEnergy * phase * powder;

            // Multi-scatter
            const float energyLostToScatter = 1.0 - exp(-sunOD * cloudsData.muT);
            const float tau_sun = sunOD * cloudsData.muT;
            const int numOctaves = (int)cloudsData.multiScatterOctaves;
            const float msIntensity = 1.0;
            
            float currentEnergy = energyLostToScatter * cloudsData.albedo;
            float3 S_ms_accum = 0.0;
            
            for (int octave = 0; octave < numOctaves; octave++)
            {
                // Occlusion: Light is attenuated in each rebound
                float occlusionFactor = exp(-tau_sun * cloudsData.multiScatterOcclusion * (octave + 1));
                float octaveEnergy = currentEnergy * occlusionFactor;
                
                // Octave's phase
                float g_octave = cloudsData.multiScatterBaseG * pow(cloudsData.multiScatterEccentricity, octave);
                //    La fase isotrópica es 1/(4π) ≈ 0.0796
                float phase_octave = HenyeyGreenstein(cosTheta, g_octave);
                
                // Adds octave contribution
                S_ms_accum += octaveEnergy * cloudsData.multiScatterContribution * phase_octave;
                
                // Next octave
                currentEnergy = octaveEnergy * cloudsData.albedo;
            }            
            float3 S_ms = cloudsData.sunRadiance * cloudsData.muS * S_ms_accum * msIntensity;
                        
            float segmentTransmittance = exp(-density * stepSize * cloudsData.muT);            
            float integrationWeight = cloudsData.muT > 0.0001 ? 
                (1.0 - segmentTransmittance) / cloudsData.muT : density * stepSize;

            result.color += result.transmittance * (S_direct + S_ms) * integrationWeight;
            result.transmittance *= segmentTransmittance;
            result.weightedDistance += sampleT * density;

            totalDensity += density;
            
            if (result.transmittance < 0.001)
                break;
        }
        
        t += stepSize;
    }    
        
    float3 ambientUp = cloudsData.ambientStrength * float3(0.6, 0.7, 0.9); // cool zenith
    float3 ambientDown = cloudsData.ambientStrength * float3(0.3, 0.25, 0.2); // warm ground bounce
    float skyWeight = saturate(-rayDir.y * 0.5 + 0.5); // 0=looking down, 1=looking up
    float3 ambient = lerp(ambientDown, ambientUp, skyWeight);
    
    result.color += ambient * (1.0 - result.transmittance) * cloudsData.albedo;
    
    result.weightedDistance /= max(totalDensity, 0.0001f);
    result.tEntry = tEntry;
    result.tExit = marchExit;

    return result;
}

[RootSignature(BindlessRootSignature)]
float4 main(PS_INPUT input) : SV_Target
{
    ConstantBuffer<interop::CloudsData> cloudsData = ResourceDescriptorHeap[Constants.cloudsDataDI];    
    Texture3D<float4> baseTexture = ResourceDescriptorHeap[cloudsData.cloudsBaseShapeTexture];
    Texture3D<float4> detailTexture = ResourceDescriptorHeap[cloudsData.cloudsDetailTexture];
    Texture2D<float> linearDepthTex = ResourceDescriptorHeap[cloudsData.linearDepthTexDI];
    
    // Reconstruct world-space ray direction from clip-space coordinates.
    // matClipToTranslatedWorld transforms from clip space to world space
    // with the camera at the origin (translated world), so no camera translation
    // is baked into the matrix -- avoids floating point precision issues at large distances.
    float4 clipPos;
    clipPos.x = input.uv.x * 2.0 - 1.0;
    clipPos.y = 1.0 - input.uv.y * 2.0; // flip Y: UV origin is top-left, clip space origin is bottom-left
    clipPos.z = 1.0;
    clipPos.w = 1.0;

    float4 rayDirH = mul(Constants.matClipToTranslatedWorld, clipPos);
    float3 rayDir = normalize(rayDirH.xyz);

    // Read linear depth (distance along the camera forward axis) and convert to scene distance along the ray direction.
    // viewZ: depth along camera forward vector (Z component in view space)
    // sceneDist: actual distance along rayDir to the geometry
    float viewZ = linearDepthTex.SampleLevel(pointClampSampler, input.uv, 0.0);
    float cosAngle = saturate(dot(rayDir, cloudsData.cameraForward));
    float sceneDist = viewZ / max(cosAngle, 0.0001);
    
    float2 pixelPos = input.uv * Constants.viewportSize;
    
    CloudResult clouds = GetCloudsColorRayMarch(
        Constants.cameraPosition, rayDir, baseTexture, detailTexture, cloudsData, sceneDist, pixelPos);
    
    if (Constants.debugChannel == DebugChannel_Clouds_Transmitance)
    {
        return float4(clouds.transmittance.xxxx);
    }
        
    // Punto estable: centro del segmento atravesado por el rayo dentro de la capa.
    // Es EXACTO para este rayo (no depende del muestreo) y NO SALTA con jitter.
    float hit_t = 0.5f * (clouds.tEntry + clouds.tExit);
    float blendToMass = saturate((1.0f - clouds.transmittance) * (1.0f - clouds.transmittance));
    hit_t = lerp(hit_t, clouds.weightedDistance, blendToMass);
    
    float3 hitPosWorld = Constants.cameraPosition + rayDir * hit_t;
    
    // hit in prev frame
    float4 prevClipPos = mul(cloudsData.matPrevFrameViewProj, float4(hitPosWorld, 1.0));
    prevClipPos.xyz /= prevClipPos.w;
    // Convert NDC to uv
    float2 prevUv = prevClipPos.xy * float2(0.5, -0.5) + 0.5;
    float prevViewZ = linearDepthTex.SampleLevel(pointClampSampler, prevUv, 0.0);
    float depthDiff = abs(prevViewZ - viewZ) / max(viewZ, 1.0);
    bool prevDepthValid = depthDiff < cloudsData.depthThreshold;
    
    bool prevUvValid = prevDepthValid 
                       && all(prevUv >= 0.0) && all(prevUv <= 1.0)
                       && prevClipPos.z >= 0.0 && prevClipPos.z <= 1.0;
    
    float4 currentColor = float4(clouds.color, clouds.transmittance);
    float4 finalColor = currentColor;
    
    if (prevUvValid)
    {        
        Texture2D<float4> prevCloudsTex = ResourceDescriptorHeap[cloudsData.prevCloudsTexDI];
        float4 prevColor = prevCloudsTex.SampleLevel(pointClampSampler, prevUv, 0.0);        
        
        bool historyHadCloud = prevColor.a < 0.999;
        bool hasHit = clouds.transmittance < 0.999;
        
        if (hasHit && historyHadCloud)
            finalColor = lerp(prevColor, currentColor, cloudsData.blendFactor);
        else if (hasHit)
            finalColor = currentColor;
        else
            finalColor = lerp(prevColor, float4(0, 0, 0, 1), cloudsData.blendFactor);
    }
    
    return finalColor;
}