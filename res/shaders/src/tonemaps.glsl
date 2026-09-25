// Copyright 2024-25 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0

#ifndef TONEMAP_INCLUDE
#define TONEMAP_INCLUDE

struct TonemapParams
{
    float exposure;
    float gamma;
};

// Reinhard tonemapping operator based from "Photographic Tone Reproduction for Digital Images" 
// Reinhard et al., Siggraph 2002 (https://dl.acm.org/doi/10.1145/566654.566575)
// Public domain - Reinhard et al
vec3 tonemapReinhard(vec3 color, TonemapParams params)
{
    color *= params.exposure;
    return color / (vec3(1.0) + color);
}

// Approximate ACES filmic response curve
// Academy Color Encoding System (https://www.oscars.org/science-technology/sci-tech-projects/aces)
// Licensed under OpenEXR (https://openexr.com/en/latest/license.html)
vec3 tonemapACES(vec3 color, TonemapParams params)
{
    color *= params.exposure;
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

// KHR Neutral tonemapping operator
// Copyright 2024 The Khronos Group
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
vec3 tonemapKHRNeutral(vec3 color, TonemapParams params)
{
    color *= params.exposure;
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;
    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;
    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression)
    {
        return color;
    }
    const float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;
    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(color, newPeak * vec3(1.0), g);
}

// Uncharted 2 tonemapping operator
// Public domain - John Hable (http://filmicworlds.com/blog/filmic-tonemapping-operators/)
vec3 tonemapUncharted2(vec3 color, TonemapParams params)
{
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    const float W = 11.2;
    const float exposureBias = 2.0;

    color *= params.exposure * exposureBias;

    vec3 curr = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E/F;
    vec3 whiteScale = vec3(1.0) / ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F) - E/F);
    
    return curr * whiteScale;
}

vec3 gammaCorrect(vec3 color, float gamma)
{
    return pow(max(color, vec3(0.0)), vec3(1.0 / max(gamma, 0.001)));
}

#endif // TONEMAP_INCLUDE