#version 330 core
// =====================================================================
//  heuristic.frag -- distorcao HEURISTICA
//
//  Portabilidade do metodo do codigo legado (funcao drawDistorcion da
//  versao GLUT), que fazia:
//
//     1. renderizava a cena (estrelas + metade de tras do disco);
//     2. copiava o framebuffer para uma textura (glCopyTexImage2D);
//     3. redesenhava essa textura como uma malha 60x60 de GL_QUADS em
//        projecao ortografica, deslocando a COORDENADA DE TEXTURA de
//        cada vertice radialmente para dentro:
//
//              x_curr = x * (winW/winH)
//              r      = sqrt(x_curr^2 + y^2)
//              shift  = mass / r^2            // mass = 0.009
//              u     -= shift * (x/r)
//              v     -= shift * (y/r)
//
//        (sem deslocar nada dentro de r < eventHorizon = 0.1);
//     4. desenhava a esfera negra e a metade da frente do disco em cima.
//
//  Distorcao em ESPACO DE TELA com queda 1/r^2, sem resolver equacao
//  nenhuma -- a mesma familia de tecnica que GPU Gems 2 cap. 19 usa para
//  refracao e miragem de calor.
//
//  ---------------------------------------------------------------
//  A LEI E RADIAL E ISOTROPICA
//  ---------------------------------------------------------------
//  Vale registrar porque e facil errar: apesar de r usar x corrigido
//  pelo aspecto e a direcao usar x cru, a lei do legado E isotropica no
//  espaco angular. Definindo
//        q = (ndc_x * aspecto, ndc_y),      r = |q|
//  (q e o deslocamento angular dividido por tan(fov/2), e tambem e
//  proporcional ao raio em PIXELS, pois q = 2*offset_px/altura), temos
//  du = -shift*x/r  com  dq_x = 2*aspecto*du = -2*shift*q_x/r, logo
//        dq = -2*shift * q_hat.
//  Puramente radial. O fator 2 vem de (u,v) viver em [0,1] e q em
//  [-1,1]; com ele, uHeurMass = 0.009 reproduz numericamente a mesma
//  deformacao do original.
//
//  Em forma de mapa radial, o metodo inteiro e uma linha:
//        r_amostrado = r - 2*mass/r^2
//
//  ---------------------------------------------------------------
//  DUAS CORRECOES SOBRE O ORIGINAL
//  ---------------------------------------------------------------
//  (1) SATURACAO. Para r pequeno, 2*mass/r^2 fica MAIOR que r e
//      r_amostrado fica negativo: a amostra atravessa o centro da tela
//      e devolve uma copia espelhada do fundo. No original isso
//      acontecia para r < 0.33 (mass = 0.009), e era em parte escondido
//      pela esfera desenhada por cima -- mas o que escapava aparecia
//      como um anel invertido em volta da esfera. Aqui o deslocamento e
//      saturado em HEUR_MAX_PULL*r, o que mantem o mapa
//      monotono e injetivo (r_amostrado >= 0.15*r sempre) sem mexer na
//      lei 1/r^2 onde ela e bem comportada.
//
//  (2) ESCALA. mass e eventHorizon eram constantes fixas em espaco de
//      tela. Como a documentacao anterior registra, era por isso que
//      "nao e possivel implementar um zoom na simulacao". Aqui as duas
//      sao derivadas do raio APARENTE da sombra, sin(theta) = b_c/r0
//      com b_c = 3*sqrt(3)*M, calculado em main.cpp -- a heuristica
//      continua valendo ao mudar zoom e Rs.
//
//  Custo: 1 raio reto por amostra. E o baseline barato do artigo.
// =====================================================================
#include "common.glsl"

in vec2 vNDC;
out vec4 FragColor;

uniform float uHeurMass;    // "mass" do codigo legado (unidades de tela)
uniform float uHeurEH;      // raio de tela da sombra ("eventHorizon")
uniform int   uLegacySplit; // 1 = reproduz o fatiamento em 2 passagens
uniform int   uOutMode;     // 0 = cor; 1 = direcao amostrada (diagnostico)

// Fracao maxima de r que o deslocamento pode consumir. 0.85 deixa a
// distorcao bem agressiva e ainda garante injetividade.
#define HEUR_MAX_PULL 0.85

// Mapa radial em espaco de tela: devolve o NDC de onde amostrar.
vec2 warp(vec2 ndc, float r) {
    float pull = 2.0 * uHeurMass / (r * r);   // lei 1/r^2 do legado
    pull = min(pull, HEUR_MAX_PULL * r);      // saturacao, correcao (1)
    return ndc * (1.0 - pull / r);            // radial e isotropico
}

// ---------------------------------------------------------------------
// As TRES CAMADAS do codigo legado, na mesma ordem:
//
//   1. fundo (ceu + metade de TRAS do disco), distorcido;
//   2. a esfera negra, no seu tamanho projetado real;
//   3. metade da FRENTE do disco, por cima da esfera.
//
// A camada 1 nao tem teste de captura: no original ela era uma textura
// 2D plana, capturada antes de a esfera existir. E por isso que a
// metade de tras do disco aparece ARQUEADA por cima da sombra -- ela e
// puxada para dentro pelo deslocamento. A esfera fica com o tamanho
// projetado real, independente da forca da distorcao.
//
// ---------------------------------------------------------------------
// A FALHA DE JUNCAO E DE ONDE ELA VEM
// ---------------------------------------------------------------------
// No legado a camada 1 era distorcida e a camada 3 nao. Nas duas
// costuras (esquerda e direita, onde o disco cruza o plano que corta o
// buraco negro) o MESMO anel de gas chega em raios de tela diferentes
// nas duas camadas, e as metades nao se encontram: aparece um degrau.
// O tamanho do degrau e exatamente o deslocamento naquele ponto,
//        delta = 2*mass/r_costura^2   (em unidades de NDC vertical).
//
// A correcao e usar o MESMO mapa nas duas metades. A camada 3 continua
// sendo composta depois da esfera -- entao a metade da frente ainda
// passa na frente do buraco negro -- mas agora as duas metades sao
// amostradas pela mesma coordenada deslocada e se encontram
// exatamente. uLegacySplit = 1 volta ao comportamento antigo, para a
// figura de comparacao.
// ---------------------------------------------------------------------
vec3 render(vec2 ndc) {
    // r em espaco angular corrigido pelo aspecto -- o "x_curr" do legado
    float r = length(vec2(ndc.x * uAspect, ndc.y));
    if (r < 1e-6) return vec3(0.0);           // centro exato: a sombra

    vec3 rdWarp = rayDir(warp(ndc, r));       // coordenada deslocada
    vec3 col    = vec3(0.0);

    // ---- camada 1: fundo + metade de tras do disco, distorcidos ----
    if (r >= uHeurEH) {
        col += sky(rdWarp);
        col += diskAlongRay(uCamPos, rdWarp, -1, 1e30);
    }

    // ---- camada 2: a esfera negra ----
    if (r < uHeurEH) col = vec3(0.0);

    // ---- camada 3: metade da frente do disco, por cima da esfera ----
    // No legado, sem distorcao (dai a falha); aqui, com o mesmo mapa.
    vec3 rdFront = (uLegacySplit == 1) ? rayDir(ndc) : rdWarp;
    col += diskAlongRay(uCamPos, rdFront, +1, 1e30);

    return col;
}

void main() {
    // Modo diagnostico: grava a direcao do ceu que este pixel amostra
    // (w = 0 dentro da esfera negra), para o mapa de erro.
    if (uOutMode == 1) {
        float r = length(vec2(vNDC.x * uAspect, vNDC.y));
        vec3  d = (r < 1e-6) ? rayDir(vNDC) : rayDir(warp(vNDC, r));
        FragColor = vec4(d, (r < uHeurEH) ? 0.0 : 1.0);
        return;
    }

    // Supersampling NxN. A distorcao comprime o ceu radialmente perto da
    // sombra, entao um pixel pode cobrir uma regiao do ceu muito maior
    // que uma estrela; sem mais de uma amostra isso cintila.
    int   n   = clamp(uSPP, 1, 3);
    vec2  px  = 2.0 / uResolution;             // tamanho do pixel em NDC
    vec3  acc = vec3(0.0);

    for (int j = 0; j < n; j++) {
        for (int i = 0; i < n; i++) {
            vec2 off = (vec2(float(i), float(j)) + 0.5) / float(n) - 0.5;
            acc += render(vNDC + off * px);
        }
    }

    FragColor = vec4(tonemap(acc / float(n * n)), 1.0);
}
