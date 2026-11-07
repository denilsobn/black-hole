// =====================================================================
//  Buraco Negro -- OpenGL 3.3 core (pipeline programavel)
//
//  Quatro modelos de curvatura da luz no mesmo programa, para
//  comparacao direta:
//
//    tecla 1 -> shaders/heuristic.frag : heuristica legada (deslocamento
//               1/r^2 das coordenadas de tela);
//    tecla 2 -> shaders/geodesic.frag  : RK4 da geodesica do foton em
//               Schwarzschild -- a verdade de referencia;
//    tecla 3 -> shaders/lens.frag      : lente fina de Einstein;
//    tecla 4 -> shaders/lens.frag      : lente fina CALIBRADA (deflexao
//               aprendida do RK4 por tools/calibra_lente.cpp).
//
//  A cena (estrelas, disco de acrecao, Doppler, tone mapping) e a mesma
//  em todos, definida em shaders/common.glsl -- de propósito, para que a
//  medicao isole so o modelo de curvatura.
//
//  Modos sem janela: --foto (captura), --bench (tempo de parede),
//  --erro (mapas de erro angular contra o RK4), --perfil (tempo de GPU
//  por resolucao).
// =====================================================================
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include "shader.h"
#include "png_simples.h"

static const float PI = 3.14159265358979f;

// ---------------------------------------------------------------------
// Estado da camera. Segue a mesma parametrizacao do codigo legado:
// azimute, elevacao e raio orbital, com o buraco negro na origem.
// ---------------------------------------------------------------------
static float camAzimuth = 0.0f;   // legado: camAnguloX
static float camElev    = 0.20f;  // legado: camAnguloY (comecava em 0.2)
static float camDist    = 13.0f;  // legado: camRaio

// Sensibilidade do mouse: mesmo valor do codigo legado (rad por pixel)
static const float MOUSE_SENS = 0.005f;

static bool   mousePressed = false;
static double lastMouseX = 0.0, lastMouseY = 0.0;

// ---------------------------------------------------------------------
// Parametros de simulacao
// ---------------------------------------------------------------------
static float rs        = 0.5f;   // raio de Schwarzschild (Rs = 2M)
// 1 = heuristica legada (1/r^2), 2 = RK4 (verdade de referencia),
// 3 = lente fina de Einstein, 4 = lente fina calibrada
static int   activeShader = 1;
static int   outMode      = 0;  // 0 = cor; 1 = direcao no ceu (mapa de erro)

static int   skyMode   = 0;      // 0 = estrelas, 1 = grade lat/long
static bool  diskOn    = true;
static float diskInner = 3.0f;   // em Rs. ISCO = 6M = 3 Rs
static float diskOuter = 16.0f;  // em Rs -- disco largo, como o legado
static float diskSpin  = 3.0f;
static float diskBright= 2.2f;
static float dopplerK  = 0.85f;  // valor do codigo legado
static float exposure  = 1.4f;

static float heurK     = 1.8f;   // forca da heuristica (ver abaixo)
static float stepPhi   = 0.01f;  // passo do RK4, em radianos
static int   maxSteps  = 1600;
static int   spp       = 1;      // amostras por pixel por eixo (1..3)
static bool  legacySplit = false; // reproduz o fatiamento em 2 passagens

static bool  vsyncOn   = true;

static int fbWidth  = 1024;
static int fbHeight = 768;

static bool reloadRequested = false;

// Captura offscreen (--foto). Com fotoPath preenchido o programa
// renderiza um unico quadro, grava e sai -- sem abrir janela visivel.
static std::string fotoPath;
static int    fotoW    = 1024;
static int    fotoH    = 768;
static double fotoTime = 3.0;   // instante fixo, para a figura ser reprodutivel

// Benchmark offscreen (--bench N). Mede o tempo de N quadros num
// framebuffer proprio, sem v-sync, sem compositor e sem depender de a
// janela estar visivel -- e assim que saem os numeros do artigo.
static int benchFrames = 0;

// Mapa de erro (--erro PREFIXO) e perfil de GPU (--perfil ARQUIVO.dat).
static std::string erroPrefix;
static std::string perfilPath;

// ---------------------------------------------------------------------
// Localizacoes de uniforms, resolvidas uma vez por programa.
// glUniform* com location -1 e um no-op silencioso, entao os uniforms
// que existem so em um dos shaders podem ser enviados sempre.
// ---------------------------------------------------------------------
struct Uniforms {
    GLint resolution, camPos, camForward, camRight, camUp, tanHalfFov, aspect;
    GLint rs, time, exposure;
    GLint skyMode, diskOn, diskInner, diskOuter, diskSpin, dopplerK, diskBright;
    GLint heurMass, heurEH, legacySplit;
    GLint stepPhi, maxSteps, spp;
    GLint outMode, lensModel;
};

struct Program {
    GLuint id = 0;
    Uniforms u{};
};

static void cacheUniforms(Program& p) {
    Uniforms& u = p.u;
    u.resolution = glGetUniformLocation(p.id, "uResolution");
    u.camPos     = glGetUniformLocation(p.id, "uCamPos");
    u.camForward = glGetUniformLocation(p.id, "uCamForward");
    u.camRight   = glGetUniformLocation(p.id, "uCamRight");
    u.camUp      = glGetUniformLocation(p.id, "uCamUp");
    u.tanHalfFov = glGetUniformLocation(p.id, "uTanHalfFov");
    u.aspect     = glGetUniformLocation(p.id, "uAspect");
    u.rs         = glGetUniformLocation(p.id, "uRs");
    u.time       = glGetUniformLocation(p.id, "uTime");
    u.exposure   = glGetUniformLocation(p.id, "uExposure");
    u.skyMode    = glGetUniformLocation(p.id, "uSkyMode");
    u.diskOn     = glGetUniformLocation(p.id, "uDiskOn");
    u.diskInner  = glGetUniformLocation(p.id, "uDiskInner");
    u.diskOuter  = glGetUniformLocation(p.id, "uDiskOuter");
    u.diskSpin   = glGetUniformLocation(p.id, "uDiskSpin");
    u.dopplerK   = glGetUniformLocation(p.id, "uDopplerK");
    u.diskBright = glGetUniformLocation(p.id, "uDiskBright");
    u.heurMass   = glGetUniformLocation(p.id, "uHeurMass");
    u.heurEH     = glGetUniformLocation(p.id, "uHeurEH");
    u.legacySplit= glGetUniformLocation(p.id, "uLegacySplit");
    u.spp        = glGetUniformLocation(p.id, "uSPP");
    u.stepPhi    = glGetUniformLocation(p.id, "uStepPhi");
    u.maxSteps   = glGetUniformLocation(p.id, "uMaxSteps");
    u.outMode    = glGetUniformLocation(p.id, "uOutMode");
    u.lensModel  = glGetUniformLocation(p.id, "uLensModel");
}

static Program progHeuristic, progGeodesic, progLens;

static const Program& programa(int modelo) {
    if (modelo == 2) return progGeodesic;
    if (modelo >= 3) return progLens;
    return progHeuristic;
}

static const char* nomeModelo(int modelo) {
    switch (modelo) {
        case 2:  return "RK4 (referencia)";
        case 3:  return "lente de Einstein";
        case 4:  return "lente calibrada";
        default: return "legado 1/r^2";
    }
}

static void apagaProgramas() {
    if (progHeuristic.id) glDeleteProgram(progHeuristic.id);
    if (progGeodesic.id)  glDeleteProgram(progGeodesic.id);
    if (progLens.id)      glDeleteProgram(progLens.id);
    progHeuristic.id = progGeodesic.id = progLens.id = 0;
}

// ---------------------------------------------------------------------
// Raio APARENTE da sombra, em unidades de NDC vertical.
//
// A sombra de Schwarzschild vista de r0 tem raio angular
//     sin(theta) = b_c / r0 ,   b_c = 3*sqrt(3)*M = 2.598*Rs
// e um angulo theta cai na tela em  tan(theta)/tan(fov/2)  unidades de
// NDC. Isto amarra as constantes da heuristica -- que no codigo legado
// eram numeros fixos em espaco de tela (mass = 0.009, eventHorizon =
// 0.1) -- ao tamanho real do buraco negro na imagem. Sem isso a
// heuristica se desmancha assim que voce muda o zoom ou o Rs.
// ---------------------------------------------------------------------
static float shadowRadiusNDC(float tanHalfFov) {
    // Observador ESTATICO (Synge): sin(psi_sombra) = (b_c/r0) sqrt(1 - Rs/r0)
    const float bCrit = 2.59807621f * rs;
    float s = bCrit / std::max(camDist, 1e-4f)
            * std::sqrt(std::max(1.0f - rs / std::max(camDist, 1e-4f), 0.0f));
    s = std::min(s, 0.999f);
    float tanTheta = s / std::sqrt(1.0f - s * s);
    return tanTheta / tanHalfFov;
}

static void printHelp() {
    std::cout <<
    "\n"
    "=====================  CONTROLES  =====================\n"
    " MOUSE\n"
    "   arrastar com botao esquerdo ... orbitar a camera\n"
    "   roda do mouse ................. zoom (aproxima/afasta)\n"
    "\n"
    " TECLADO\n"
    "   1 ............. heuristica legada (1/r^2 em espaco de tela)\n"
    "   2 ............. RK4 (verdade de referencia)\n"
    "   3 / 4 ......... lente fina de Einstein / lente fina CALIBRADA\n"
    "   setas <- -> ... orbitar        setas cima/baixo ... zoom\n"
    "   = / - ......... raio de Schwarzschild (Rs)\n"
    "   K ............. amostras por pixel: 1x1 -> 2x2 -> 3x3\n"
    "   J ............. modo legado do disco (mostra a falha de juncao)\n"
    "   G ............. ceu: estrelas <-> GRADE lat/long\n"
    "                   (a grade e a melhor forma de VER a distorcao)\n"
    "   D ............. liga/desliga o disco de acrecao\n"
    "   Z / X ......... raio EXTERNO do disco\n"
    "   C / V ......... raio INTERNO do disco (ISCO)\n"
    "   [ / ] ......... forca da distorcao heuristica\n"
    "   N / M ......... passo do RK4 em phi (so no geodesico)\n"
    "   , / . ......... exposicao\n"
    "   B ............. liga/desliga v-sync (DESLIGUE para medir FPS)\n"
    "   P ............. imprime uma linha de benchmark no terminal\n"
    "   R ............. volta tudo ao padrao\n"
    "   F5 ............ recarrega os shaders do disco\n"
    "   H ............. mostra esta ajuda\n"
    "   ESC ........... sair\n"
    "=======================================================\n\n";
}

static void resetParams() {
    camAzimuth = 0.0f; camElev = 0.20f; camDist = 13.0f;
    rs = 0.5f;
    diskOn = true; diskInner = 3.0f; diskOuter = 16.0f;
    diskSpin = 3.0f; diskBright = 2.2f; dopplerK = 0.85f;
    exposure = 1.4f;
    heurK = 1.8f; stepPhi = 0.01f; maxSteps = 1600;
    spp = 1; legacySplit = false;
}

// ---------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------
static void framebufferSizeCallback(GLFWwindow*, int w, int h) {
    fbWidth  = (w > 0) ? w : 1;
    fbHeight = (h > 0) ? h : 1;
    glViewport(0, 0, fbWidth, fbHeight);
}

static void mouseButtonCallback(GLFWwindow* win, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    if (action == GLFW_PRESS) {
        mousePressed = true;
        glfwGetCursorPos(win, &lastMouseX, &lastMouseY);
    } else if (action == GLFW_RELEASE) {
        mousePressed = false;
    }
}

static void cursorPosCallback(GLFWwindow*, double x, double y) {
    if (!mousePressed) { lastMouseX = x; lastMouseY = y; return; }

    float dx = float(x - lastMouseX);
    float dy = float(y - lastMouseY);
    lastMouseX = x;
    lastMouseY = y;

    // Mesmos sinais e mesma sensibilidade do codigo legado
    camAzimuth += dx * MOUSE_SENS;
    camElev    += dy * MOUSE_SENS;

    // O legado nao limitava a elevacao, o que deixava a camera virar de
    // cabeca para baixo ao passar do polo (o vetor "up" degenerava).
    camElev = std::clamp(camElev, -1.45f, 1.45f);
}

static void scrollCallback(GLFWwindow*, double, double dy) {
    camDist *= std::pow(0.90f, float(dy));
    float minDist = std::max(1.2f, 3.0f * rs);
    camDist = std::clamp(camDist, minDist, 500.0f);
}

static void keyCallback(GLFWwindow* win, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    switch (key) {
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(win, GLFW_TRUE); break;
        case GLFW_KEY_1: activeShader = 1; std::cout << "[shader] heuristico\n"; break;
        case GLFW_KEY_2: activeShader = 2; std::cout << "[shader] geodesico (RK4)\n"; break;
        case GLFW_KEY_3: activeShader = 3; std::cout << "[shader] lente fina de Einstein\n"; break;
        case GLFW_KEY_4: activeShader = 4; std::cout << "[shader] lente fina calibrada\n"; break;
        case GLFW_KEY_D:
            diskOn = !diskOn;
            std::cout << "[disco] " << (diskOn ? "ligado" : "desligado") << "\n";
            break;
        case GLFW_KEY_K:
            spp = (spp % 3) + 1;
            std::cout << "[amostras] " << spp << "x" << spp << " = "
                      << (spp * spp) << " por pixel\n";
            break;
        case GLFW_KEY_J:
            legacySplit = !legacySplit;
            std::cout << "[juncao] " << (legacySplit
                ? "MODO LEGADO: disco em 2 passagens (mostra a falha de juncao)"
                : "passagem unica: sem costura") << "\n";
            break;
        case GLFW_KEY_G:
            skyMode = 1 - skyMode;
            std::cout << "[ceu] " << (skyMode == 1 ? "grade lat/long (a distorcao fica legivel)"
                                                   : "campo de estrelas") << "\n";
            break;
        case GLFW_KEY_B:
            vsyncOn = !vsyncOn;
            glfwSwapInterval(vsyncOn ? 1 : 0);
            std::cout << "[v-sync] " << (vsyncOn ? "ligado" : "DESLIGADO (bom para medir FPS)") << "\n";
            break;
        case GLFW_KEY_R: resetParams(); std::cout << "[reset] parametros padrao\n"; break;
        case GLFW_KEY_H: printHelp(); break;
        case GLFW_KEY_F5: reloadRequested = true; break;
        case GLFW_KEY_P: {
            float tanHalfFov = std::tan(PI / 180.0f * 22.5f);
            float rSh = shadowRadiusNDC(tanHalfFov);
            std::printf(
                "[bench] shader=%s res=%dx%d vsync=%s Rs=%.3f cam(dist=%.2f elev=%.1fdeg azim=%.1fdeg) "
                "disco=%s[%.1f..%.1f Rs] spp=%dx%d sombraNDC=%.3f heurMass=%.5f dphi=%.4f maxSteps=%d\n",
                nomeModelo(activeShader),
                fbWidth, fbHeight, (vsyncOn ? "on" : "off"),
                rs, camDist, camElev * 180.0f / PI, camAzimuth * 180.0f / PI,
                (diskOn ? "on" : "off"), diskInner, diskOuter, spp, spp,
                rSh, heurK * rSh * rSh * rSh * 0.5f, stepPhi, maxSteps);
            std::fflush(stdout);
            break;
        }
        default: break;
    }
}

// Teclas de ajuste continuo (mantidas pressionadas). Escalonadas por dt
// para que a velocidade nao dependa do FPS -- com v-sync desligado o
// codigo legado ficava incontrolavel.
static void processContinuousInput(GLFWwindow* w, float dt) {
    auto down = [&](int k) { return glfwGetKey(w, k) == GLFW_PRESS; };

    if (down(GLFW_KEY_LEFT))  camAzimuth -= 1.2f * dt;
    if (down(GLFW_KEY_RIGHT)) camAzimuth += 1.2f * dt;
    if (down(GLFW_KEY_UP))    camDist    -= 3.0f * dt;
    if (down(GLFW_KEY_DOWN))  camDist    += 3.0f * dt;

    if (down(GLFW_KEY_EQUAL)) rs += 0.30f * dt;
    if (down(GLFW_KEY_MINUS)) rs -= 0.30f * dt;
    rs = std::clamp(rs, 0.05f, 3.0f);

    camDist = std::clamp(camDist, std::max(1.2f, 3.0f * rs), 500.0f);

    if (down(GLFW_KEY_X)) diskOuter += 6.0f * dt;
    if (down(GLFW_KEY_Z)) diskOuter -= 6.0f * dt;
    if (down(GLFW_KEY_V)) diskInner += 3.0f * dt;
    if (down(GLFW_KEY_C)) diskInner -= 3.0f * dt;
    diskInner = std::clamp(diskInner, 1.05f, 30.0f);
    diskOuter = std::clamp(diskOuter, diskInner + 0.5f, 60.0f);

    if (down(GLFW_KEY_RIGHT_BRACKET)) heurK += 0.5f * dt;
    if (down(GLFW_KEY_LEFT_BRACKET))  heurK -= 0.5f * dt;
    heurK = std::clamp(heurK, 0.0f, 4.0f);

    if (down(GLFW_KEY_M)) stepPhi += 0.05f * dt;
    if (down(GLFW_KEY_N)) stepPhi -= 0.05f * dt;
    stepPhi = std::clamp(stepPhi, 0.002f, 0.30f);

    if (down(GLFW_KEY_PERIOD)) exposure += 1.5f * dt;
    if (down(GLFW_KEY_COMMA))  exposure -= 1.5f * dt;
    exposure = std::clamp(exposure, 0.10f, 8.0f);
}

// ---------------------------------------------------------------------
// Carga / recarga dos dois programas
// ---------------------------------------------------------------------
static bool loadAllShaders(const std::string& dir) {
    GLuint h = loadProgram(dir, "fullscreen.vert", "heuristic.frag");
    GLuint g = loadProgram(dir, "fullscreen.vert", "geodesic.frag");
    GLuint l = loadProgram(dir, "fullscreen.vert", "lens.frag");

    if (!h || !g || !l) {
        // Falhou: descarta o que compilou e mantem o que ja estava rodando
        if (h) glDeleteProgram(h);
        if (g) glDeleteProgram(g);
        if (l) glDeleteProgram(l);
        std::cerr << "[shader] recarga abortada -- mantendo os shaders anteriores\n";
        return false;
    }

    apagaProgramas();
    progHeuristic.id = h;
    progGeodesic.id  = g;
    progLens.id      = l;
    cacheUniforms(progHeuristic);
    cacheUniforms(progGeodesic);
    cacheUniforms(progLens);

    // Uniform nao usado pelo shader e removido pelo compilador e devolve
    // location -1. Isso e um no-op silencioso no glUniform, o que ja
    // custou uma sessao de depuracao: uResolution passou a ser usada
    // (supersampling) e, se por algum motivo vier -1, px = 2/0 gera NaN
    // e a tela inteira fica branca.
    std::printf("[uniforms] heuristico: uResolution=%d uSPP=%d | geodesico: uResolution=%d uSPP=%d\n",
                progHeuristic.u.resolution, progHeuristic.u.spp,
                progGeodesic.u.resolution,  progGeodesic.u.spp);
    std::fflush(stdout);
    return true;
}

static GLuint gVao = 0, gVbo = 0;

// ---------------------------------------------------------------------
// Desenha um quadro completo no framebuffer que estiver ligado.
// Extraida do laco principal para que a captura offscreen (--foto)
// use exatamente o mesmo caminho de renderizacao da janela.
// ---------------------------------------------------------------------
static void renderFrame(const Program& p, double t, int w, int h) {
    // Camera orbital. Mesma parametrizacao do codigo legado:
    //   camX = R cos(elev) sin(azim)
    //   camY = R sin(elev)
    //   camZ = R cos(elev) cos(azim)
    const float ce = std::cos(camElev), se = std::sin(camElev);
    const float ca = std::cos(camAzimuth), sa = std::sin(camAzimuth);

    const float camPos[3] = { camDist * ce * sa, camDist * se, camDist * ce * ca };

    // forward = -normalize(camPos)  (a camera sempre olha para o BH)
    float fx = -camPos[0], fy = -camPos[1], fz = -camPos[2];
    const float flen = std::sqrt(fx*fx + fy*fy + fz*fz);
    fx /= flen; fy /= flen; fz /= flen;

    // "up" da orbita, igual ao gluLookAt do codigo legado
    const float upX = -se * sa, upY = ce, upZ = -se * ca;

    // right = normalize(forward x up)
    float rx = fy * upZ - fz * upY;
    float ry = fz * upX - fx * upZ;
    float rz = fx * upY - fy * upX;
    const float rlen = std::sqrt(rx*rx + ry*ry + rz*rz);
    rx /= rlen; ry /= rlen; rz /= rlen;

    // up ortonormal = right x forward
    const float ux = ry * fz - rz * fy;
    const float uy = rz * fx - rx * fz;
    const float uz = rx * fy - ry * fx;

    // FOV vertical de 45 graus (igual ao gluPerspective(45,...) legado)
    const float tanHalfFov = std::tan(PI / 180.0f * 22.5f);
    const float aspect = float(w) / float(h);

    const float rSh      = shadowRadiusNDC(tanHalfFov);
    const float heurMass = heurK * rSh * rSh * rSh * 0.5f;

    glUseProgram(p.id);

    glUniform2f(p.u.resolution, float(w), float(h));
    glUniform3f(p.u.camPos,     camPos[0], camPos[1], camPos[2]);
    glUniform3f(p.u.camForward, fx, fy, fz);
    glUniform3f(p.u.camRight,   rx, ry, rz);
    glUniform3f(p.u.camUp,      ux, uy, uz);
    glUniform1f(p.u.tanHalfFov, tanHalfFov);
    glUniform1f(p.u.aspect,     aspect);
    glUniform1f(p.u.rs,         rs);
    glUniform1f(p.u.time,       float(t));
    glUniform1f(p.u.exposure,   exposure);

    glUniform1i(p.u.skyMode,    skyMode);
    glUniform1i(p.u.diskOn,     diskOn ? 1 : 0);
    glUniform1f(p.u.diskInner,  diskInner);
    glUniform1f(p.u.diskOuter,  diskOuter);
    glUniform1f(p.u.diskSpin,   diskSpin);
    glUniform1f(p.u.dopplerK,   dopplerK);
    glUniform1f(p.u.diskBright, diskBright);

    glUniform1f(p.u.heurMass,   heurMass);
    glUniform1f(p.u.heurEH,     rSh);
    glUniform1i(p.u.legacySplit, legacySplit ? 1 : 0);
    glUniform1i(p.u.spp,        spp);

    glUniform1f(p.u.stepPhi,    stepPhi);
    glUniform1i(p.u.maxSteps,   maxSteps);
    glUniform1i(p.u.outMode,    outMode);
    glUniform1i(p.u.lensModel,  activeShader == 4 ? 1 : 0);

    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(gVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

// ---------------------------------------------------------------------
// Captura offscreen: renderiza um quadro num framebuffer proprio e
// grava um PPM (P6). Nao depende da janela estar visivel -- e o unico
// jeito confiavel de gerar as figuras do artigo sempre no mesmo estado,
// e funciona mesmo com a tela bloqueada ou em suspensao.
// ---------------------------------------------------------------------
static bool salvaFoto(const Program& p, const std::string& path,
                      int w, int h, double t) {
    GLuint fbo = 0, tex = 0;
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &tex);

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, tex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[foto] framebuffer incompleto\n";
        return false;
    }

    renderFrame(p, t, w, h);
    glFinish();

    std::vector<unsigned char> px(size_t(w) * size_t(h) * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);

    // O OpenGL entrega a primeira linha EMBAIXO; PNG e PPM esperam de
    // cima. Inverte no lugar, trocando linha por linha.
    const size_t passo = size_t(w) * 4;
    std::vector<unsigned char> tmp(passo);
    for (int y = 0; y < h / 2; y++) {
        unsigned char* la = px.data() + size_t(y) * passo;
        unsigned char* lb = px.data() + size_t(h - 1 - y) * passo;
        std::memcpy(tmp.data(), la, passo);
        std::memcpy(la, lb, passo);
        std::memcpy(lb, tmp.data(), passo);
    }

    bool ok;
    const bool ehPng = path.size() > 4 &&
        path.compare(path.size() - 4, 4, ".png") == 0;

    if (ehPng) {
        ok = png_simples::grava(path, w, h, px.data());
    } else {
        // PPM (P6), caso voce peca outra extensao
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) { std::cerr << "[foto] nao consegui abrir " << path << "\n"; return false; }
        std::fprintf(f, "P6\n%d %d\n255\n", w, h);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                const unsigned char* q = &px[(size_t(y) * size_t(w) + size_t(x)) * 4];
                std::fputc(q[0], f); std::fputc(q[1], f); std::fputc(q[2], f);
            }
        ok = true;
        std::fclose(f);
    }

    if (!ok) { std::cerr << "[foto] falha ao gravar " << path << "\n"; return false; }
    std::printf("[foto] %s (%dx%d, t=%.2f s)\n", path.c_str(), w, h, t);
    return true;
}

// ---------------------------------------------------------------------
// Mede o custo de renderizacao num framebuffer offscreen.
//
// Preferivel ao FPS da janela: sem v-sync, sem compositor de janelas e
// sem depender de a tela estar acordada (uma tela em suspensao trava a
// GPU em 30 Hz e falseia a medida). Faz aquecimento antes de medir,
// porque o primeiro quadro paga a compilacao final do shader no driver.
// ---------------------------------------------------------------------
static void benchmark(const Program& p, int n, int w, int h,
                      const char* nome) {
    GLuint fbo = 0, tex = 0;
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[bench] framebuffer incompleto\n";
        return;
    }

    for (int i = 0; i < 20; i++) renderFrame(p, 3.0, w, h);   // aquecimento
    glFinish();

    const double t0 = glfwGetTime();
    for (int i = 0; i < n; i++) renderFrame(p, 3.0 + 0.01 * double(i), w, h);
    glFinish();
    const double dt = glfwGetTime() - t0;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);

    std::printf("[bench] %s | %dx%d | spp=%dx%d | disco=%s | dphi=%.4f maxSteps=%d\n",
                nome, w, h, spp, spp, (diskOn ? "on" : "off"), stepPhi, maxSteps);
    std::printf("[bench] %d quadros em %.3f s  ->  %.2f ms/quadro  =  %.1f FPS\n",
                n, dt, 1000.0 * dt / double(n), double(n) / dt);
    std::fflush(stdout);
}

// =====================================================================
//  MAPA DE ERRO VISUAL
//
//  Subtrair duas imagens RGB de um campo de estrelas mede pouco: uma
//  estrela deslocada de um pixel da erro maximo, e uma estrela no lugar
//  certo mas um pouco mais fraca da erro quase nulo. O que a heuristica
//  pode errar e a DIRECAO de onde cada pixel busca o ceu -- entao e isso
//  que comparamos. Cada programa roda em modo diagnostico (uOutMode = 1)
//  e grava, num framebuffer de ponto flutuante, a direcao final no ceu
//  do raio que passa pelo centro de cada pixel (e se ele foi capturado).
//  O erro e o angulo entre essa direcao e a do RK4.
//
//  O disco e desligado aqui: ele esta a distancia finita, e o que se
//  mede e a lente sobre o ceu distante.
// =====================================================================
struct DirImg { std::vector<float> px; int w = 0, h = 0; };

static DirImg renderDirecoes(int modelo, int w, int h) {
    GLuint fbo = 0, tex = 0;
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    DirImg img; img.w = w; img.h = h;
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        const int modeloAntes = activeShader;
        activeShader = modelo;
        renderFrame(programa(modelo), 3.0, w, h);
        activeShader = modeloAntes;
        glFinish();
        img.px.resize(size_t(w) * size_t(h) * 4);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, img.px.data());
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    return img;
}

// Rampa sequencial de UM tom (azul, claro -> escuro): erro pequeno fica
// claro, erro grande escuro. Mesmos 13 passos da legenda desenhada pelo
// pgfplots no artigo (docs/artigo/main.tex), para as cores baterem.
static void corRampa(double t, unsigned char* rgb) {
    static const unsigned char R[13][3] = {
        {0xcd,0xe2,0xfb},{0xb7,0xd3,0xf6},{0x9e,0xc5,0xf4},{0x86,0xb6,0xef},
        {0x6d,0xa7,0xec},{0x55,0x98,0xe7},{0x39,0x87,0xe5},{0x2a,0x78,0xd6},
        {0x25,0x6a,0xbf},{0x1c,0x5c,0xab},{0x18,0x4f,0x95},{0x10,0x42,0x81},
        {0x0d,0x36,0x6b}};
    t = std::min(std::max(t, 0.0), 1.0) * 12.0;
    int i = std::min(int(t), 11);
    double f = t - i;
    for (int c = 0; c < 3; c++)
        rgb[c] = (unsigned char)std::lround(R[i][c] + f * (R[i+1][c] - R[i][c]));
}

static bool mapaDeErro(const std::string& prefixo, int w, int h) {
    diskOn = false; outMode = 1; spp = 1;

    const DirImg gt = renderDirecoes(2, w, h);
    if (gt.px.empty()) { std::cerr << "[erro] framebuffer float indisponivel\n"; return false; }

    // Escala logaritmica compartilhada: 10^-3 a 10^2 graus
    const double LOG_MIN = -3.0, LOG_MAX = 2.0;

    std::string relatorio;
    char linha[512];
    std::snprintf(linha, sizeof(linha),
        "# mapa de erro angular contra o RK4 | %dx%d | r0 = %.2f (%.1f Rs) | Rs = %.3f\n"
        "# modelo            media     mediana   p95       p99       max    >0.1g   >1g   >10g  diverg.\n",
        w, h, camDist, camDist / rs, rs);
    relatorio += linha;

    const int modelos[] = {1, 3, 4};
    const char* sufixo[] = {"legado", "einstein", "calibrada"};

    for (int k = 0; k < 3; k++) {
        const DirImg hi = renderDirecoes(modelos[k], w, h);
        std::vector<unsigned char> rgba(size_t(w) * size_t(h) * 4, 255);
        std::vector<double> erros;
        erros.reserve(size_t(w) * size_t(h));
        size_t diverg = 0, foraSombra = 0;

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                const size_t i = (size_t(y) * size_t(w) + size_t(x)) * 4;
                // PNG e de cima para baixo; o framebuffer, de baixo para cima
                unsigned char* o = &rgba[(size_t(h - 1 - y) * size_t(w) + size_t(x)) * 4];
                const bool gEsc = gt.px[i + 3] > 0.5f;
                const bool hEsc = hi.px[i + 3] > 0.5f;

                if (!gEsc && !hEsc) {                 // os dois na sombra
                    o[0] = 0x89; o[1] = 0x87; o[2] = 0x81;
                    continue;
                }
                foraSombra++;
                if (gEsc != hEsc) {                   // um ve ceu, o outro a sombra
                    diverg++;
                    corRampa(1.0, o);
                    continue;
                }
                const double gx = gt.px[i], gy = gt.px[i+1], gz = gt.px[i+2];
                const double hx = hi.px[i], hy = hi.px[i+1], hz = hi.px[i+2];
                const double cx = gy*hz - gz*hy, cy = gz*hx - gx*hz, cz = gx*hy - gy*hx;
                const double ang = std::atan2(std::sqrt(cx*cx + cy*cy + cz*cz),
                                              gx*hx + gy*hy + gz*hz) * 180.0 / 3.14159265358979;
                erros.push_back(ang);
                const double l = std::log10(std::max(ang, 1e-6));
                corRampa((l - LOG_MIN) / (LOG_MAX - LOG_MIN), o);
            }
        }

        const std::string arq = prefixo + "_" + sufixo[k] + ".png";
        png_simples::grava(arq, w, h, rgba.data());

        std::sort(erros.begin(), erros.end());
        auto pct = [&](double q) {
            return erros.empty() ? 0.0 : erros[std::min(erros.size() - 1, size_t(q * erros.size()))];
        };
        double soma = 0; size_t a01 = 0, a1 = 0, a10 = 0;
        for (double e : erros) { soma += e; a01 += e > 0.1; a1 += e > 1.0; a10 += e > 10.0; }
        const double nE = std::max<double>(erros.size(), 1);
        std::snprintf(linha, sizeof(linha),
            "%-16s %9.4f %9.4f %9.4f %9.4f %8.2f %6.2f%% %5.2f%% %5.2f%% %6.3f%%\n",
            sufixo[k], soma / nE, pct(0.5), pct(0.95), pct(0.99),
            erros.empty() ? 0.0 : erros.back(),
            100.0 * a01 / nE, 100.0 * a1 / nE, 100.0 * a10 / nE,
            100.0 * double(diverg) / std::max<double>(foraSombra, 1));
        relatorio += linha;
        std::printf("[erro] %s\n", arq.c_str());
    }

    std::printf("%s", relatorio.c_str());
    FILE* f = std::fopen((prefixo + "_estatisticas.txt").c_str(), "w");
    if (f) { std::fputs(relatorio.c_str(), f); std::fclose(f); }
    return true;
}

// =====================================================================
//  PERFIL DE GPU POR RESOLUCAO
//
//  Tempo de GPU medido com timer queries (GL_TIME_ELAPSED, nucleo do
//  OpenGL 3.3), em framebuffer fora da tela, sem v-sync. Para cada
//  modelo e resolucao: 5 quadros de aquecimento, uma sonda para estimar
//  o custo, e entao N quadros cronometrados (N ~ 1,5 s de GPU, entre 15
//  e 1500). Reporta a MEDIANA, que ignora os soluços do sistema.
// =====================================================================
static double gpuMsMediana(int modelo, int w, int h) {
    const int modeloAntes = activeShader;
    activeShader = modelo;
    const Program& p = programa(modelo);

    for (int i = 0; i < 5; i++) renderFrame(p, 3.0, w, h);
    glFinish();

    GLuint sonda = 0;
    glGenQueries(1, &sonda);
    glBeginQuery(GL_TIME_ELAPSED, sonda);
    renderFrame(p, 3.0, w, h);
    glEndQuery(GL_TIME_ELAPSED);
    GLuint64 ns = 0;
    glGetQueryObjectui64v(sonda, GL_QUERY_RESULT, &ns);
    glDeleteQueries(1, &sonda);

    const double msSonda = std::max(double(ns) * 1e-6, 1e-3);
    const int n = int(std::min(std::max(1500.0 / msSonda, 15.0), 1500.0));

    std::vector<GLuint> qs(size_t(n), 0);
    glGenQueries(n, qs.data());
    for (int i = 0; i < n; i++) {
        glBeginQuery(GL_TIME_ELAPSED, qs[size_t(i)]);
        renderFrame(p, 3.0 + 0.01 * i, w, h);
        glEndQuery(GL_TIME_ELAPSED);
    }
    std::vector<double> ms(static_cast<size_t>(n));
    for (int i = 0; i < n; i++) {
        glGetQueryObjectui64v(qs[size_t(i)], GL_QUERY_RESULT, &ns);
        ms[size_t(i)] = double(ns) * 1e-6;
    }
    glDeleteQueries(n, qs.data());
    activeShader = modeloAntes;

    std::sort(ms.begin(), ms.end());
    return ms[ms.size() / 2];
}

static bool perfilGPU(const std::string& caminho) {
    outMode = 0;
    static const int res[][2] = {{640, 480}, {1280, 720}, {1920, 1080},
                                 {2560, 1440}, {3840, 2160}};
    const int modelos[] = {2, 4, 3, 1};     // RK4, calibrada, Einstein, legado

    FILE* f = std::fopen(caminho.c_str(), "w");
    if (!f) { std::cerr << "[perfil] nao consegui abrir " << caminho << "\n"; return false; }
    std::fprintf(f, "# tempo de GPU por quadro (ms, mediana), spp = %d, disco = %s\n",
                 spp * spp, diskOn ? "on" : "off");
    std::fprintf(f, "mpx largura altura rk4 calibrada einstein legado\n");
    std::printf("%-10s %6s | %10s %10s %10s %10s   (ms de GPU)\n",
                "resolucao", "Mpx", "RK4", "calibrada", "Einstein", "legado");

    for (const auto& r : res) {
        const int w = r[0], h = r[1];
        GLuint fbo = 0, tex = 0;
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

        double t[4];
        for (int k = 0; k < 4; k++) t[k] = gpuMsMediana(modelos[k], w, h);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &tex);

        const double mpx = double(w) * double(h) / 1e6;
        std::fprintf(f, "%.4f %d %d %.4f %.4f %.4f %.4f\n", mpx, w, h, t[0], t[1], t[2], t[3]);
        std::printf("%4dx%-5d %6.2f | %10.3f %10.4f %10.4f %10.4f\n", w, h, mpx, t[0], t[1], t[2], t[3]);
        std::fflush(stdout);
    }
    std::fclose(f);
    std::printf("[perfil] %s\n", caminho.c_str());
    return true;
}

// Uso:  buraco_negro [1|2] [--grade] [--legado] [--spp2|--spp3]
//                   [--sem-disco] [--sem-vsync]
//                   [--foto arq.ppm] [--res 1600x1200] [--tempo 3.0]
//                   [--bench 200] [--dphi 0.01] [--passos 1600] [--forca 1.8]
//   1 = comeca no shader heuristico (padrao)
//   2 = comeca no shader geodesico
// Serve para gerar as figuras do artigo sempre no mesmo estado, sem
// depender de apertar teclas depois de abrir.
static void parseArgs(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        if      (a == "1")           activeShader = 1;
        else if (a == "2")           activeShader = 2;
        else if (a == "3")           activeShader = 3;
        else if (a == "4")           activeShader = 4;
        else if (a == "--grade")     skyMode = 1;
        else if (a == "--legado")    legacySplit = true;
        else if (a == "--spp2")      spp = 2;
        else if (a == "--spp3")      spp = 3;
        else if (a == "--sem-disco") diskOn = false;
        else if (a == "--sem-vsync") vsyncOn = false;
        else if (a == "--foto" && i + 1 < argc)  fotoPath = argv[++i];
        else if (a == "--bench" && i + 1 < argc) benchFrames = std::atoi(argv[++i]);
        else if (a == "--erro" && i + 1 < argc)   erroPrefix = argv[++i];
        else if (a == "--perfil" && i + 1 < argc) perfilPath = argv[++i];
        else if (a == "--dist" && i + 1 < argc)   camDist = float(std::atof(argv[++i]));
        else if (a == "--dphi" && i + 1 < argc)  stepPhi = float(std::atof(argv[++i]));
        else if (a == "--passos" && i + 1 < argc) maxSteps = std::atoi(argv[++i]);
        else if (a == "--forca" && i + 1 < argc)  heurK = float(std::atof(argv[++i]));
        else if (a == "--tempo" && i + 1 < argc) fotoTime = std::atof(argv[++i]);
        else if (a == "--res" && i + 1 < argc) {
            const std::string r = argv[++i];
            const size_t xp = r.find('x');
            if (xp != std::string::npos) {
                fotoW = std::atoi(r.substr(0, xp).c_str());
                fotoH = std::atoi(r.substr(xp + 1).c_str());
            }
        }
        else std::cerr << "[args] ignorando argumento desconhecido: " << a << "\n";
    }
}

int main(int argc, char** argv) {
    parseArgs(argc, argv);

    if (!glfwInit()) {
        std::cerr << "Falha ao iniciar GLFW\n";
        return -1;
    }

    // Contexto OpenGL 3.3 CORE: pipeline programavel, sem glBegin/glEnd
    // e sem a pilha de matrizes do OpenGL legado.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // No modo --foto a janela existe so para carregar o contexto OpenGL:
    // mantendo-a invisivel, a captura funciona com a tela bloqueada,
    // em suspensao ou por acesso remoto.
    if (!fotoPath.empty() || benchFrames > 0 || !erroPrefix.empty() || !perfilPath.empty())
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(fbWidth, fbHeight,
                                          "Buraco Negro - OpenGL Moderno",
                                          nullptr, nullptr);
    if (!window) {
        std::cerr << "Falha ao criar janela GLFW\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(vsyncOn ? 1 : 0);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Falha ao iniciar GLAD\n";
        glfwTerminate();
        return -1;
    }

    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetKeyCallback(window, keyCallback);

    std::cout << "OpenGL " << glGetString(GL_VERSION)
              << " | " << glGetString(GL_RENDERER) << "\n";

    // -----------------------------------------------------------------
    // Quad de tela cheia: 2 triangulos cobrindo o NDC [-1,1]. Toda a
    // cena (estrelas + disco + distorcao) e calculada no fragment
    // shader, por pixel, como um ray tracer.
    // -----------------------------------------------------------------
    const float quadVerts[] = {
        -1.0f, -1.0f,   1.0f, -1.0f,   1.0f,  1.0f,
        -1.0f, -1.0f,   1.0f,  1.0f,  -1.0f,  1.0f
    };

    glGenVertexArrays(1, &gVao);
    glGenBuffers(1, &gVbo);
    glBindVertexArray(gVao);
    glBindBuffer(GL_ARRAY_BUFFER, gVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    const std::string shaderDir = findShaderDir();
    std::cout << "Shaders em: " << shaderDir << "\n";

    if (!loadAllShaders(shaderDir)) {
        std::cerr << "Nao foi possivel compilar os shaders iniciais. Abortando.\n";
        glfwTerminate();
        return -1;
    }

    if (!erroPrefix.empty() || !perfilPath.empty()) {
        bool ok = true;
        if (!erroPrefix.empty()) ok = mapaDeErro(erroPrefix, fotoW, fotoH) && ok;
        if (!perfilPath.empty()) ok = perfilGPU(perfilPath) && ok;
        glDeleteVertexArrays(1, &gVao);
        glDeleteBuffers(1, &gVbo);
        apagaProgramas();
        glfwTerminate();
        return ok ? 0 : -1;
    }

    if (benchFrames > 0) {
        const Program& p = programa(activeShader);
        benchmark(p, benchFrames, fotoW, fotoH,
                  nomeModelo(activeShader));
        glDeleteVertexArrays(1, &gVao);
        glDeleteBuffers(1, &gVbo);
        apagaProgramas();
        glfwTerminate();
        return 0;
    }

    if (!fotoPath.empty()) {
        const Program& p = programa(activeShader);
        const bool ok = salvaFoto(p, fotoPath, fotoW, fotoH, fotoTime);
        glDeleteVertexArrays(1, &gVao);
        glDeleteBuffers(1, &gVbo);
        apagaProgramas();
        glfwTerminate();
        return ok ? 0 : -1;
    }

    printHelp();

    double lastTime  = glfwGetTime();
    double titleTime = lastTime;
    int    frames    = 0;
    float  fps       = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = float(now - lastTime);
        lastTime = now;
        dt = std::min(dt, 0.1f);   // evita saltos gigantes apos um travamento

        if (reloadRequested) {
            reloadRequested = false;
            if (loadAllShaders(shaderDir))
                std::cout << "[shader] recarregados de " << shaderDir << "\n";
        }

        processContinuousInput(window, dt);

        const Program& p = programa(activeShader);
        renderFrame(p, now, fbWidth, fbHeight);

        glfwSwapBuffers(window);
        glfwPollEvents();

        // ---- FPS na barra de titulo ----
        frames++;
        if (now - titleTime >= 0.4) {
            fps = float(frames / (now - titleTime));
            frames = 0;
            titleTime = now;

            char title[256];
            std::snprintf(title, sizeof(title),
                "Buraco Negro | %s | %.1f FPS | %dx%d | %dspp | Rs=%.2f | r=%.1f | disco %s%s",
                nomeModelo(activeShader),
                fps, fbWidth, fbHeight, spp * spp, rs, camDist,
                (diskOn ? "on" : "off"),
                (vsyncOn ? " | vsync" : ""));
            glfwSetWindowTitle(window, title);
        }
    }

    glDeleteVertexArrays(1, &gVao);
    glDeleteBuffers(1, &gVbo);
    apagaProgramas();

    glfwTerminate();
    return 0;
}
