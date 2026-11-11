#include <cstdio>
#include <cmath>

static const double PI = 3.14159265358979323846;

static void rk4Step(double& u, double& w, double M, double h) {
    const double k1u = w;
    const double k1w = -u + 3.0 * M * u * u;

    const double ua = u + 0.5 * h * k1u, wa = w + 0.5 * h * k1w;
    const double k2u = wa;
    const double k2w = -ua + 3.0 * M * ua * ua;

    const double ub = u + 0.5 * h * k2u, wb = w + 0.5 * h * k2w;
    const double k3u = wb;
    const double k3w = -ub + 3.0 * M * ub * ub;

    const double uc = u + h * k3u, wc = w + h * k3w;
    const double k4u = wc;
    const double k4w = -uc + 3.0 * M * uc * uc;

    u += (h / 6.0) * (k1u + 2.0 * k2u + 2.0 * k3u + k4u);
    w += (h / 6.0) * (k1w + 2.0 * k2w + 2.0 * k3w + k4w);
}

static bool cai(double r0, double b, double M, double h, long maxSteps) {
    double sina = b / r0;
    if (sina > 1.0) sina = 1.0;
    const double alpha = PI - asin(sina);
    double u = 1.0 / r0;
    double w = -cos(alpha) / b;

    const double uHor = 1.0 / (2.0 * M);
    const double uEsc = 1.0 / (200.0 * r0);

    for (long i = 0; i < maxSteps; i++) {
        rk4Step(u, w, M, h);
        if (u > uHor) return true;
        if (u <= 0.0) return false;
        if (u < uEsc && w < 0.0) return false;
    }
    return false;
}

static double perielioU(double b, double M) {
    double lo = 0.0, hi = 1.0 / (3.0 * M);
    for (int i = 0; i < 200; i++) {
        const double mid = 0.5 * (lo + hi);
        const double f = mid * mid * (1.0 - 2.0 * M * mid) - 1.0 / (b * b);
        if (f < 0.0) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}

static double deflexao(double b, double M, double h, long maxSteps) {
    double u = perielioU(b, M);
    double w = 0.0;
    double phi = 0.0;

    for (long i = 0; i < maxSteps; i++) {
        const double uPrev = u, phiPrev = phi;
        rk4Step(u, w, M, h);
        phi += h;
        if (u <= 0.0) {
            const double f = uPrev / (uPrev - u);
            return 2.0 * (phiPrev + f * (phi - phiPrev)) - PI;
        }
    }
    return NAN;
}

int main() {
    const double M  = 1.0;
    const double Rs = 2.0 * M;

    std::printf("=========================================================\n");
    std::printf(" Validacao do RK4 de shaders/geodesic.frag   (M = 1)\n");
    std::printf("=========================================================\n");

    const double bcExato = 3.0 * std::sqrt(3.0) * M;
    double lo = 3.0 * M, hi = 10.0 * M;
    for (int i = 0; i < 60; i++) {
        const double mid = 0.5 * (lo + hi);
        if (cai(1.0e4 * M, mid, M, 1e-4, 2000000L)) lo = mid; else hi = mid;
    }
    const double bc = 0.5 * (lo + hi);

    std::printf("\n[1] Parametro de impacto critico de captura\n");
    std::printf("    b_c numerico  = %.9f\n", bc);
    std::printf("    b_c = 3*sqrt(3)*M = %.9f\n", bcExato);
    std::printf("    erro relativo = %.3e\n", std::fabs(bc - bcExato) / bcExato);
    std::printf("    b_c/Rs = %.9f   (esperado 2.598076211)\n", bc / Rs);

    std::printf("\n[2] Deflexao total (h = 1e-4 rad)\n");
    std::printf("    %8s %16s %16s %16s %11s\n",
                "b/M", "RK4", "4M/b", "+15pi M^2/4b^2", "dif. rel.");
    const double bs[] = {1000, 500, 100, 50, 20, 10, 7, 6, 5.5};
    for (double b : bs) {
        const double d  = deflexao(b, M, 1e-4, 200000000L);
        const double a1 = 4.0 * M / b;
        const double a2 = a1 + 15.0 * PI * M * M / (4.0 * b * b);
        std::printf("    %8.1f %16.9f %16.9f %16.9f %10.2e\n",
                    b / M, d, a1, a2, std::fabs(d - a2) / a2);
    }
    std::printf("\n    Nota: para b pequeno a diferenca CRESCE porque a serie\n");
    std::printf("    analitica e uma expansao de campo fraco e diverge quando\n");
    std::printf("    b -> b_c = %.4f M. Ali o valor correto e o do RK4, nao o\n", bcExato);
    std::printf("    da serie truncada.\n");

    std::printf("\n[3] Efeito do passo em phi (b = 10 M)\n");
    std::printf("    %10s %18s %14s\n", "h", "deflexao", "|dif. vs h=1e-6|");
    const double ref = deflexao(10.0, M, 1e-6, 2000000000L);
    const double hs[] = {0.04, 0.02, 0.01, 0.005, 0.0025};
    for (double h : hs) {
        const double d = deflexao(10.0, M, h, 200000000L);
        std::printf("    %10.5f %18.10f %14.3e\n", h, d, std::fabs(d - ref));
    }
    std::printf("\n    O passo padrao do shader e 0.02 rad (teclas N / M).\n");
    std::printf("    referencia h = 1e-6: %.10f\n\n", ref);
    return 0;
}
