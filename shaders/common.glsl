uniform vec2  uResolution;
uniform vec3  uCamPos;
uniform vec3  uCamForward;
uniform vec3  uCamRight;
uniform vec3  uCamUp;
uniform float uTanHalfFov;
uniform float uAspect;

uniform float uRs;
uniform float uTime;
uniform float uExposure;
uniform int   uSPP;

uniform int   uSkyMode;
uniform int   uDiskOn;
uniform float uDiskInner;
uniform float uDiskOuter;
uniform float uDiskSpin;
uniform float uDopplerK;
uniform float uDiskBright;

#define B_CRIT_OVER_RS 2.59807621

float hash13(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float vnoise(vec3 x) {
    vec3 i = floor(x);
    vec3 f = x - i;
    f = f * f * (3.0 - 2.0 * f);

    float n000 = hash13(i + vec3(0.0, 0.0, 0.0));
    float n100 = hash13(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash13(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash13(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash13(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash13(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash13(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash13(i + vec3(1.0, 1.0, 1.0));

    return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
               mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}

#define STAR_GRID      200.0
#define STAR_THRESHOLD 0.9667

vec3 starfield(vec3 dir) {
    vec3 d = normalize(dir);
    vec3 a = abs(d);

    float face;
    vec2  uv;
    if (a.x >= a.y && a.x >= a.z) {
        face = (d.x > 0.0) ? 0.0 : 1.0;
        uv   = vec2(d.y, d.z) / a.x;
    } else if (a.y >= a.z) {
        face = (d.y > 0.0) ? 2.0 : 3.0;
        uv   = vec2(d.z, d.x) / a.y;
    } else {
        face = (d.z > 0.0) ? 4.0 : 5.0;
        uv   = vec2(d.x, d.y) / a.z;
    }

    vec2 p    = (uv * 0.5 + 0.5) * STAR_GRID;
    vec2 cell = floor(p);
    vec2 f    = p - cell;

    vec3 key = vec3(cell, face);
    if (hash13(key) < STAR_THRESHOLD) return vec3(0.0);

    vec2 off = 0.25 + 0.5 * vec2(hash13(key + 11.0), hash13(key + 23.0));

    float dd   = length(f - off);
    float core = exp(-(dd * dd) / (0.09 * 0.09));
    float halo = 0.35 * exp(-(dd * dd) / (0.22 * 0.22));
    float b    = core + halo;
    if (b < 0.002) return vec3(0.0);

    float cr = hash13(key + 53.0);
    vec3 tint;
    if      (cr > 0.85) tint = vec3(0.70, 0.80, 1.00);
    else if (cr > 0.70) tint = vec3(1.00, 0.80, 0.60);
    else                tint = vec3(1.00, 1.00, 1.00);

    float intensity = 0.2 + 0.8 * hash13(key + 71.0);
    return tint * intensity * b;
}

vec3 skyGrid(vec3 dir) {
    vec3  d   = normalize(dir);
    float lat = degrees(asin(clamp(d.y, -1.0, 1.0)));
    float lon = degrees(atan(d.z, d.x));

    const float CELL = 10.0;
    const float W    = 0.35;

    float dLat = abs(fract(lat / CELL + 0.5) - 0.5) * CELL;
    float dLon = abs(fract(lon / CELL + 0.5) - 0.5) * CELL;

    float line = max(1.0 - smoothstep(0.0, W, dLat),
                     1.0 - smoothstep(0.0, W, dLon));

    float chk = mod(floor(lat / CELL) + floor(lon / CELL), 2.0);
    vec3  bg  = mix(vec3(0.020, 0.035, 0.075),
                    vec3(0.070, 0.045, 0.020), chk);

    vec3 lc = (lat >= 0.0) ? vec3(0.25, 0.95, 0.55)
                           : vec3(1.00, 0.80, 0.30);

    return mix(bg, lc, line);
}

vec3 sky(vec3 dir) {
    return (uSkyMode == 1) ? skyGrid(dir) : starfield(dir);
}

float diskGrain(float r, float phiCo) {

    vec3 q = vec3(cos(phiCo) * 6.0, sin(phiCo) * 6.0, r * 2.5);
    float n  = 0.60 * vnoise(q * 2.0);
    n       += 0.30 * vnoise(q * 5.0);
    n       += 0.10 * vnoise(q * 11.0);
    return n;
}

vec3 diskEmission(vec3 pos, vec3 photonDir) {
    float r    = length(pos.xz);
    float rIn  = uDiskInner * uRs;
    float rOut = uDiskOuter * uRs;
    if (r < rIn || r > rOut) return vec3(0.0);

    float M = 0.5 * uRs;

    float phi   = atan(pos.z, pos.x);
    float omega = uDiskSpin * sqrt(M / (r * r * r));
    float phiCo = phi - omega * uTime;

    float grain = diskGrain(r, phiCo);
    grain = smoothstep(0.20, 0.78, grain);

    float t       = (r - rIn) / max(rOut - rIn, 1e-5);
    float fadeIn  = smoothstep(0.0, 0.05, t);
    float fadeOut = 1.0 - smoothstep(0.86, 1.0, t);
    float radial  = pow(rIn / r, 0.75);

    vec2 vhat = vec2(-pos.z, pos.x) / r;
    vec2 tc   = vec2(photonDir.x, photonDir.z);
    float tl  = length(tc);
    float dotp = (tl > 1e-6) ? dot(vhat, tc / tl) : 0.0;

    float doppler = max(1.0 + uDopplerK * dotp, 0.0);

    float temp = pow(rIn / r, 0.75);
    vec3  cool = vec3(1.00, 0.30, 0.05);
    vec3  hot  = vec3(1.00, 0.94, 0.82);
    vec3  col  = mix(cool, hot, smoothstep(0.38, 1.00, temp)) * doppler;

    if (dotp > 0.4) {
        col.b += (dotp - 0.4) * 1.5;
        col.g += (dotp - 0.4) * 0.5;
    }

    col *= sqrt(max(1.0 - uRs / r, 0.0));

    return col * (uDiskBright * grain * radial * fadeIn * fadeOut);
}

float shadowT(vec3 ro, vec3 rd) {
    float Rsh  = B_CRIT_OVER_RS * uRs;
    float bb   = dot(ro, rd);
    float cc   = dot(ro, ro) - Rsh * Rsh;
    float disc = bb * bb - cc;
    if (disc <= 0.0) return 1e30;
    float s  = sqrt(disc);
    float t0 = -bb - s;
    float t1 = -bb + s;
    if (t0 > 0.0) return t0;
    if (t1 > 0.0) return 0.0;
    return 1e30;
}

vec3 diskAlongRay(vec3 ro, vec3 rd, int side, float tMax) {
    if (uDiskOn == 0 || abs(rd.y) < 1e-7) return vec3(0.0);

    float td = -ro.y / rd.y;
    if (td <= 0.0 || td >= tMax) return vec3(0.0);

    vec3 p = ro + td * rd;

    if (side != 0) {
        float dotPlane = dot(p, uCamPos);
        if (side < 0 && dotPlane >= 0.0) return vec3(0.0);
        if (side > 0 && dotPlane <  0.0) return vec3(0.0);
    }
    return diskEmission(p, -rd);
}

vec3 traceStraight(vec3 ro, vec3 rd) {
    float tSh = shadowT(ro, rd);
    vec3  col = diskAlongRay(ro, rd, 0, tSh);
    if (tSh < 1e29) return col;
    return col + sky(rd);
}

vec3 tonemap(vec3 c) {
    return 1.0 - exp(-uExposure * max(c, vec3(0.0)));
}

vec3 rayDir(vec2 ndc) {
    return normalize(uCamForward
        + ndc.x * uTanHalfFov * uAspect * uCamRight
        + ndc.y * uTanHalfFov * uCamUp);
}
