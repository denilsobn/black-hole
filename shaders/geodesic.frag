#version 330 core
// =====================================================================
//  geodesic.frag -- distorcao FISICA (integracao numerica)
//
//  Integra a equacao da orbita do foton na metrica de Schwarzschild:
//
//       d^2u/dphi^2 + u = 3*M*u^2       com  u = 1/r,  M = Rs/2,  G=c=1
//
//  por Runge-Kutta classico de 4a ordem (RK4), marchando em phi a
//  partir da camera. Cada AMOSTRA integra a sua propria geodesica.
//
//  ---------------------------------------------------------------
//  CONDICAO INICIAL: OBSERVADOR ESTATICO
//  ---------------------------------------------------------------
//  A camera e um observador ESTATICO em r0: a direcao d de cada pixel
//  esta no referencial local dele, nao nas coordenadas de Schwarzschild.
//  Sendo psi o angulo local entre d e a direcao do buraco negro, o
//  parametro de impacto do raio e dado pela formula de Synge (1966):
//
//        sin(psi) = (b / r0) * sqrt(1 - 2M/r0)
//
//  e w(0) = du/dphi sai da lei de conservacao
//        w^2 = 1/b^2 - u^2 + 2 M u^3 ,
//  com sinal positivo se o raio aponta para dentro (psi < 90 graus).
//
//  Uma versao anterior usava b = r0*sin(psi) e w = -cos(alpha)/b, isto
//  e, tratava d como direcao COORDENADA. O parametro de impacto
//  conservado saia entao b/sqrt(1 - 2M b^2/r0^3) -- 0,02% maior em
//  r0 = 26 Rs. Longe da sombra isso nao aparece; perto dela a deflexao
//  diverge com -ln(b - b_c), e a diferenca chegava a 2,5 rad.
//
//  O plano orbital e gerado por e1 (radial) e e2 (componente
//  transversal de d), o mesmo nos dois referenciais.
//
//  ---------------------------------------------------------------
//  DISCO DE ACRECAO
//  ---------------------------------------------------------------
//  A cada passo do RK4 checamos o sinal da altura do foton em relacao
//  ao plano equatorial. Como P(phi) = r*(cos*e1 + sin*e2) com r > 0, o
//  sinal da altura e o sinal de
//        s(phi) = cos(phi)*e1.y + sin(phi)*e2.y.
//  Quando s troca de sinal houve um cruzamento: a posicao exata sai por
//  interpolacao linear e a emissao do disco e somada ali.
//
//  E isso que produz o visual classico: o MESMO disco aparece varias
//  vezes na imagem, porque a geodesica pode cruzar o plano equatorial
//  mais de uma vez, e a imagem secundaria passa por cima e por baixo da
//  sombra (Luminet 1979). A heuristica nao consegue produzir isso: um
//  raio reto cruza o plano no maximo uma vez.
//
//  Custo: ate uMaxSteps avaliacoes de RK4 por amostra (4 derivadas
//  cada), vezes uSPP^2 amostras por pixel.
// =====================================================================
#include "common.glsl"

in vec2 vNDC;
out vec4 FragColor;

uniform float uStepPhi;   // passo fixo em phi, em radianos
uniform int   uMaxSteps;  // teto de passos por amostra
uniform int   uOutMode;   // 0 = cor; 1 = direcao final no ceu (diagnostico)

// skyDir.xyz = direcao de onde a luz veio no ceu distante;
// skyDir.w   = 1 se o raio escapou, 0 se caiu no horizonte.
vec3 render(vec2 ndc, out vec4 skyDir) {
    vec3 rd = rayDir(ndc);
    skyDir = vec4(rd, 1.0);

    vec3  ro = uCamPos;
    float r0 = length(ro);

    vec3  e1   = ro / r0;              // eixo radial da camera
    vec3  nrm  = cross(ro, rd);        // normal do plano orbital
    float nlen = length(nrm);

    // Caso degenerado: raio praticamente radial. O plano orbital fica
    // indefinido; se aponta para o buraco negro, cai.
    if (nlen < 1e-6) {
        skyDir.w = (dot(rd, e1) < 0.0) ? 0.0 : 1.0;
        return traceStraight(ro, rd);
    }

    vec3 axis = nrm / nlen;
    vec3 e2   = normalize(cross(axis, e1));  // tangencial, no sentido do raio

    float M      = 0.5 * uRs;
    float cosPsi = -dot(rd, e1);             // psi: angulo ate o buraco negro
    float sinPsi = nlen / r0;
    float b      = r0 * sinPsi / sqrt(max(1.0 - uRs / r0, 1e-6));   // Synge

    float u   = 1.0 / r0;
    float w2  = 1.0 / (b * b) - u * u + 2.0 * M * u * u * u;
    float w   = sign(cosPsi) * sqrt(max(w2, 0.0));   // du/dphi em phi = 0
    float phi = 0.0;

    // Estado anterior, para detectar o cruzamento do plano do disco
    float sPrev   = e1.y;              // s(0) = cos(0)*e1.y + sin(0)*e2.y
    float uPrev   = u;
    float wPrev   = w;
    float phiPrev = 0.0;

    // Criterio de escape: r maior que 200*r0 e ainda crescendo
    float uEsc = 1.0 / (200.0 * max(r0, 1.0));
    float uHor = 1.0 / max(uRs, 1e-6);

    vec3  col      = vec3(0.0);
    bool  absorbed = false;
    bool  haveEnd  = false;
    float phiEnd   = 0.0;

    for (int i = 0; i < uMaxSteps; i++) {
        float h = uStepPhi;

        // ---- RK4 em  du/dphi = w ,  dw/dphi = -u + 3*M*u^2 ----
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

        if (u > uHor) { absorbed = true; break; }   // cruzou o horizonte

        if (u <= 0.0) {
            // O passo pulou por cima de r = infinito: interpola u = 0
            float fr = uPrev / (uPrev - u);
            phiEnd  = phiPrev + fr * (phi - phiPrev);
            haveEnd = true;
            break;
        }

        // ---- cruzamento do plano equatorial: emissao do disco ----
        float sCur = cos(phi) * e1.y + sin(phi) * e2.y;

        if (uDiskOn == 1 && sPrev * sCur < 0.0) {
            float fr   = sPrev / (sPrev - sCur);        // fracao do passo
            float phiC = phiPrev + fr * (phi - phiPrev);
            float uC   = mix(uPrev, u, fr);
            float wC   = mix(wPrev, w, fr);

            if (uC > 1e-7) {
                float rC = 1.0 / uC;

                vec3 er  =  cos(phiC) * e1 + sin(phiC) * e2;
                vec3 eph = -sin(phiC) * e1 + cos(phiC) * e2;
                vec3 pC  = rC * er;

                // Tangente da geodesica (direcao de propagacao):
                //   dP/dphi = (dr/dphi)*er + r*ephi ,  dr/dphi = -w/u^2
                vec3 T = normalize((-wC / (uC * uC)) * er + rC * eph);

                // -T aponta de volta para a camera: e a direcao local
                // correta do foton para o calculo do Doppler
                col += diskEmission(pC, -T);
            }
        }

        sPrev   = sCur;
        uPrev   = u;
        wPrev   = w;
        phiPrev = phi;

        if (u < uEsc && w < 0.0) {
            // -------------------------------------------------------------
            // Escapou. Em vez de parar em phi (que e sempre um multiplo de
            // uStepPhi), extrapola para r = infinito de forma analitica.
            //
            // Longe do buraco negro o termo 3*M*u^2 e desprezivel e a
            // solucao vira a reta do espaco plano, u = sin(phi - phi0)/b.
            // Conhecendo u e w = du/dphi, o angulo dessa senoide vale
            // theta = atan2(u, w), e ainda faltam (pi - theta) radianos
            // para u chegar a zero.
            //
            // Isso NAO e cosmetico: sem a extrapolacao, phi final fica
            // quantizado no passo do RK4, amostras vizinhas amostram o ceu
            // em direcoes que pulam de ~uStepPhi de cada vez, e as
            // estrelas saem como TRACINHOS em vez de pontos.
            // -------------------------------------------------------------
            phiEnd  = phi + (3.14159265 - atan(u, w));
            haveEnd = true;
            break;
        }
    }

    if (!haveEnd) phiEnd = phi;   // estourou uMaxSteps: usa o que tem

    // Direcao assintotica de onde a luz veio no ceu distante.
    // Para r grande a direcao da POSICAO e a da propagacao coincidem.
    vec3 fim = cos(phiEnd) * e1 + sin(phiEnd) * e2;
    skyDir = vec4(fim, absorbed ? 0.0 : 1.0);

    if (!absorbed) col += sky(fim);
    return col;
}

void main() {
    vec4 skyDir;

    // Modo diagnostico: em vez de cor, grava a direcao final no ceu do
    // raio que passa pelo CENTRO do pixel. E o que o mapa de erro
    // compara pixel a pixel com as heuristicas.
    if (uOutMode == 1) {
        render(vNDC, skyDir);
        FragColor = skyDir;
        return;
    }

    // Supersampling NxN. Perto do anel de fotons a ampliacao tangencial
    // passa de 10x e um pixel cobre uma regiao do ceu muito maior que
    // uma estrela -- e onde o aliasing aparece. Cada amostra e uma
    // geodesica inteira, entao o custo cresce com N^2.
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
