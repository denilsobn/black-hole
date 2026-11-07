#version 330 core
// ---------------------------------------------------------------
// Vertex shader trivial: o quad de tela cheia ja chega em NDC.
// Todo o trabalho (cena + distorcao) acontece no fragment shader,
// que funciona como um ray tracer por pixel.
// ---------------------------------------------------------------
layout(location = 0) in vec2 aPos; // ja em NDC: cada componente vai de -1 a 1

out vec2 vNDC; // repassado direto para o fragment shader

void main() {
    vNDC = aPos;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
