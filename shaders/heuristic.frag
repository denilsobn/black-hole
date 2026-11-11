#version 330 core

#include "common.glsl"

in vec2 vNDC;
out vec4 FragColor;

uniform float uHeurMass;
uniform float uHeurEH;
uniform int   uLegacySplit;
uniform int   uOutMode;

#define HEUR_MAX_PULL 0.85

vec2 warp(vec2 ndc, float r) {
    float pull = 2.0 * uHeurMass / (r * r);
    pull = min(pull, HEUR_MAX_PULL * r);
    return ndc * (1.0 - pull / r);
}

vec3 render(vec2 ndc) {

    float r = length(vec2(ndc.x * uAspect, ndc.y));
    if (r < 1e-6) return vec3(0.0);

    vec3 rdWarp = rayDir(warp(ndc, r));
    vec3 col    = vec3(0.0);

    if (r >= uHeurEH) {
        col += sky(rdWarp);
        col += diskAlongRay(uCamPos, rdWarp, -1, 1e30);
    }

    if (r < uHeurEH) col = vec3(0.0);

    vec3 rdFront = (uLegacySplit == 1) ? rayDir(ndc) : rdWarp;
    col += diskAlongRay(uCamPos, rdFront, +1, 1e30);

    return col;
}

void main() {

    if (uOutMode == 1) {
        float r = length(vec2(vNDC.x * uAspect, vNDC.y));
        vec3  d = (r < 1e-6) ? rayDir(vNDC) : rayDir(warp(vNDC, r));
        FragColor = vec4(d, (r < uHeurEH) ? 0.0 : 1.0);
        return;
    }

    int   n   = clamp(uSPP, 1, 3);
    vec2  px  = 2.0 / uResolution;
    vec3  acc = vec3(0.0);

    for (int j = 0; j < n; j++) {
        for (int i = 0; i < n; i++) {
            vec2 off = (vec2(float(i), float(j)) + 0.5) / float(n) - 0.5;
            acc += render(vNDC + off * px);
        }
    }

    FragColor = vec4(tonemap(acc / float(n * n)), 1.0);
}
