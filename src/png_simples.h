#pragma once
// =====================================================================
//  png_simples.h -- gravador de PNG mínimo, sem dependência externa.
//
//  Por que existe: o pdflatex aceita PNG, JPG e PDF, mas nao PPM. Sem
//  isto, gerar as figuras do artigo exigiria um passo manual de
//  conversao (ImageMagick, um script...), e a reprodutibilidade das
//  figuras dependeria de uma ferramenta que pode nao estar instalada.
//
//  Como funciona: um PNG e uma sequencia de chunks, cada um com
//  tamanho, tipo, dados e CRC-32. Os dados de imagem (IDAT) vao num
//  fluxo zlib. Nao precisamos comprimir de verdade: o deflate admite
//  blocos "stored" (nao comprimidos), que so precisam de um cabecalho
//  de 5 bytes a cada 65535 bytes de dados. Entao o arquivo sai maior
//  que um PNG normal, mas e um PNG perfeitamente valido, e o codigo
//  cabe em 80 linhas sem zlib nem libpng.
// =====================================================================
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace png_simples {

inline uint32_t crc32(const unsigned char* data, size_t n, uint32_t crc = 0) {
    static uint32_t tabela[256];
    static bool pronta = false;
    if (!pronta) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            tabela[i] = c;
        }
        pronta = true;
    }
    crc = crc ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++)
        crc = tabela[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

inline void be32(std::vector<unsigned char>& v, uint32_t x) {
    v.push_back((unsigned char)(x >> 24));
    v.push_back((unsigned char)(x >> 16));
    v.push_back((unsigned char)(x >> 8));
    v.push_back((unsigned char)(x));
}

inline void chunk(std::vector<unsigned char>& out, const char tipo[4],
                  const std::vector<unsigned char>& dados) {
    be32(out, (uint32_t)dados.size());
    const size_t ini = out.size();
    out.insert(out.end(), tipo, tipo + 4);
    out.insert(out.end(), dados.begin(), dados.end());
    be32(out, crc32(out.data() + ini, out.size() - ini));
}

// pixels: RGBA, 4 bytes por pixel, primeira linha EM CIMA.
inline bool grava(const std::string& caminho, int w, int h,
                  const unsigned char* rgba) {
    // ---- linhas filtradas (filtro 0 = None) + adler32 ----
    std::vector<unsigned char> bruto;
    bruto.reserve(size_t(h) * (1 + size_t(w) * 3));
    uint32_t a = 1, b = 0;
    auto adler = [&](unsigned char c) {
        a = (a + c) % 65521; b = (b + a) % 65521;
    };
    for (int y = 0; y < h; y++) {
        bruto.push_back(0);
        adler(0);
        for (int x = 0; x < w; x++) {
            const unsigned char* q = rgba + (size_t(y) * size_t(w) + size_t(x)) * 4;
            for (int c = 0; c < 3; c++) { bruto.push_back(q[c]); adler(q[c]); }
        }
    }

    // ---- fluxo zlib com blocos deflate "stored" ----
    std::vector<unsigned char> z;
    z.push_back(0x78); z.push_back(0x01);          // cabecalho zlib
    size_t pos = 0;
    while (pos < bruto.size()) {
        const size_t n = (bruto.size() - pos > 65535) ? 65535 : (bruto.size() - pos);
        const bool ultimo = (pos + n == bruto.size());
        z.push_back(ultimo ? 1 : 0);
        z.push_back((unsigned char)(n & 0xFF));
        z.push_back((unsigned char)(n >> 8));
        z.push_back((unsigned char)(~n & 0xFF));
        z.push_back((unsigned char)((~n >> 8) & 0xFF));
        z.insert(z.end(), bruto.begin() + pos, bruto.begin() + pos + n);
        pos += n;
    }
    be32(z, (b << 16) | a);                        // adler32

    // ---- monta o arquivo ----
    std::vector<unsigned char> out = {137, 'P', 'N', 'G', 13, 10, 26, 10};

    std::vector<unsigned char> ihdr;
    be32(ihdr, (uint32_t)w);
    be32(ihdr, (uint32_t)h);
    ihdr.push_back(8);   // 8 bits por canal
    ihdr.push_back(2);   // tipo 2 = RGB
    ihdr.push_back(0);   // deflate
    ihdr.push_back(0);   // filtro adaptativo
    ihdr.push_back(0);   // sem entrelacamento
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});

    FILE* f = std::fopen(caminho.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

} // namespace png_simples
