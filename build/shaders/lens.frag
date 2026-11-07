#version 330 core
// =====================================================================
//  lens.frag -- heuristica de LENTE FINA CALIBRADA
//
//  Em vez de deslocar coordenadas de tela por uma lei inventada
//  (1/r^2 no codigo legado), usa a estrutura exata do problema e
//  aproxima so a parte cara.
//
//  ---------------------------------------------------------------
//  A IDENTIDADE
//  ---------------------------------------------------------------
//  Para um observador estatico em r0 e um raio que passa pelo perielio,
//  o angulo theta_s entre a direcao de onde a luz vem no ceu e a
//  direcao do buraco negro e EXATAMENTE
//
//        theta_s = I(r0, b) - alpha(b)
//
//  onde
//    * b e o parametro de impacto, que o observador obtem do angulo
//      local psi pela formula de Synge:  sin(psi) = (b/r0) sqrt(1-2M/r0);
//    * alpha(b) e a deflexao TOTAL de um raio que vem do infinito e
//      volta ao infinito -- toda a curvatura forte mora aqui;
//    * I(r0, b) = int_0^{1/r0} du / sqrt(1/b^2 - u^2 + 2Mu^3) e o angulo
//      varrido entre o infinito e o observador, em campo fraco.
//
//  (Sem massa, I = asin(b/r0) = psi e alpha = 0: theta_s = psi.)
//
//  ---------------------------------------------------------------
//  O QUE E APROXIMADO
//  ---------------------------------------------------------------
//  I tem forma fechada em 1a ordem em M:
//        I ~ asin(s) - (M/b) (1/c + c - 2),   s = b/r0,  c = sqrt(1-s^2).
//
//  alpha(b) tem duas versoes, selecionadas por uLensModel:
//
//    0 = Einstein:   alpha = 4M/b.  A lente fina classica. Correta longe
//                    da sombra; nao tem anel de fotons.
//
//    1 = calibrada:  alpha(x) = -ln(1-x) + c1 x + c2 x^2 + c3 x^3
//                               + c4 (1-x) ln(1-x),     x = b_c / b.
//                    O -ln(1-x) e a divergencia de campo forte (Bozza
//                    2002); c1..c4 foram APRENDIDOS por minimos quadrados
//                    a partir de 200 raios integrados por RK4
//                    (tools/calibra_lente.cpp -> lente_coef.glsl). Erro
//                    maximo de alpha no treino: 0,0096 graus.
//
//  As duas versoes tem a mesma estrutura e a mesma sombra (exata:
//  b < b_c); a unica diferenca entre elas e alpha. Isso isola, no mapa
//  de erro, o efeito do termo aprendido.
//
//  ---------------------------------------------------------------
//  DISCO DE ACRECAO: RAIO EM DOIS SEGMENTOS
//  ---------------------------------------------------------------
//  O mapeamento acima vale para fontes no infinito (o ceu). O disco
//  esta perto do buraco negro, entao usamos a geometria de lente fina:
//  o raio segue reto da camera ate o ponto de maior aproximacao, sofre
//  ali a deflexao inteira e segue reto na direcao final. Sao dois
//  segmentos, logo o raio pode cruzar o plano do disco DUAS vezes -- e
//  e isso que produz a imagem do disco por cima da sombra, que um raio
//  reto unico (heuristica de espaco de tela) nao consegue.
//
//  Custo: constante por pixel -- um log, um asin, algumas raizes.
// =====================================================================
#include "common.glsl"
#include "lente_coef.glsl"

in vec2 vNDC;
out vec4 FragColor;

uniform int uLensModel;   // 0 = Einstein (campo fraco); 1 = calibrada
uniform int uOutMode;     // 0 = cor; 1 = direcao no ceu (diagnostico)

float alphaLente(float b, float M) {
    if (uLensModel == 0) return 4.0 * M / b;
    float x = min(B_CRIT_OVER_RS * uRs / b, 0.9999999);
    float l = log(1.0 - x);
    return -l + x * (LENTE_C1 + x * (LENTE_C2 + x * LENTE_C3))
              + LENTE_C4 * (1.0 - x) * l;
}

// Angulo varrido entre o infinito e o observador (1a ordem em M)
float trechoObservador(float r0, float b, float M) {
    float s = min(b / r0, 0.999999);
    float c = sqrt(1.0 - s * s);
    return asin(s) - (M / b) * (1.0 / c + c - 2.0);
}

vec3 render(vec2 ndc, out vec4 skyDir) {
    vec3  rd = rayDir(ndc);
    vec3  ro = uCamPos;
    float r0 = length(ro);
    vec3  n  = -ro / r0;                        // direcao do buraco negro
    float M  = 0.5 * uRs;

    float cosPsi = dot(rd, n);
    vec3  tv     = rd - cosPsi * n;
    float sinPsi = length(tv);
    vec3  t      = (sinPsi > 1e-7) ? tv / sinPsi : vec3(0.0);

    float b  = r0 * sinPsi / sqrt(max(1.0 - uRs / r0, 1e-6));   // Synge
    float bc = B_CRIT_OVER_RS * uRs;
    bool  entra    = (cosPsi > 0.0);
    bool  capturado = entra && (b < bc);

    // ---- direcao no ceu ----
    float thetaS;
    if (capturado)  thetaS = 0.0;
    else if (entra) thetaS = trechoObservador(r0, b, M) - alphaLente(b, M);
    else            thetaS = 3.14159265 - trechoObservador(r0, b, M);  // raio saindo
    vec3 s = cos(thetaS) * n + sin(thetaS) * t;

    skyDir = vec4(s, capturado ? 0.0 : 1.0);

    // ---- disco: segmento 1, da camera ate o ponto de maior aproximacao
    float tPerto = max(r0 * cosPsi, 0.0);
    vec3  col    = diskAlongRay(ro, rd, 0, tPerto);
    if (capturado) return col;

    // ---- disco: segmento 2, do ponto de maior aproximacao na direcao s
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
