// Copyright 2024-26 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
// Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
// SPDX-License-Identifier: Apache-2.0
#version 450
#include "pbr_types.glsl"
#include "tonemaps.glsl"
#define PI 3.14159265359
layout(set=0,binding=1) uniform sampler2D baseColorMap;
layout(set=0,binding=2) uniform sampler2D metallicRoughnessMap;
layout(set=0,binding=3) uniform sampler2D normalMap;
layout(set=0,binding=4) uniform sampler2D emissiveMap;
layout(set=0,binding=5) uniform sampler2D occlusionMap;
layout(location=0) in vec3 inWorldPos;
layout(location=1) in vec2 inUV;
layout(location=2) in vec3 inNormal;
layout(location=3) in vec3 inTangent;
layout(location=4) in vec3 inBitangent;
layout(location=5) in vec3 inViewDirection;
layout(location=6) in vec3 inColor;
layout(location=7) in vec2 inUV1;
layout(location=0) out vec4 outColor;

vec3 safeNormalize(vec3 value) { return value * inversesqrt(max(dot(value,value),1e-20)); }
vec2 textureUV(uint flag) { return (material.textureFlags & (flag << 8)) != 0u ? inUV1 : inUV; }
vec3 srgbToLinear(vec3 value) {
    return mix(pow((value+0.055)/1.055,vec3(2.4)),value/12.92,lessThanEqual(value,vec3(0.04045)));
}
vec3 fresnelSchlick(float cosine, vec3 F0) {
    float x = 1.0-clamp(cosine,0.0,1.0);
    float x2 = x*x;
    return F0 + (1.0-F0)*(x2*x2*x);
}

// glTF roughness is perceptual: alpha = roughness^2, GGX uses alpha^2
float distributionGGX(float NoH, float alphaSquared) {
    float d = max((NoH*alphaSquared-NoH)*NoH+1.0,1e-7);
    return alphaSquared/(PI*d*d);
}

// Height-correlated Smith visibility, including the 1/(4 NoV NoL) term
float visibilitySmith(float NoV, float NoL, float alphaSquared) {
    float v = NoL*sqrt(NoV*NoV*(1.0-alphaSquared)+alphaSquared);
    float l = NoV*sqrt(NoL*NoL*(1.0-alphaSquared)+alphaSquared);
    return 0.5/max(v+l,1e-7);
}
vec3 directLight(vec3 N, vec3 V, float NoV, vec3 L, vec3 radiance,
                 vec3 baseColor, vec3 F0, float metallic, float alphaSquared, uint mode) {
    float NoL = max(dot(N,L),0.0);
    if (NoL <= 0.0 || NoV <= 0.0) return vec3(0);
    vec3 H = safeNormalize(V+L);
    if (mode == 1u) {
        float specular = pow(max(dot(N,H),0.0),32.0);
        return (baseColor*NoL+vec3(specular*0.5))*radiance;
    }
    vec3 F = fresnelSchlick(max(dot(V,H),0.0),F0);
    float D = distributionGGX(max(dot(N,H),0.0),alphaSquared);
    float visibility = visibilitySmith(NoV,NoL,alphaSquared);
    vec3 diffuse = (1.0-F)*(1.0-metallic)*baseColor/PI;
    return (diffuse+D*visibility*F)*radiance*NoL;
}
float distanceAttenuation(float distanceSquared, float range) {
    float attenuation = 1.0/max(distanceSquared,1e-4);
    if (range > 0.0) {
        float ratio = distanceSquared/(range*range);
        float cutoff = clamp(1.0-ratio*ratio,0.0,1.0);
        attenuation *= cutoff*cutoff;
    }
    return attenuation;
}
vec3 surfaceNormal(vec3 V) {
    vec3 N = safeNormalize(inNormal);
    if (dot(N,N) < 0.5) {
        N = safeNormalize(cross(dFdx(inWorldPos),dFdy(inWorldPos)));
        if (dot(N,V)<0.0) N = -N;
    } else if (!gl_FrontFacing) N = -N;
    if ((material.textureFlags & TEXTURE_NORMAL_BIT) == 0u) return N;
    vec2 uv = textureUV(TEXTURE_NORMAL_BIT);
    vec3 sampled = texture(normalMap,uv).xyz*2.0-1.0;
    sampled.xy *= material.normalScale;
    vec3 T = inTangent - N*dot(N,inTangent);
    vec3 B = inBitangent;

    // Missing tangents and UV1 normal maps need a frame from the selected UVs
    if (dot(T,T)<1e-10 || dot(B,B)<1e-10 || (material.textureFlags & (TEXTURE_NORMAL_BIT << 8)) != 0u) {
        vec3 p1=dFdx(inWorldPos), p2=dFdy(inWorldPos);
        vec2 t1=dFdx(uv), t2=dFdy(uv);
        float determinant=t1.x*t2.y-t1.y*t2.x;
        if (abs(determinant)<1e-10) return N;
        T=(p1*t2.y-p2*t1.y)/determinant;
        B=(p2*t1.x-p1*t2.x)/determinant;
        T-=N*dot(N,T);
    }
    T=safeNormalize(T);
    B=safeNormalize(cross(N,T))*(dot(cross(N,T),B)<0.0 ? -1.0 : 1.0);
    return safeNormalize(mat3(T,B,N)*sampled);
}
vec3 displayColor(vec3 linearColor) {
    uint op = scene.tonemapping.tonemap & 15u;
    vec3 color=max(linearColor,vec3(0))*max(scene.tonemapping.exposure,0.0);
    TonemapParams params=TonemapParams(1.0,scene.tonemapping.gamma);
    if (op==1u) color=tonemapReinhard(color,params);
    else if (op==2u) color=tonemapACES(color,params);
    else if (op==3u) color=tonemapKHRNeutral(color,params);
    else if (op==4u) color=tonemapUncharted2(color,params);
    float luminance=dot(color,vec3(0.2126,0.7152,0.0722));
    color=mix(vec3(luminance),color,max(scene.tonemapping.saturation,0.0));
    color=max((color-0.18)*max(scene.tonemapping.contrast,0.0)+0.18,vec3(0));

    // SRGB render targets encode once in hardware. Gamma remains available for UNORM targets
    return scene.outputSRGB!=0u ? color : gammaCorrect(color,scene.tonemapping.gamma);
}
void main() {
    uint flags=material.textureFlags;
    uint mode=(scene.tonemapping.tonemap >> 4)&15u;
    uint alphaMode=(flags >> 24)&3u;
    vec4 base=material.baseColorFactor*vec4(inColor,1);
    if ((flags & TEXTURE_BASE_COLOR_BIT)!=0u) {
        vec4 sampled=texture(baseColorMap,textureUV(TEXTURE_BASE_COLOR_BIT));
        if ((flags & TEXTURE_BASE_COLOR_SRGB_BIT)==0u) sampled.rgb=srgbToLinear(sampled.rgb);
        base*=sampled;
    }
    if (alphaMode==1u && base.a<material.alphaCutoff) discard;
    if (alphaMode!=2u) base.a=1.0;
    if (mode==0u) { outColor=vec4(displayColor(base.rgb),base.a); return; }
    vec3 V=safeNormalize(inViewDirection);
    vec3 N=surfaceNormal(V);
    float NoV=max(dot(N,V),0.0);
    float metallic=material.metallicFactor;
    float roughness=material.roughnessFactor;
    float ao=1.0;
    if (mode==2u) {
        if ((flags & TEXTURE_METALLIC_ROUGH_BIT)!=0u) {
            vec4 mr=texture(metallicRoughnessMap,textureUV(TEXTURE_METALLIC_ROUGH_BIT));
            metallic*=mr.b; roughness*=mr.g;
        }
        if ((flags & TEXTURE_OCCLUSION_BIT)!=0u)
            ao=mix(1.0,texture(occlusionMap,textureUV(TEXTURE_OCCLUSION_BIT)).r,clamp(material.occlusionStrength,0.0,1.0));
    } else metallic=0.0;
    metallic=clamp(metallic,0.0,1.0);
    roughness=clamp(roughness,0.045,1.0);
    float alpha=roughness*roughness;
    float alphaSquared=alpha*alpha;
    vec3 F0=mix(vec3(0.04),base.rgb,metallic);
    vec3 color=directLight(N,V,NoV,safeNormalize(-scene.mainLight.direction),scene.mainLight.color*scene.mainLight.intensity,base.rgb,F0,metallic,alphaSquared,mode);
    for (uint i=0u;i<min(scene.activePointLights,uint(MAX_POINT_LIGHTS));++i) {
        vec3 delta=scene.pointLights[i].position-inWorldPos;
        float d2=dot(delta,delta);
        vec3 radiance=scene.pointLights[i].color*scene.pointLights[i].intensity*distanceAttenuation(d2,scene.pointLights[i].range);
        color+=directLight(N,V,NoV,safeNormalize(delta),radiance,base.rgb,F0,metallic,alphaSquared,mode);
    }
    for (uint i=0u;i<min(scene.activeSpotLights,uint(MAX_SPOTLIGHTS));++i) {
        vec3 delta=scene.spotLights[i].position-inWorldPos;
        float d2=dot(delta,delta);
        vec3 L=safeNormalize(delta);
        float cosine=dot(L,safeNormalize(-scene.spotLights[i].direction));
        float inner=scene.spotLights[i].innerCone, outer=scene.spotLights[i].outerCone;
        float cone=inner>outer ? clamp((cosine-outer)/(inner-outer),0.0,1.0) : step(outer,cosine);
        if (cone<=0.0) continue;
        vec3 radiance=scene.spotLights[i].color*scene.spotLights[i].intensity*cone*cone*distanceAttenuation(d2,scene.spotLights[i].range);
        color+=directLight(N,V,NoV,L,radiance,base.rgb,F0,metallic,alphaSquared,mode);
    }

    // Ambient is diffuse irradiance. Real specular IBL needs an environment map;
    // a made-up sky gradient is not a reflection of the user's surroundings
    vec3 diffuseWeight=mode==2u ? (1.0-fresnelSchlick(NoV,F0))*(1.0-metallic) : vec3(1);
    color+=diffuseWeight*base.rgb*scene.ambientColor*scene.ambientIntensity*ao/PI;
    vec3 emissive=material.emissiveFactor.rgb;
    if ((flags & TEXTURE_EMISSIVE_BIT)!=0u) {
        vec3 sampled=texture(emissiveMap,textureUV(TEXTURE_EMISSIVE_BIT)).rgb;
        if ((flags & TEXTURE_EMISSIVE_SRGB_BIT)==0u) sampled=srgbToLinear(sampled);
        emissive*=sampled;
    }
    outColor=vec4(displayColor(color+emissive),base.a);
}
