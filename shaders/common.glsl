// =====================================================================
//  common.glsl
//  Codigo compartilhado pelos dois shaders de distorcao.
//
//  Este arquivo NAO tem "#version": ele e injetado por um #include
//  resolvido em C++ (ver src/shader.h) logo depois do #version do
//  arquivo .frag que o inclui.
//
//  O ponto de manter tudo aqui e METODOLOGICO: a cena (campo de
//  estrelas + disco de acrecao + tone mapping) e exatamente a mesma nos
//  dois shaders. Assim, quando voce comparar heuristica x geodesica no
//  artigo, a unica diferenca entre as duas medicoes e o modelo de
//  curvatura da luz -- nada mais.
// =====================================================================

// ---------------------------------------------------------------------
// Uniforms (identicos nos dois programas)
// ---------------------------------------------------------------------
uniform vec2  uResolution;
uniform vec3  uCamPos;
uniform vec3  uCamForward;
uniform vec3  uCamRight;
uniform vec3  uCamUp;
uniform float uTanHalfFov;
uniform float uAspect;

uniform float uRs;          // raio de Schwarzschild (Rs = 2M, com G = c = 1)
uniform float uTime;        // segundos desde o inicio (anima o disco)
uniform float uExposure;    // exposicao do tone mapping
uniform int   uSPP;         // amostras por pixel por eixo (1, 2 ou 3)

uniform int   uSkyMode;     // 0 = campo de estrelas, 1 = grade lat/long
uniform int   uDiskOn;      // 0 = sem disco, 1 = com disco
uniform float uDiskInner;   // raio interno  (em unidades de Rs)
uniform float uDiskOuter;   // raio externo  (em unidades de Rs)
uniform float uDiskSpin;    // multiplicador da velocidade angular
uniform float uDopplerK;    // forca do Doppler empirico (0.85 no legado)
uniform float uDiskBright;  // ganho de brilho do disco

// Parametro de impacto critico de captura de um foton em Schwarzschild:
//     b_c = 3*sqrt(3)*M = (3*sqrt(3)/2)*Rs ~= 2.598*Rs
// (raio da "sombra" vista de longe -- Misner, Thorne e Wheeler, cap. 25)
#define B_CRIT_OVER_RS 2.59807621

// ---------------------------------------------------------------------
// Hash / ruido
// ---------------------------------------------------------------------
float hash13(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

// Ruido de valor 3D com interpolacao suave (8 amostras)
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

// ---------------------------------------------------------------------
// Campo de estrelas procedural.
//
// Porta a distribuicao do codigo legado (NUM_STARS = 8000 estrelas
// sorteadas uniformemente numa casca esferica, com 3 classes de cor e
// brilho aleatorio) para uma forma procedural indexada pela DIRECAO do
// raio -- sem VBO e, principalmente, funcionando quando o raio chega
// "de tras" depois de ser desviado.
//
// A grade e montada na SUPERFICIE de um cubo (projecao em face de
// cubemap), nao num grid 3D: num grid 3D a estrela e sorteada em
// qualquer ponto do volume da celula e a esfera de direcoes so passa por
// uma fatia fina dela, entao a maioria das estrelas cai fora da casca e
// some. Com STAR_GRID = 200 temos 6*200^2 = 240 mil celulas cobrindo o
// ceu; com 3.33% de ocupacao dao ~8000 estrelas, o mesmo NUM_STARS do
// original.
// ---------------------------------------------------------------------
#define STAR_GRID      200.0
#define STAR_THRESHOLD 0.9667   // 1 - 8000/240000

vec3 starfield(vec3 dir) {
    vec3 d = normalize(dir);
    vec3 a = abs(d);

    // Face dominante do cubo + coordenada 2D dentro dela, em [-1,1]
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

    // Posicao da estrela dentro da celula, limitada a [0.25, 0.75] para
    // que o halo nunca seja cortado na fronteira da celula.
    vec2 off = 0.25 + 0.5 * vec2(hash13(key + 11.0), hash13(key + 23.0));

    float dd   = length(f - off);
    float core = exp(-(dd * dd) / (0.09 * 0.09));
    float halo = 0.35 * exp(-(dd * dd) / (0.22 * 0.22));
    float b    = core + halo;
    if (b < 0.002) return vec3(0.0);

    // As mesmas 3 classes de cor do codigo legado
    float cr = hash13(key + 53.0);
    vec3 tint;
    if      (cr > 0.85) tint = vec3(0.70, 0.80, 1.00);  // azuis
    else if (cr > 0.70) tint = vec3(1.00, 0.80, 0.60);  // amarelas/laranjas
    else                tint = vec3(1.00, 1.00, 1.00);  // brancas

    float intensity = 0.2 + 0.8 * hash13(key + 71.0);   // idem legado
    return tint * intensity * b;
}

// ---------------------------------------------------------------------
// Ceu alternativo: grade de meridianos e paralelos a cada 10 graus
// (tecla G). Um campo de estrelas mostra QUE existe distorcao; a grade
// mostra QUAL. E o equivalente da grade de referencia usada nas figuras
// de Nemiroff (1993) e de Muller e Weiskopf (2010).
// ---------------------------------------------------------------------
vec3 skyGrid(vec3 dir) {
    vec3  d   = normalize(dir);
    float lat = degrees(asin(clamp(d.y, -1.0, 1.0)));   // -90 .. 90
    float lon = degrees(atan(d.z, d.x));                // -180 .. 180

    const float CELL = 10.0;    // graus por celula
    const float W    = 0.35;    // meia-espessura da linha, em graus

    float dLat = abs(fract(lat / CELL + 0.5) - 0.5) * CELL;
    float dLon = abs(fract(lon / CELL + 0.5) - 0.5) * CELL;

    float line = max(1.0 - smoothstep(0.0, W, dLat),
                     1.0 - smoothstep(0.0, W, dLon));

    float chk = mod(floor(lat / CELL) + floor(lon / CELL), 2.0);
    vec3  bg  = mix(vec3(0.020, 0.035, 0.075),
                    vec3(0.070, 0.045, 0.020), chk);

    // Hemisferio norte em verde, sul em ambar: mostra de onde a luz veio
    vec3 lc = (lat >= 0.0) ? vec3(0.25, 0.95, 0.55)
                           : vec3(1.00, 0.80, 0.30);

    return mix(bg, lc, line);
}

vec3 sky(vec3 dir) {
    return (uSkyMode == 1) ? skyGrid(dir) : starfield(dir);
}

// ---------------------------------------------------------------------
// Disco de acrecao -- disco FINO no plano equatorial y = 0.
//
// Modelo portado do codigo legado, onde 100 mil particulas orbitavam
// com velocidade angular ~ r^(-3/2) e a cor era modulada por um
// "Doppler" empirico  D = 1 + 0.85*(v_hat . dir_camera), com reforco de
// azul quando o produto passava de 0.4. Aqui as particulas viraram um
// campo continuo avaliado por pixel:
//
//   * granulacao  -> ruido amostrado em coordenadas COMOVENTES
//                    (phi - Omega*t), o que faz os filamentos girarem
//                    junto com o gas, como as particulas do original;
//   * temperatura -> perfil de disco fino T ~ r^(-3/4)
//                    (Shakura e Sunyaev 1973);
//   * Doppler     -> mesma formula empirica do legado;
//   * redshift    -> fator sqrt(1 - Rs/r), que o legado nao tinha.
//
// A composicao e ADITIVA (emissao), igual ao
// glBlendFunc(GL_SRC_ALPHA, GL_ONE) do codigo legado: disco
// opticamente fino, a face de tras soma com a da frente.
// ---------------------------------------------------------------------
float diskGrain(float r, float phiCo) {
    // Amostrar em (cos, sin) faz o ruido ser periodico em phi, sem a
    // costura que apareceria em phi = +-pi se usassemos phi direto.
    vec3 q = vec3(cos(phiCo) * 6.0, sin(phiCo) * 6.0, r * 2.5);
    float n  = 0.60 * vnoise(q * 2.0);
    n       += 0.30 * vnoise(q * 5.0);
    n       += 0.10 * vnoise(q * 11.0);
    return n;
}

// pos        : ponto do cruzamento com o plano do disco (y ~ 0)
// photonDir  : direcao que aponta do ponto DE VOLTA para a camera.
//              Na versao geodesica isso e a tangente da geodesica
//              invertida, ou seja a direcao LOCAL correta do foton --
//              a heuristica so consegue usar a reta camera-ponto.
vec3 diskEmission(vec3 pos, vec3 photonDir) {
    float r    = length(pos.xz);
    float rIn  = uDiskInner * uRs;
    float rOut = uDiskOuter * uRs;
    if (r < rIn || r > rOut) return vec3(0.0);

    float M = 0.5 * uRs;

    // ---- cinematica kepleriana: Omega = sqrt(M/r^3) ~ r^(-3/2) ----
    // (o legado usava "6.0/sqrt(r^3)", mesma lei com outra constante)
    float phi   = atan(pos.z, pos.x);
    float omega = uDiskSpin * sqrt(M / (r * r * r));
    float phiCo = phi - omega * uTime;        // angulo comovente com o gas

    // ---- granulacao: substitui as 100 mil particulas ----
    float grain = diskGrain(r, phiCo);
    grain = smoothstep(0.20, 0.78, grain);    // filamentos densos

    // ---- perfil radial + bordas suaves (como o alpha do legado) ----
    // O expoente 0.75 e deliberadamente mais suave que o T^4 ~ r^-3 de
    // um disco fino: com r^-3 a borda externa fica 200x mais fraca que a
    // interna e o disco "encolhe" visualmente, ao contrario do disco
    // cheio e brilhante da simulacao legada, que nao tinha queda radial
    // nenhuma -- so o fade nas bordas.
    float t       = (r - rIn) / max(rOut - rIn, 1e-5);
    float fadeIn  = smoothstep(0.0, 0.05, t);
    float fadeOut = 1.0 - smoothstep(0.86, 1.0, t);
    float radial  = pow(rIn / r, 0.75);

    // ---- Doppler empirico, identico ao codigo legado ----
    vec2 vhat = vec2(-pos.z, pos.x) / r;      // tangente da orbita
    vec2 tc   = vec2(photonDir.x, photonDir.z);
    float tl  = length(tc);
    float dotp = (tl > 1e-6) ? dot(vhat, tc / tl) : 0.0;

    float doppler = max(1.0 + uDopplerK * dotp, 0.0);

    // ---- temperatura -> cor ----
    float temp = pow(rIn / r, 0.75);          // T ~ r^(-3/4)
    vec3  cool = vec3(1.00, 0.30, 0.05);      // laranja/vermelho (externa)
    vec3  hot  = vec3(1.00, 0.94, 0.82);      // branco quente (interna)
    vec3  col  = mix(cool, hot, smoothstep(0.38, 1.00, temp)) * doppler;

    // Reforco de blueshift do lado que se aproxima -- mesmos limiares
    // e ganhos do codigo legado (0.4 / 1.5 / 0.5)
    if (dotp > 0.4) {
        col.b += (dotp - 0.4) * 1.5;
        col.g += (dotp - 0.4) * 0.5;
    }

    // ---- redshift gravitacional (fator de lapso) ----
    col *= sqrt(max(1.0 - uRs / r, 0.0));

    return col * (uDiskBright * grain * radial * fadeIn * fadeOut);
}

// ---------------------------------------------------------------------
// Distancia ate a esfera de captura de raio b_c. 1e30 = nao acerta.
// ---------------------------------------------------------------------
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
    if (t1 > 0.0) return 0.0;       // camera dentro da sombra
    return 1e30;
}

// ---------------------------------------------------------------------
// Emissao do disco ao longo de um raio RETO (intersecao com y = 0).
//
// side:  0 = as duas metades
//       -1 = so a metade ATRAS do plano que corta o buraco negro
//       +1 = so a metade DA FRENTE
// O criterio de metade e o mesmo produto escalar do codigo legado:
//     dotPlane = px*camX + py*camY + pz*camZ
// (negativo = atras). Ver secao 4 da documentacao anterior.
//
// tMax limita a distancia, para o disco nao aparecer atras da sombra.
// ---------------------------------------------------------------------
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

// ---------------------------------------------------------------------
// Traca um raio RETO completo: sombra + disco (duas metades) + ceu.
// Usado pelo caso degenerado do shader geodesico (raio radial, em que
// nao existe plano orbital e a deflexao e nula).
// ---------------------------------------------------------------------
vec3 traceStraight(vec3 ro, vec3 rd) {
    float tSh = shadowT(ro, rd);
    vec3  col = diskAlongRay(ro, rd, 0, tSh);
    if (tSh < 1e29) return col;     // engolido: so o que estava na frente
    return col + sky(rd);
}

// ---------------------------------------------------------------------
// Tone mapping exponencial. O disco e aditivo e estoura facil acima de
// 1.0; sem isso a comparacao visual entre os dois shaders fica poluida
// por saturacao em vez de geometria.
// ---------------------------------------------------------------------
vec3 tonemap(vec3 c) {
    return 1.0 - exp(-uExposure * max(c, vec3(0.0)));
}

// ---------------------------------------------------------------------
// Direcao do raio para uma coordenada NDC.
// ---------------------------------------------------------------------
vec3 rayDir(vec2 ndc) {
    return normalize(uCamForward
        + ndc.x * uTanHalfFov * uAspect * uCamRight
        + ndc.y * uTanHalfFov * uCamUp);
}
