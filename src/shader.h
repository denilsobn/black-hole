#pragma once
#include <glad/glad.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

inline std::string getExecutableDir() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    std::string path(buffer, len);
    size_t pos = path.find_last_of("\\/");
    return (pos == std::string::npos) ? "." : path.substr(0, pos);
#else
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        std::string path(buffer);
        size_t pos = path.find_last_of('/');
        return (pos == std::string::npos) ? "." : path.substr(0, pos);
    }
    return ".";
#endif
}

inline bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

inline std::string findShaderDir() {
#ifdef SHADER_SOURCE_DIR
    {
        std::string src = SHADER_SOURCE_DIR;
        if (fileExists(src + "/fullscreen.vert")) return src;
    }
#endif
    return getExecutableDir() + "/shaders";
}

inline std::string readFile(const std::string& path, bool* ok = nullptr) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[shader] Nao foi possivel abrir: " << path << "\n";
        if (ok) *ok = false;
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    if (ok) *ok = true;
    return ss.str();
}

inline std::string resolveIncludes(const std::string& source,
                                   const std::string& dir,
                                   bool* ok,
                                   int depth = 0) {
    if (depth > 8) {
        std::cerr << "[shader] #include aninhado demais (ciclo?)\n";
        if (ok) *ok = false;
        return source;
    }

    std::stringstream in(source);
    std::string out, line;

    while (std::getline(in, line)) {

        size_t p = line.find_first_not_of(" \t");
        if (p != std::string::npos && line.compare(p, 8, "#include") == 0) {
            size_t q1 = line.find('"', p + 8);
            size_t q2 = (q1 == std::string::npos) ? std::string::npos
                                                  : line.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                std::string name = line.substr(q1 + 1, q2 - q1 - 1);
                std::string path = dir + "/" + name;

                bool readOk = false;
                std::string inc = readFile(path, &readOk);
                if (!readOk && ok) *ok = false;

                out += "// ---- inicio do include: " + name + " ----\n";
                out += resolveIncludes(inc, dir, ok, depth + 1);
                out += "\n// ---- fim do include: " + name + " ----\n";
                continue;
            }
            std::cerr << "[shader] #include mal formado: " << line << "\n";
            if (ok) *ok = false;
        }
        out += line;
        out += '\n';
    }
    return out;
}

inline void dumpNumbered(const std::string& src) {
    std::stringstream in(src);
    std::string line;
    int n = 1;
    while (std::getline(in, line))
        std::cerr << "  " << n++ << "\t" << line << "\n";
}

inline GLuint compileShader(GLenum type, const std::string& source,
                            const std::string& debugName) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 1 ? len : 1);
        glGetShaderInfoLog(shader, (GLsizei)log.size(), nullptr, log.data());
        std::cerr << "\n[shader] ERRO DE COMPILACAO (" << debugName << "):\n"
                  << log.data() << "\n--- fonte apos resolver os #include ---\n";
        dumpNumbered(source);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

inline GLuint loadProgram(const std::string& dir,
                          const std::string& vertName,
                          const std::string& fragName) {
    bool ok = true;
    std::string vertSrc = readFile(dir + "/" + vertName, &ok);
    std::string fragSrc = readFile(dir + "/" + fragName, &ok);
    if (!ok) return 0;

    vertSrc = resolveIncludes(vertSrc, dir, &ok);
    fragSrc = resolveIncludes(fragSrc, dir, &ok);
    if (!ok) return 0;

    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc, vertName);
    if (!vert) return 0;
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc, fragName);
    if (!frag) { glDeleteShader(vert); return 0; }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 1 ? len : 1);
        glGetProgramInfoLog(program, (GLsizei)log.size(), nullptr, log.data());
        std::cerr << "\n[shader] ERRO DE LINKAGEM (" << vertName << " + "
                  << fragName << "):\n" << log.data() << "\n";
        glDeleteShader(vert);
        glDeleteShader(frag);
        glDeleteProgram(program);
        return 0;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
    return program;
}
