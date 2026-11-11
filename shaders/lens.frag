#version 330 core

#include "common.glsl"
#include "lente_coef.glsl"

in vec2 vNDC;
out vec4 FragColor;

uniform int uLensModel;
uniform int uOutMode;

float alphaLente(float b, float M) {
    if (uLensModel == 0) return 4.0 * M / b;
    float x = min(B_CRIT_OVER_RS * uRs / b, 0.9999999);
    float l = log(1.0 - x);
    return -l + x * (LENTE_C1 + x * (LENTE_C2 + x * LENTE_C3))
              + LENTE_C4 * (1.0 - x) * l;
}

float trechoObservador(float r0, float b, float M) {
    float s = min(b / r0, 0.999999);
    float c = sqrt(1.0 - s * s);
    return asin(s) - (M / b) * (1.0 / c + c - 2.0);
}

vec3 render(vec2 ndc, out vec4 skyDir) {
    vec3  rd = rayDir(ndc);
    vec3  ro = uCamPos;
    float r0 = length(ro);
    vec3  n  = -ro / r0;
    float M  = 0.5 * uRs;

    float cosPsi = dot(rd, n);
    vec3  tv     = rd - cosPsi * n;
    float sinPsi = length(tv);
    vec3  t      = (sinPsi > 1e-7) ? tv / sinPsi : vec3(0.0);

    float b  = r0 * sinPsi / sqrt(max(1.0 - uRs / r0, 1e-6));
    float bc = B_CRIT_OVER_RS * uRs;
    bool  entra    = (cosPsi > 0.0);
    bool  capturado = entra && (b < bc);

    float thetaS;
    if (capturado)  thetaS = 0.0;
    else if (entra) thetaS = trechoObservador(r0, b, M) - alphaLente(b, M);
    else            thetaS = 3.14159265 - trechoObservador(r0, b, M);
    vec3 s = cos(thetaS) * n + sin(thetaS) * t;

    skyDir = vec4(s, capturado ? 0.0 : 1.0);

    float tPerto = max(r0 * cosPsi, 0.0);
    vec3  col    = diskAlongRay(ro, rd, 0, tPerto);
    if (capturado) return col;

    vec3 P = ro + tPerto * rd;
    col += diskAlongRay(P, s, 0, 1e30);

    return col + sky(s);
}

void main() {
    vec4 skyDir;

    if (uOutMode == 1) {
        render(vNDC, skyDir);
        FragColor = skyDir;
        return;
    }

    int   n   = clamp(uSPP, 1, 3);
    vec2  px  = 2.0 / uResolution;
    vec3  acc = vec3(0.0);
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < n; i++) {
            vec2 off = (vec2(float(i), float(j)) + 0.5) / float(n) - 0.5;
            acc += render(vNDC + off * px, skyDir);
        }
    }
    FragColor = vec4(tonemap(acc / float(n * n)), 1.0);
}
