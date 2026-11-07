// =====================================================================
//  calibra_lente.cpp -- calibracao da heuristica de lente fina
//
//  1) TREINO. Calcula por RK4 o angulo de deflexao exato alpha(b) de um
//     foton que vem do infinito e volta ao infinito, para N parametros
//     de impacto b > b_c, e ajusta por minimos quadrados
//
//        alpha(x) = -ln(1 - x) + sum_k c_k * phi_k(x),   x = b_c / b,
//
//     O termo -ln(1-x) reproduz a divergencia de campo forte (Bozza
//     2002: alpha ~ -ln(b/b_c - 1) + b_bar); a base phi_k corrige o
//     resto. Ha tres variantes de base (argumento da linha de comando):
//
//        A  polinomio x, x^2, ..., x^K
//        B  idem + (1-x) ln(1-x), o termo sub-dominante de campo forte
//        C  como B, mas com c1 FIXO no limite de Einstein (4M/b)
//
//  2) TESTE. Com os coeficientes congelados, compara o mapa
//     angulo-na-imagem -> angulo-no-ceu das heuristicas com o mapa
//     exato visto por um OBSERVADOR ESTATICO a distancia finita r0 --
//     dado que o ajuste nunca viu (o treino e com observador no
//     infinito). Erro angular em graus.
//
//  Observador estatico (Synge 1966): o angulo local psi entre o raio e a
//  direcao do buraco negro se relaciona com b por
//        sin(psi) = (b / r0) * sqrt(1 - 2M/r0).
//
//  Unidades geometrizadas, G = c = M = 1.  Rs = 2, b_c = 3*sqrt(3).
// =====================================================================
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>

static const double PI = 3.14159265358979323846;
static const double M  = 1.0;
static const double BC = 3.0 * std::sqrt(3.0);

static void rk4(double& u, double& w, double h) {
    double k1u = w,              k1w = -u + 3*M*u*u;
    double ua = u + 0.5*h*k1u,   wa = w + 0.5*h*k1w;
    double k2u = wa,             k2w = -ua + 3*M*ua*ua;
    double ub = u + 0.5*h*k2u,   wb = w + 0.5*h*k2w;
    double k3u = wb,             k3w = -ub + 3*M*ub*ub;
    double uc = u + h*k3u,       wc = w + h*k3w;
    double k4u = wc,             k4w = -uc + 3*M*uc*uc;
    u += (h/6)*(k1u + 2*k2u + 2*k3u + k4u);
    w += (h/6)*(k1w + 2*k2w + 2*k3w + k4w);
}

// ---------------- deflexao, observador e fonte no infinito -----------
static double perielioU(double b) {
    double lo = 0.0, hi = 1.0 / (3.0 * M);
    for (int i = 0; i < 200; i++) {
        double mid = 0.5 * (lo + hi);
        if (mid*mid*(1.0 - 2.0*M*mid) - 1.0/(b*b) < 0.0) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}

static double alphaExato(double b, double h) {
    double u = perielioU(b), w = 0.0, phi = 0.0;
    for (long i = 0; i < 400000000L; i++) {
        double up = u, pp = phi;
        rk4(u, w, h); phi += h;
        if (u <= 0.0) return 2.0 * (pp + (up / (up - u)) * h) - PI;
    }
    return NAN;
}

// ---------------- impacto de um raio visto por observador estatico -----
static double bSynge(double psi, double r0) {
    return r0 * std::sin(psi) / std::sqrt(1.0 - 2.0 * M / r0);
}

// ---------------- mapa exato, observador estatico em r0 ----------------
// psi: angulo LOCAL entre o raio e a direcao do buraco negro.
// Devolve theta_s: angulo da direcao final no ceu a partir da mesma
// direcao (negativo = do outro lado). NAN se capturado.
static double thetaExato(double psi, double r0, double h) {
    const double b  = bSynge(psi, r0);
    const double u0 = 1.0 / r0;
    // w0 sai da lei de conservacao, com o sinal do sentido do raio:
    // psi < 90 graus -> aponta para dentro -> u cresce -> w > 0
    double w2 = 1.0/(b*b) - u0*u0 + 2.0*M*u0*u0*u0;
    double u = u0, w = (std::cos(psi) >= 0 ? 1.0 : -1.0) * std::sqrt(std::max(w2, 0.0));
    double phi = 0.0;
    const double uEsc = 1.0 / (2000.0 * r0);
    for (long i = 0; i < 400000000L; i++) {
        double up = u, pp = phi;
        rk4(u, w, h); phi += h;
        if (u > 1.0 / (2.0 * M)) return NAN;
        // theta_s = pi - phi_inf: angulo entre a direcao final (no
        // infinito) e a direcao camera -> buraco negro
        if (u <= 0.0) return PI - (pp + (up / (up - u)) * h);
        if (u < uEsc && w < 0.0) return PI - (phi + (PI - std::atan2(u, w)));
    }
    return NAN;
}

// ---------------- base do modelo ----------------
static char MODELO = 'B';
static int  GRAU   = 3;

static int nBase() { return GRAU + (MODELO == 'A' ? 0 : 1); }

static void base(double x, std::vector<double>& f) {
    f.clear();
    double xp = x;
    for (int k = 0; k < GRAU; k++) { f.push_back(xp); xp *= x; }
    if (MODELO != 'A') f.push_back((1.0 - x) * std::log(1.0 - x));
}

// minimos quadrados; se fixaC1, c1 e fixo e so o resto e ajustado
static std::vector<double> ajusta(const std::vector<double>& xs,
                                  const std::vector<double>& ys,
                                  bool fixaC1, double c1fixo) {
    const int K = nBase();
    const int k0 = fixaC1 ? 1 : 0;
    const int n = K - k0;
    std::vector<double> A(n*n, 0.0), r(n, 0.0), f;
    for (size_t i = 0; i < xs.size(); i++) {
        base(xs[i], f);
        double y = ys[i] - (fixaC1 ? c1fixo * f[0] : 0.0);
        for (int a = 0; a < n; a++) {
            r[a] += f[a+k0] * y;
            for (int bb = 0; bb < n; bb++) A[a*n+bb] += f[a+k0] * f[bb+k0];
        }
    }
    for (int c = 0; c < n; c++) {
        int p = c;
        for (int q = c+1; q < n; q++) if (std::fabs(A[q*n+c]) > std::fabs(A[p*n+c])) p = q;
        for (int j = 0; j < n; j++) std::swap(A[c*n+j], A[p*n+j]);
        std::swap(r[c], r[p]);
        for (int q = c+1; q < n; q++) {
            double m = A[q*n+c] / A[c*n+c];
            for (int j = c; j < n; j++) A[q*n+j] -= m * A[c*n+j];
            r[q] -= m * r[c];
        }
    }
    std::vector<double> s(n);
    for (int i = n-1; i >= 0; i--) {
        double v = r[i];
        for (int j = i+1; j < n; j++) v -= A[i*n+j] * s[j];
        s[i] = v / A[i*n+i];
    }
    std::vector<double> c;
    if (fixaC1) c.push_back(c1fixo);
    c.insert(c.end(), s.begin(), s.end());
    return c;
}

static double alphaModelo(double b, const std::vector<double>& c) {
    double x = BC / b;
    std::vector<double> f; base(x, f);
    double s = -std::log(1.0 - x);
    for (size_t k = 0; k < c.size(); k++) s += c[k] * f[k];
    return s;
}

// ---------------------------------------------------------------------
// Trecho do observador:  I(u0, b) = integral_0^u0 du / sqrt(1/b^2 - u^2 + 2Mu^3)
// e o angulo varrido entre o infinito e r0 = 1/u0. Com ele, a identidade
// EXATA para um raio que passa pelo perielio e
//        theta_s = I(u0, b) - alpha(b).
// ---------------------------------------------------------------------
// (a) forma analitica de 1a ordem em M
static double I1(double r0, double b) {
    double s  = std::min(b / r0, 0.999999);
    double c0 = std::sqrt(1.0 - s * s);
    return std::asin(s) - (M / b) * (1.0 / c0 + c0 - 2.0);
}
// (b) quadratura de Gauss-Legendre de 4 pontos em [0, u0]. O integrando
//     e suave ali enquanto o raio nao estiver quase tangente no
//     observador (b/r0 longe de 1), o que vale dentro do campo de visao
static double IGauss(double r0, double b) {
    static const double x[4] = {-0.8611363116, -0.3399810436, 0.3399810436, 0.8611363116};
    static const double wg[4] = { 0.3478548451,  0.6521451549, 0.6521451549, 0.3478548451};
    const double u0 = 1.0 / r0;
    double soma = 0.0;
    for (int i = 0; i < 4; i++) {
        double s = 0.5 * (x[i] + 1.0);          // s em [0,1]
        double u = u0 * s;
        double rad = 1.0/(b*b) - u*u + 2.0*M*u*u*u;
        soma += 0.5 * wg[i] * u0 / std::sqrt(std::max(rad, 1e-30));
    }
    return soma;
}

static double wrapAng(double a) {
    a = std::fmod(a + PI, 2.0 * PI);
    if (a < 0) a += 2.0 * PI;
    return a - PI;
}

// Grava os coeficientes num include GLSL, para o shader usar exatamente
// o que foi ajustado aqui -- sem copiar numeros a mao.
static void gravaGLSL(const char* caminho, const std::vector<double>& c,
                      double eMaxGraus) {
    FILE* f = std::fopen(caminho, "w");
    if (!f) { std::printf("nao consegui gravar %s\n", caminho); return; }
    std::fprintf(f,
        "// =================================================================\n"
        "//  lente_coef.glsl -- GERADO por tools/calibra_lente.cpp.\n"
        "//  Nao edite a mao: rode  mingw32-make calibra\n"
        "//\n"
        "//  Deflexao total de Schwarzschild, ajustada por minimos quadrados\n"
        "//  sobre %d raios integrados por RK4 (observador no infinito):\n"
        "//     alpha(x) = -ln(1-x) + c1 x + c2 x^2 + c3 x^3 + c4 (1-x) ln(1-x)\n"
        "//     x = b_c / b\n"
        "//  Erro maximo de alpha no treino: %.4f graus.\n"
        "// =================================================================\n",
        200, eMaxGraus);
    const char* nomes[] = {"LENTE_C1", "LENTE_C2", "LENTE_C3", "LENTE_C4"};
    for (size_t k = 0; k < c.size() && k < 4; k++)
        std::fprintf(f, "#define %s (%+.9f)\n", nomes[k], c[k]);
    std::fclose(f);
    std::printf("coeficientes gravados em %s\n", caminho);
}

int main(int argc, char** argv) {
    if (argc > 1) MODELO = argv[1][0];
    if (argc > 2) GRAU = std::atoi(argv[2]);
    const char* saidaGLSL = (argc > 3) ? argv[3] : nullptr;
    const bool fixaC1 = (MODELO == 'C');
    const double c1Einstein = 4.0 / BC - 1.0;

    // ------------------------------ TREINO ------------------------------
    // metade das amostras uniforme em x (campo fraco), metade
    // log-espacada em (1-x) (campo forte)
    std::vector<double> xs, ys, as, bs;
    const int N = 200;
    for (int i = 0; i < N; i++) {
        double s = (i + 0.5) / (N / 2);
        double x = (i < N/2) ? 0.005 + 0.9 * s
                             : 1.0 - 0.1 * std::pow(0.0005 / 0.1, (i - N/2 + 0.5) / (N/2));
        double b = BC / x;
        double a = alphaExato(b, (x > 0.99) ? 2e-5 : 1e-4);
        xs.push_back(x); bs.push_back(b); as.push_back(a);
        ys.push_back(a + std::log(1.0 - x));
    }
    std::vector<double> c = ajusta(xs, ys, fixaC1, c1Einstein);

    std::printf("=== TREINO  modelo %c, grau %d, %d amostras ===\n  c =", MODELO, GRAU, N);
    for (double v : c) std::printf(" %+.7f", v);
    double eMax = 0, xMax = 0;
    for (size_t i = 0; i < xs.size(); i++) {
        double e = std::fabs(alphaModelo(bs[i], c) - as[i]);
        if (e > eMax) { eMax = e; xMax = xs[i]; }
    }
    std::printf("\n  erro max de alpha no treino: %.2e rad = %.4f graus (x = %.4f)\n",
                eMax, eMax * 180 / PI, xMax);
    if (saidaGLSL) {
        if (MODELO == 'B' && GRAU == 3) gravaGLSL(saidaGLSL, c, eMax * 180 / PI);
        else std::printf("(o shader espera modelo B grau 3; nada gravado)\n");
    }

    // ------------------------------ TESTE -------------------------------
    const double tanHalf = std::tan(22.5 * PI / 180.0);
    const double r0s[] = {20.0, 52.0, 120.0};
    for (double r0 : r0s) {
        const double psiSh = std::asin((BC / r0) * std::sqrt(1.0 - 2.0*M/r0));
        const double rSh   = std::tan(std::asin(BC / r0)) / tanHalf;   // legado usa a formula antiga
        const double mass  = 1.8 * rSh * rSh * rSh * 0.5;

        std::printf("\n=== TESTE  r0 = %.0f M (%.0f Rs), sombra psi_sh = %.4f graus ===\n",
                    r0, r0/2, psiSh * 180 / PI);
        std::printf("%10s %10s | %9s %9s %10s %10s\n", "psi-psi_sh", "theta_ex",
                    "legado", "Einstein", "cal.I1", "cal.Gauss");
        const double offs[] = {1e-4, 1e-3, 0.01, 0.03, 0.1, 0.3, 1.0, 3.0, 10.0, 20.0};
        for (double od : offs) {
            double psi = psiSh + od * PI / 180.0;
            double te = thetaExato(psi, r0, (od < 0.05) ? 2e-5 : 1e-4);
            if (std::isnan(te)) { std::printf("%10.4f  capturado\n", od); continue; }
            double b = bSynge(psi, r0), cp = std::cos(psi);

            double q = std::tan(psi) / tanHalf;
            double pull = std::min(2.0 * mass / (q*q), 0.85 * q);
            double tL = std::atan((q - pull) * tanHalf);
            // Einstein: lente fina classica, campo fraco
            double tE = psi - (2.0*M/b) * (1.0 + cp);

            // calibrada: theta_s = I(r0, b) - alpha(b), com alpha aprendido
            double aC = alphaModelo(b, c);
            double tC  = I1(r0, b)     - aC;     // trecho do observador analitico
            double tCG = IGauss(r0, b) - aC;     // idem por quadratura

            std::printf("%10.4f %10.3f | %9.3f %9.3f %10.5f %10.5f\n", od, te * 180 / PI,
                        std::fabs(wrapAng(tL - te)) * 180 / PI,
                        std::fabs(wrapAng(tE - te)) * 180 / PI,
                        std::fabs(wrapAng(tC - te)) * 180 / PI,
                        std::fabs(wrapAng(tCG - te)) * 180 / PI);
        }
    }
    return 0;
}
