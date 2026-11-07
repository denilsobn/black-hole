// =================================================================
//  lente_coef.glsl -- GERADO por tools/calibra_lente.cpp.
//  Nao edite a mao: rode  mingw32-make calibra
//
//  Deflexao total de Schwarzschild, ajustada por minimos quadrados
//  sobre 200 raios integrados por RK4 (observador no infinito):
//     alpha(x) = -ln(1-x) + c1 x + c2 x^2 + c3 x^3 + c4 (1-x) ln(1-x)
//     x = b_c / b
//  Erro maximo de alpha no treino: 0.0096 graus.
// =================================================================
#define LENTE_C1 (-0.483321916)
#define LENTE_C2 (+0.050369748)
#define LENTE_C3 (+0.032896854)
#define LENTE_C4 (-0.255648046)
