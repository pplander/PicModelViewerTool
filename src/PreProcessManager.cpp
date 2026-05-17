#include "PreProcessManager.h"
#include <osg/Shader>
#include <osg/BlendFunc>
#include <osg/Texture>
#include <osg/NodeVisitor>
#include <osg/Geode>
#include <osg/Geometry>
#include <map>

namespace {

static constexpr int kEffectCount = static_cast<int>(PreProcessManager::EffectCount);

// --------------- Vertex Shader ---------------
static const char* kVertexShader = R"GLSL(
#version 330 compatibility

uniform float uTime;
uniform int   uEffectMask0;
uniform int   uEffectMask1;
uniform float uParam0[64];
uniform float uParam1[64];

varying vec3 vNormal;
varying vec3 vPos;
varying vec3 vObjPos;
varying vec2 vUv;
varying vec4 vColor;

uniform float uSceneScale; // model bounding-sphere radius (>=1.0); used to scale animation amplitude
uniform vec3  uPartCenter;  // center of the current sub-part (per-StateSet)
uniform vec3  uSceneCenter; // overall scene center (model-space)

float sat(float x){ return clamp(x,0.0,1.0); }

void main() {
    vec4 pos = gl_Vertex;

    // E22 VertexWave: 顶点波动
    if ((uEffectMask0 & (1 << 22)) != 0) {
        float freq = mix(1.2, 7.5, sat(uParam0[22]));
        float amp  = mix(0.05, 0.40, sat(uParam1[22])) * uSceneScale;
        pos.z += amp * sin(freq * pos.x / max(uSceneScale, 0.001) + uTime * 1.25);
        pos.z += amp * sin(freq * pos.y / max(uSceneScale, 0.001) + uTime * 1.55);
    }
    // E23 FlagWave: 旗帜飘动
    if ((uEffectMask0 & (1 << 23)) != 0) {
        float speed = mix(0.25, 3.2, sat(uParam0[23]));
        float dirPhase = mix(0.0, 6.2831853, sat(uParam1[23]));
        vec2 windDir = normalize(vec2(cos(dirPhase), sin(dirPhase)) + vec2(1e-4, 0.0));
        float waveCoord = dot(pos.xy, windDir * vec2(3.0, 1.5)) / max(uSceneScale, 0.001);
        float amp = 0.20 * uSceneScale;
        pos.z += amp * sin(waveCoord + uTime * speed) * gl_MultiTexCoord0.x;
        pos.z += amp * 0.35 * sin(waveCoord * 1.8 + uTime * speed * 1.4) * gl_MultiTexCoord0.x;
    }
    // E24 WaterRipple: 水面波纹
    if ((uEffectMask0 & (1 << 24)) != 0) {
        float spd = mix(0.5, 5.0, sat(uParam0[24]));
        float ht  = mix(0.03, 0.25, sat(uParam1[24])) * uSceneScale;
        float r   = length(pos.xz) / max(uSceneScale, 0.001);
        pos.y += ht * sin(r * 6.0 - uTime * spd);
        pos.y += ht * 0.45 * sin(r * 10.0 - uTime * spd * 1.25 + 1.0);
    }
    // E27 ElectricArc: 电弧闪烁 - 顶点抖动
    if ((uEffectMask0 & (1 << 27)) != 0) {
        float intensity = mix(0.015, 0.08, sat(uParam1[27])) * uSceneScale;
        float invS = 1.0 / max(uSceneScale, 0.001);
        pos.xyz += intensity * vec3(
            sin(uTime * 37.0 + pos.x * 11.0 * invS),
            sin(uTime * 41.0 + pos.y * 13.0 * invS),
            sin(uTime * 43.0 + pos.z * 17.0 * invS)
        );
    }
    // E28 BurnEffect: 燃烧 - 底部顶点扰动
    if ((uEffectMask0 & (1 << 28)) != 0) {
        float progress = sat(uParam0[28]);
        float burnY = mix(-2.0, 4.0, progress) * uSceneScale;
        if (pos.y < burnY) {
            float factor = sat((burnY - pos.y) * 2.0 / max(uSceneScale, 0.001));
            float invS = 1.0 / max(uSceneScale, 0.001);
            pos.xz += factor * 0.05 * uSceneScale * vec2(sin(uTime * 8.0 + pos.y * 5.0 * invS), cos(uTime * 7.0 + pos.y * 3.0 * invS));
        }
    }
    // E29 GlitchEffect: 故障 - 偶发顶点偏移
    if ((uEffectMask0 & (1 << 29)) != 0) {
        float intensity = mix(0.0, 0.20, sat(uParam0[29])) * uSceneScale;
        float trigger = step(0.92, fract(sin(uTime * mix(2.0, 15.0, sat(uParam1[29]))) * 43758.5453));
        pos.x += trigger * intensity * sin(uTime * 50.0 + pos.y * 20.0 / max(uSceneScale, 0.001));
    }

    // E36 Explode: 模型炸开 (可逆: progress=0 时位移=0)
    // 按子物体整体飞散: 同一 binding(StateSet) 内所有顶点共享同一 dir、dist
    // mask1 bit 4 (36 - 32)
    if ((uEffectMask1 & (1 << 4)) != 0) {
        float progress = sat(uParam0[36]);
        if (progress > 0.0) {
            // mode: 0 = center-out (radial), 1 = random per-part
            float mode = uParam1[36];
            // radial direction: from scene center to part center
            vec3 radial = uPartCenter - uSceneCenter;
            float rl = length(radial);
            vec3 dirRadial = (rl > 1e-4) ? (radial / rl) : vec3(0.0, 1.0, 0.0);
            vec3 dir;
            if (mode < 0.5) {
                dir = dirRadial;
            } else {
                // pseudo-random direction stable per-part (seeded by part center)
                float seed = sin(dot(uPartCenter + vec3(7.13, 3.71, 5.91), vec3(12.9898, 78.233, 37.719))) * 43758.5453;
                vec3 dirRand = vec3(
                    fract(seed)        * 2.0 - 1.0,
                    fract(seed * 1.7)  * 2.0 - 1.0,
                    fract(seed * 2.3)  * 2.0 - 1.0
                );
                float rndL = length(dirRand);
                dir = (rndL > 1e-4) ? (dirRand / rndL) : dirRadial;
            }
            float dist = progress * uSceneScale * 1.6;
            pos.xyz += dir * dist;
        }
    }

    vObjPos = pos.xyz;
    vNormal = normalize(gl_NormalMatrix * gl_Normal);
    vPos    = vec3(gl_ModelViewMatrix * pos);
    vUv     = gl_MultiTexCoord0.xy;
    vColor  = gl_Color;

    gl_Position = gl_ModelViewProjectionMatrix * pos;
}
)GLSL";

// --------------- Fragment Shader ---------------
static const char* kFragmentShader = R"GLSL(
#version 330 compatibility

uniform float uTime;
uniform int   uEffectMask0;
uniform int   uEffectMask1;
uniform float uParam0[64];
uniform float uParam1[64];
uniform vec3  uColor[64];
uniform sampler2D uBaseTex;
uniform bool uUseBaseTex;

varying vec3 vNormal;
varying vec3 vPos;
varying vec3 vObjPos;
varying vec2 vUv;
varying vec4 vColor;

float sat(float x){ return clamp(x,0.0,1.0); }

float hash(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453123); }
float noise(vec2 p){
    vec2 i=floor(p), f=fract(p);
    float a=hash(i), b=hash(i+vec2(1,0)), c=hash(i+vec2(0,1)), d=hash(i+vec2(1,1));
    vec2 u=f*f*(3.0-2.0*f);
    return mix(mix(a,b,u.x),mix(c,d,u.x),u.y);
}
float fbm(vec2 p) {
    float s = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; ++i) {
        s += a * noise(p);
        p = p * 2.02 + vec2(13.1, 7.7);
        a *= 0.5;
    }
    return s;
}
float hexMask(vec2 p, float scale) {
    p *= scale;
    vec2 q = vec2(p.x * 1.154700538, p.y + 0.577350269 * floor(p.x));
    vec2 cell = fract(q) - 0.5;
    cell.x *= 1.154700538;
    float d = max(abs(cell.x) * 0.8660254 + cell.y * 0.5, abs(cell.y));
    return smoothstep(0.45, 0.12, d);
}

void main() {
    vec3 N   = normalize(vNormal);
    vec3 V   = normalize(-vPos);
    vec3 L   = normalize(vec3(-0.4,0.6,0.7));
    vec3 H   = normalize(L+V);
    float ndl = max(dot(N,L),0.0);
    float ndv = max(dot(N,V),0.0);

    vec4 texel = uUseBaseTex ? texture2D(uBaseTex, vUv) : vec4(1.0);
    vec3 base = mix(vec3(0.72) + 0.2 * vColor.rgb, texel.rgb * vColor.rgb, 0.85);
    vec3 color = base;
    float alpha = clamp(texel.a * vColor.a, 0.0, 1.0);

    // ====== 渲染风格 (E0~E9) ======

    // E0 PhongEnhanced
    if ((uEffectMask0 & (1<<0)) != 0) {
        float shin = mix(8.0,128.0,sat(uParam1[0]));
        float spec = pow(max(dot(reflect(-L,N),V),0.0),shin);
        color = base*(0.18+ndl) + vec3(spec)*(0.3+1.7*uParam0[0]);
    }
    // E1 CelShading
    if ((uEffectMask0 & (1<<1)) != 0) {
        float steps = mix(2.0,8.0,sat(uParam0[1]));
        float q = floor(ndl * steps) / max(steps - 1.0, 1.0);
        float width = mix(1.2, 6.0, sat(uParam1[1]));
        float rim = pow(1.0 - ndv, width);
        float ink = smoothstep(0.35, 0.75, rim);
        color = mix(base * (0.18 + q), vec3(0.02), ink);
    }
    // E2 HalfLambert
    if ((uEffectMask0 & (1<<2)) != 0) {
        float hl = ndl*0.5+0.5;
        color = base * hl * (0.5+uParam0[2]);
    }
    // E3 AnisotropicSpecular
    if ((uEffectMask0 & (1<<3)) != 0) {
        vec3 T0 = normalize(abs(N.z) < 0.9 ? cross(N, vec3(0,0,1)) : cross(N, vec3(0,1,0)));
        vec3 B0 = normalize(cross(N, T0));
        float angle = mix(0.0, 6.2831853, sat(uParam0[3]));
        vec3 T = normalize(T0 * cos(angle) + B0 * sin(angle));
        float stripe = max(0.0, 1.0 - abs(dot(H, T)));
        float aniso = pow(stripe, 24.0) * mix(0.2, 2.5, sat(uParam1[3]));
        color = base*(0.14+ndl) + vec3(aniso)*vec3(0.9,0.8,0.7);
    }
    // E4 SubsurfaceScatter
    if ((uEffectMask0 & (1<<4)) != 0) {
        float th = mix(0.05,1.0,sat(uParam0[4]));
        float wrap = sat((dot(N, L) + th) / (1.0 + th));
        float back = pow(max(dot(-N, L), 0.0), 1.5) * th;
        vec3 sss = mix(base, uColor[4], sat(uParam1[4]));
        color = mix(base * (0.18 + wrap), sss * (0.25 + wrap + back), th);
    }
    // E5 RimLight
    if ((uEffectMask0 & (1<<5)) != 0) {
        float rim = pow(1.0-ndv, mix(1.0,8.0,sat(uParam1[5])));
        color += rim * uColor[5] * (0.5+uParam0[5]*2.0);
    }
    // E6 MatCap
    if ((uEffectMask0 & (1<<6)) != 0) {
        vec2 mcUV = N.xy * 0.5 + 0.5;
        vec3 matcapColor = vec3(mcUV.x * 0.8 + 0.2, mcUV.y * 0.7 + 0.3, 0.5 + 0.5 * dot(N, V));
        float blendAmt = sat(uParam0[6]);
        color = mix(color, matcapColor, blendAmt);
    }
    // E7 Metalness
    if ((uEffectMask0 & (1<<7)) != 0) {
        float metal = sat(uParam0[7]);
        float rough = mix(0.1, 0.9, sat(uParam1[7]));
        float spec = pow(max(dot(reflect(-L,N),V),0.0), mix(8.0,256.0,1.0-rough));
        vec3 metalColor = uColor[7];
        vec3 F0 = mix(vec3(0.04), metalColor, metal);
        float fresnel = pow(1.0-ndv, 5.0);
        vec3 specContribution = F0 + (1.0-F0)*fresnel;
        color = base*(0.08+ndl*(1.0-metal*0.8)) + specContribution*spec*ndl;
    }
    // E8 OutlineStroke
    if ((uEffectMask0 & (1<<8)) != 0) {
        float width = mix(1.5, 9.0, sat(uParam0[8]));
        float edge = pow(1.0 - ndv, width * 0.55);
        float rim = smoothstep(0.18, 0.78, edge);
        color = mix(color, uColor[8], rim);
    }
    // E9 WireframeOverlay
    if ((uEffectMask0 & (1<<9)) != 0) {
        float lineWidth = mix(1.0, 4.0, sat(uParam0[9]));
        float fillAlpha = mix(0.1, 0.6, sat(uParam1[9]));
        vec2 grid = abs(fract(vUv * 20.0) - 0.5);
        float line = min(grid.x, grid.y);
        float wire = 1.0 - smoothstep(0.0, 0.05 * lineWidth, line);
        color = mix(base * fillAlpha, uColor[9], wire);
    }

    // ====== 材质特效 (E10~E17) ======

    // E10 XRay
    if ((uEffectMask0 & (1<<10)) != 0) {
        float fres = pow(1.0-ndv,2.2);
        alpha = mix(0.35, alpha, 0.35) * mix(0.35, 0.9, fres * sat(uParam0[10]));
        color = mix(color,uColor[10],0.2 + 0.45 * fres * sat(uParam0[10]));
    }
    // E11 Hologram
    if ((uEffectMask0 & (1<<11)) != 0) {
        float speed = mix(1.0,8.0,sat(uParam0[11]));
        float density = mix(10.0,60.0,sat(uParam1[11]));
        float scan = step(0.5,fract(vUv.y*density+uTime*speed));
        float flicker = 0.75+0.25*sin(uTime*30.0+vPos.x*10.0);
        float fres = pow(1.0 - ndv, 2.5);
        vec3 holo = uColor[11] * (0.22 + 0.55 * fres + 0.18 * scan) * flicker;
        color = mix(base, holo, 0.55);
        alpha *= 0.55 + 0.25 * fres;
    }
    // E12 EnergyShield
    if ((uEffectMask0 & (1<<12)) != 0) {
        float speed = mix(0.6,8.0,sat(uParam0[12]));
        float hex = hexMask(vUv + vec2(uTime * 0.02, -uTime * 0.015), mix(6.0, 18.0, sat(uParam1[12])));
        float wave = 0.5 + 0.5 * sin(length(vPos.xy) * mix(5.0, 22.0, sat(uParam1[12])) - uTime * speed * 2.0);
        float fres = pow(1.0 - ndv, 2.8);
        float shimmer = fbm(vUv * 6.5 + uTime * 0.35);
        vec3 ripple = mix(uColor[12] * 0.55, uColor[12] * 1.35, wave);
        vec3 shield = ripple * (0.25 + 0.95 * fres + 0.18 * hex + 0.08 * shimmer);
        color = mix(base, shield, 0.35 + 0.45 * fres);
        alpha *= 0.55 + 0.2 * fres;
    }
    // E13 Dissolve
    if ((uEffectMask0 & (1<<13)) != 0) {
        float n = noise(vUv*20.0+uTime*0.1);
        float p = sat(uParam0[13]);
        if (n > p) discard;
        float edge = smoothstep(p-sat(uParam1[13])*0.15,p,n);
        color = mix(uColor[13],base,edge);
    }
    // E14 Frozen
    if ((uEffectMask0 & (1<<14)) != 0) {
        float strength = sat(uParam0[14]);
        vec3 iceColor = uColor[14];
        vec2 nOff = vec2(noise(vUv * 30.0 + uTime * 0.12), noise(vUv * 24.0 - uTime * 0.07)) - 0.5;
        vec3 pertN = normalize(N + vec3(nOff.x, nOff.y, 0.0) * strength * 0.6);
        float iceSpec = pow(max(dot(reflect(-L, pertN), V), 0.0), mix(18.0, 96.0, strength));
        float crack = noise(vUv * mix(8.0, 48.0, strength) + vec2(uTime * 0.08, -uTime * 0.05));
        float ridge = smoothstep(0.55, 0.88, crack);
        float frost = smoothstep(0.15, 0.85, noise(vUv * 18.0 + uTime * 0.15));
        vec3 frozen = mix(base, iceColor, 0.35 + 0.65 * strength);
        frozen = mix(frozen, vec3(0.95, 0.98, 1.0), ridge * 0.35);
        color = mix(color, frozen, strength);
        color += iceSpec * vec3(0.18, 0.22, 0.28);
        color += frost * 0.04;
    }
    // E15 Lava
    if ((uEffectMask0 & (1<<15)) != 0) {
        float flow = noise(vUv*8.0+vec2(uTime*mix(0.3,3.0,sat(uParam0[15])),uTime*0.2));
        vec3 lava = mix(vec3(0.08,0.02,0.01),uColor[15],flow);
        color = mix(base * 0.45, lava, 0.75);
        color += pow(ndl,2.0)*mix(0.2,1.0,sat(uParam1[15]));
    }
    // E16 Camouflage
    if ((uEffectMask0 & (1<<16)) != 0) {
        float scale = mix(2.0, 24.0, sat(uParam0[16]));
        float blendAmt = sat(uParam1[16]);
        vec3 c1 = clamp(uColor[16] * vec3(0.55, 0.85, 0.55), 0.0, 1.0);
        vec3 c2 = clamp(uColor[16] * vec3(1.05, 0.95, 0.65) + vec3(0.08, 0.06, 0.02), 0.0, 1.0);
        vec3 c3 = clamp(uColor[16] * vec3(0.65, 0.75, 0.45) + vec3(0.04, 0.02, 0.0), 0.0, 1.0);
        float n1 = noise(vUv * scale + vec2(0.0, uTime * 0.03));
        float n2 = noise(vUv * scale * 1.7 + vec2(uTime * 0.02, 7.3));
        float n3 = noise(vUv * scale * 2.4 + vec2(-uTime * 0.015, 13.1));
        vec3 camo = c1 * n1 + c2 * n2 + c3 * n3;
        color = mix(base, camo, 0.45 + 0.55 * blendAmt);
    }
    // E17 CrystalGlass
    if ((uEffectMask0 & (1<<17)) != 0) {
        float transparency = mix(0.2, 0.8, sat(uParam0[17]));
        float refraction = mix(0.0, 1.0, sat(uParam1[17]));
        float fres = pow(1.0 - ndv, 3.0);
        vec3 refractedColor = uColor[17] * (0.6 + 0.4 * sin(vObjPos.x * 3.0 + vObjPos.y * 5.0 + vObjPos.z * 4.0));
        float chromatic = sin(vUv.x * 20.0 + vUv.y * 15.0) * refraction * 0.15;
        refractedColor += vec3(chromatic, -chromatic * 0.5, chromatic * 0.3);
        color = mix(base, refractedColor, fres * 0.7 + transparency * 0.3);
        alpha = mix(1.0, 0.3 + 0.5 * fres, transparency);
        color += vec3(0.15) * pow(max(dot(reflect(-L, N), V), 0.0), 32.0) * fres;
    }

    // ====== 环境氛围 (E18~E21) ======

    // E18 DistanceFog
    if ((uEffectMask0 & (1<<18)) != 0) {
        float start = mix(5.0,120.0,sat(uParam0[18]));
        float end   = start + mix(10.0,200.0,sat(uParam1[18]));
        float fog   = sat((length(vPos)-start)/(end-start));
        color = mix(color,uColor[18],fog);
    }
    // E19 HeightFog
    if ((uEffectMask0 & (1<<19)) != 0) {
        float h    = mix(-10.0,20.0,sat(uParam0[19]));
        float dens = mix(0.03,0.3,sat(uParam1[19]));
        float fog  = sat((h - vObjPos.y) * dens);
        color = mix(color,uColor[19],fog);
    }
    // E20 AtmosphericScatter
    if ((uEffectMask0 & (1<<20)) != 0) {
        float a = pow(1.0-ndv,2.0);
        color = mix(color,uColor[20],a*mix(0.2,1.0,sat(uParam0[20])));
    }
    // E21 GroundFog
    if ((uEffectMask0 & (1<<21)) != 0) {
        float fogH = mix(-2.0, 5.0, sat(uParam0[21]));
        float dens = mix(0.1, 2.0, sat(uParam1[21]));
        float distFactor = sat(length(vPos.xz) * 0.02);
        float fog = sat((fogH - vObjPos.y) * dens) * distFactor;
        color = mix(color, uColor[21], fog * 0.8);
    }

    // ====== 动画变形-片段部分 (E25~E29) ======

    // E25 PulseBreathe
    if ((uEffectMask0 & (1<<25)) != 0) {
        float b = 0.6+0.4*sin(uTime*mix(1.0,8.0,sat(uParam0[25]))*6.2832);
        color = mix(color, uColor[25], b*0.5);
        color *= mix(0.7,1.3,b);
    }
    // E26 ScanLine
    if ((uEffectMask0 & (1<<26)) != 0) {
        float density = mix(5.0,80.0,sat(uParam1[26]));
        float speed   = mix(1.0,20.0,sat(uParam0[26]));
        float scan = step(0.5,fract(vUv.y*density - uTime*speed));
        color = mix(color,uColor[26],scan*0.35);
    }
    // E27 ElectricArc - fragment part
    if ((uEffectMask0 & (1<<27)) != 0) {
        float spd = mix(2.0, 15.0, sat(uParam0[27]));
        float arc = noise(vUv * 30.0 + vec2(uTime * spd, -uTime * spd * 0.7));
        float flash = smoothstep(0.6, 0.9, arc) * mix(0.3, 1.5, sat(uParam1[27]));
        color += uColor[27] * flash;
    }
    // E28 BurnEffect - fragment part
    if ((uEffectMask0 & (1<<28)) != 0) {
        float progress = sat(uParam0[28]);
        float burnY = mix(-2.0, 4.0, progress);
        float dist = vObjPos.y - burnY;
        float edgeWidth = mix(0.1, 0.5, sat(uParam1[28]));
        if (dist < -edgeWidth) {
            color = vec3(0.05, 0.02, 0.01);
        } else if (dist < edgeWidth) {
            float t = sat((dist + edgeWidth) / (2.0 * edgeWidth));
            color = mix(vec3(0.05, 0.02, 0.01), uColor[28], t);
            color += vec3(0.5, 0.3, 0.1) * (1.0 - t) * 2.0;
        }
    }
    // E29 GlitchEffect - fragment part
    if ((uEffectMask0 & (1<<29)) != 0) {
        float intensity = mix(0.0, 0.3, sat(uParam0[29]));
        float freq = mix(2.0, 15.0, sat(uParam1[29]));
        float trigger = step(0.92, fract(sin(uTime * freq) * 43758.5453));
        float offset = trigger * intensity;
        color.r += offset * sin(uTime * 50.0);
        color.b -= offset * cos(uTime * 50.0);
    }

    // ====== 可视化诊断 (E30~E35) ======

    // E30 NormalVis
    if ((uEffectMask0 & (1<<30)) != 0) {
        color = N*0.5+0.5;
    }
    // E31 DepthVis
    if ((uEffectMask0 & (1<<31)) != 0) {
        float near = mix(1.0,50.0,sat(uParam0[31]));
        float far  = mix(10.0,500.0,sat(uParam1[31]));
        float d = sat((length(vPos)-near)/(far-near));
        color = vec3(d);
    }
    // E32 VertexColorVis
    if ((uEffectMask1 & (1<<0)) != 0) {
        color = vColor.rgb;
    }
    // E33 UVVis
    if ((uEffectMask1 & (1<<1)) != 0) {
        color = vec3(fract(vUv.x),fract(vUv.y),0.2);
    }
    // E34 FaceOrientation
    if ((uEffectMask1 & (1<<2)) != 0) {
        color = gl_FrontFacing ? vec3(0.2,0.9,0.4) : vec3(0.9,0.2,0.2);
    }
    // E35 HeightColor
    if ((uEffectMask1 & (1<<3)) != 0) {
        float lo = mix(-5.0, 0.0, sat(uParam0[35]));
        float hi = mix(1.0, 20.0, sat(uParam1[35]));
        float t = sat((vObjPos.y - lo) / max(hi - lo, 0.001));
        vec3 c1 = vec3(0.0, 0.0, 1.0);
        vec3 c2 = vec3(0.0, 1.0, 0.0);
        vec3 c3 = vec3(1.0, 0.0, 0.0);
        color = t < 0.5 ? mix(c1, c2, t * 2.0) : mix(c2, c3, (t - 0.5) * 2.0);
    }

    gl_FragColor = vec4(color, alpha);
}
)GLSL";

// Collect all unique StateSets in the scene graph
struct StateSetCollector : public osg::NodeVisitor {
    std::vector<osg::StateSet*> stateSets;
    std::map<osg::StateSet*, std::vector<osg::Drawable*>> stateSetDrawables;
    StateSetCollector() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

    void addStateSet(osg::StateSet* ss) {
        if (!ss) return;
        for (auto* existing : stateSets) {
            if (existing == ss) return;
        }
        stateSets.push_back(ss);
    }

    void apply(osg::Node& node) override {
        addStateSet(node.getStateSet());
        traverse(node);
    }

    void apply(osg::Geode& geode) override {
        osg::StateSet* geodeSs = geode.getStateSet();
        addStateSet(geodeSs);
        for (unsigned int i = 0; i < geode.getNumDrawables(); ++i) {
            if (auto* d = geode.getDrawable(i)) {
                osg::StateSet* dSs = d->getStateSet();
                addStateSet(dSs);
                // associate drawable to its effective stateset (drawable's own > geode's)
                osg::StateSet* effective = dSs ? dSs : geodeSs;
                if (effective) {
                    stateSetDrawables[effective].push_back(d);
                }
            }
        }
        traverse(geode);
    }
};

} // namespace

PreProcessManager::PreProcessManager() = default;
PreProcessManager::~PreProcessManager() = default;

bool PreProcessManager::hasEnabledEffects() const {
    for (const auto& p : m_params) {
        if (p.enabled) return true;
    }
    return false;
}

void PreProcessManager::attach(osg::Group* root) {
    m_root = root;
    if (!m_root) return;
    if (!m_program) buildProgram();

    detach();
    m_bindings.clear();

    // Compute scene scale from bounding sphere radius (used to scale animation amplitudes)
    const osg::BoundingSphere& bs = m_root->getBound();
    float radius = bs.valid() ? bs.radius() : 0.0f;
    m_sceneScale = (radius > 1.0f) ? radius : 1.0f;
    osg::Vec3 sceneCenter = bs.valid() ? bs.center() : osg::Vec3(0,0,0);

    StateSetCollector collector;
    m_root->accept(collector);

    // If no StateSets found, add one to the root so we can still inject the program
    if (collector.stateSets.empty()) {
        if (!m_root->getStateSet())
            m_root->setStateSet(new osg::StateSet);
        collector.stateSets.push_back(m_root->getStateSet());
    }

    for (auto* ss : collector.stateSets) {
        if (!ss) continue;

        // Compute part center: union of associated drawables' bounding boxes
        osg::Vec3 partCenter = sceneCenter;
        auto it = collector.stateSetDrawables.find(ss);
        if (it != collector.stateSetDrawables.end() && !it->second.empty()) {
            osg::BoundingBox bbox;
            for (auto* d : it->second) {
                if (!d) continue;
                bbox.expandBy(d->getBoundingBox());
            }
            if (bbox.valid()) partCenter = bbox.center();
        }

        Binding binding;
        binding.stateSet = ss;
        binding.effectMask0 = new osg::Uniform("uEffectMask0", 0);
        binding.effectMask1 = new osg::Uniform("uEffectMask1", 0);
        binding.param0      = new osg::Uniform(osg::Uniform::FLOAT, "uParam0", 64);
        binding.param1      = new osg::Uniform(osg::Uniform::FLOAT, "uParam1", 64);
        binding.color       = new osg::Uniform(osg::Uniform::FLOAT_VEC3, "uColor", 64);
        binding.time        = new osg::Uniform("uTime", 0.0f);
        binding.useBaseTex  = new osg::Uniform("uUseBaseTex", true);
        binding.baseTex     = new osg::Uniform("uBaseTex", 0);
        binding.sceneScale  = new osg::Uniform("uSceneScale", m_sceneScale);
        binding.partCenter  = new osg::Uniform("uPartCenter",  partCenter);
        binding.sceneCenter = new osg::Uniform("uSceneCenter", sceneCenter);

        ss->setAttributeAndModes(m_program.get(), osg::StateAttribute::ON);
        ss->addUniform(binding.effectMask0.get());
        ss->addUniform(binding.effectMask1.get());
        ss->addUniform(binding.param0.get());
        ss->addUniform(binding.param1.get());
        ss->addUniform(binding.color.get());
        ss->addUniform(binding.time.get());
        ss->addUniform(binding.useBaseTex.get());
        ss->addUniform(binding.baseTex.get());
        ss->addUniform(binding.sceneScale.get());
        ss->addUniform(binding.partCenter.get());
        ss->addUniform(binding.sceneCenter.get());

        if (auto* tex = dynamic_cast<osg::Texture*>(ss->getTextureAttribute(0, osg::StateAttribute::TEXTURE))) {
            ss->setTextureAttributeAndModes(0, tex, osg::StateAttribute::ON);
        } else {
            binding.useBaseTex->set(false);
        }

        m_bindings.push_back(binding);
    }

    m_attached = !m_bindings.empty();
    syncUniforms();
}

void PreProcessManager::detach() {
    for (auto& binding : m_bindings) {
        if (!binding.stateSet) continue;
        binding.stateSet->removeAttribute(m_program.get());
        binding.stateSet->removeUniform(binding.effectMask0.get());
        binding.stateSet->removeUniform(binding.effectMask1.get());
        binding.stateSet->removeUniform(binding.param0.get());
        binding.stateSet->removeUniform(binding.param1.get());
        binding.stateSet->removeUniform(binding.color.get());
        binding.stateSet->removeUniform(binding.time.get());
        binding.stateSet->removeUniform(binding.useBaseTex.get());
        binding.stateSet->removeUniform(binding.baseTex.get());
    }
    m_bindings.clear();
    m_attached = false;
}

void PreProcessManager::updateTime(float t) {
    m_time = t;
    for (auto& binding : m_bindings) {
        if (binding.time) binding.time->set(m_time);
    }
}

void PreProcessManager::setEffectEnabled(Effect e, bool on) {
    m_params[static_cast<int>(e)].enabled = on;
    syncUniforms();
}

void PreProcessManager::setEffectParam0(Effect e, float v) {
    m_params[static_cast<int>(e)].param0 = v;
    syncUniforms();
}

void PreProcessManager::setEffectParam1(Effect e, float v) {
    m_params[static_cast<int>(e)].param1 = v;
    syncUniforms();
}

void PreProcessManager::setEffectColor(Effect e, float r, float g, float b) {
    auto& p = m_params[static_cast<int>(e)];
    p.colorR = r; p.colorG = g; p.colorB = b;
    syncUniforms();
}

void PreProcessManager::buildProgram() {
    m_program = new osg::Program;
    m_program->addShader(new osg::Shader(osg::Shader::VERTEX,   kVertexShader));
    m_program->addShader(new osg::Shader(osg::Shader::FRAGMENT, kFragmentShader));
}

void PreProcessManager::syncUniforms() {
    if (m_bindings.empty()) return;

    int mask0 = 0, mask1 = 0;
    for (int i = 0; i < kEffectCount; ++i) {
        if (m_params[i].enabled) {
            if (i < 32) mask0 |= (1 << i);
            else        mask1 |= (1 << (i - 32));
        }
    }

    for (auto& binding : m_bindings) {
        if (binding.effectMask0) binding.effectMask0->set(mask0);
        if (binding.effectMask1) binding.effectMask1->set(mask1);
        if (binding.time)        binding.time->set(m_time);
        if (binding.sceneScale)  binding.sceneScale->set(m_sceneScale);
        if (binding.param0 && binding.param1 && binding.color) {
            for (int i = 0; i < 64; ++i) {
                float p0 = (i < kEffectCount) ? m_params[i].param0 : 0.0f;
                float p1 = (i < kEffectCount) ? m_params[i].param1 : 0.0f;
                binding.param0->setElement(i, p0);
                binding.param1->setElement(i, p1);
                if (i < kEffectCount)
                    binding.color->setElement(i, osg::Vec3(m_params[i].colorR,
                                                           m_params[i].colorG,
                                                           m_params[i].colorB));
                else
                    binding.color->setElement(i, osg::Vec3(1.0f, 1.0f, 1.0f));
            }
        }
    }
}
