#version 330 core

#include "common.glsl"

in vec2 vNDC;
out vec4 FragColor;

uniform float uStepPhi;
uniform int   uMaxSteps;
uniform int   uOutMode;

vec3 render(vec2 ndc, out vec4 skyDir) {
    vec3 rd = rayDir(ndc);
    skyDir = vec4(rd, 1.0);

    vec3  ro = uCamPos;
    float r0 = length(ro);

    vec3  e1   = ro / r0;
    vec3  nrm  = cross(ro, rd);
    float nlen = length(nrm);

    if (nlen < 1e-6) {
        skyDir.w = (dot(rd, e1) < 0.0) ? 0.0 : 1.0;
        return traceStraight(ro, rd);
    }

    vec3 axis = nrm / nlen;
    vec3 e2   = normalize(cross(axis, e1));

    float M      = 0.5 * uRs;
    float cosPsi = -dot(rd, e1);
    float sinPsi = nlen / r0;
    float b      = r0 * sinPsi / sqrt(max(1.0 - uRs / r0, 1e-6));

    float u   = 1.0 / r0;
    float w2  = 1.0 / (b * b) - u * u + 2.0 * M * u * u * u;
    float w   = sign(cosPsi) * sqrt(max(w2, 0.0));
    float phi = 0.0;

    float sPrev   = e1.y;
    float uPrev   = u;
    float wPrev   = w;
    float phiPrev = 0.0;

    float uEsc = 1.0 / (200.0 * max(r0, 1.0));
    float uHor = 1.0 / max(uRs, 1e-6);

    vec3  col      = vec3(0.0);
    bool  absorbed = false;
    bool  haveEnd  = false;
    float phiEnd   = 0.0;

    for (int i = 0; i < uMaxSteps; i++) {
        float h = uStepPhi;

        float k1u = w;
        float k1w = -u + 3.0 * M * u * u;

        float ua  = u + 0.5 * h * k1u;
        float wa  = w + 0.5 * h * k1w;
        float k2u = wa;
        float k2w = -ua + 3.0 * M * ua * ua;

        float ub  = u + 0.5 * h * k2u;
        float wb  = w + 0.5 * h * k2w;
        float k3u = wb;
        float k3w = -ub + 3.0 * M * ub * ub;

        float uc  = u + h * k3u;
        float wc  = w + h * k3w;
        float k4u = wc;
        float k4w = -uc + 3.0 * M * uc * uc;

        u += (h / 6.0) * (k1u + 2.0 * k2u + 2.0 * k3u + k4u);
        w += (h / 6.0) * (k1w + 2.0 * k2w + 2.0 * k3w + k4w);
        phi += h;

        if (u > uHor) { absorbed = true; break; }

        if (u <= 0.0) {

            float fr = uPrev / (uPrev - u);
            phiEnd  = phiPrev + fr * (phi - phiPrev);
            haveEnd = true;
            break;
        }

        float sCur = cos(phi) * e1.y + sin(phi) * e2.y;

        if (uDiskOn == 1 && sPrev * sCur < 0.0) {
            float fr   = sPrev / (sPrev - sCur);
            float phiC = phiPrev + fr * (phi - phiPrev);
            float uC   = mix(uPrev, u, fr);
            float wC   = mix(wPrev, w, fr);

            if (uC > 1e-7) {
                float rC = 1.0 / uC;

                vec3 er  =  cos(phiC) * e1 + sin(phiC) * e2;
                vec3 eph = -sin(phiC) * e1 + cos(phiC) * e2;
                vec3 pC  = rC * er;

                vec3 T = normalize((-wC / (uC * uC)) * er + rC * eph);

                col += diskEmission(pC, -T);
            }
        }

        sPrev   = sCur;
        uPrev   = u;
        wPrev   = w;
        phiPrev = phi;

        if (u < uEsc && w < 0.0) {

            phiEnd  = phi + (3.14159265 - atan(u, w));
            haveEnd = true;
            break;
        }
    }

    if (!haveEnd) phiEnd = phi;

    vec3 fim = cos(phiEnd) * e1 + sin(phiEnd) * e2;
    skyDir = vec4(fim, absorbed ? 0.0 : 1.0);

    if (!absorbed) col += sky(fim);
    return col;
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
