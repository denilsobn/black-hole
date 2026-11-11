# Buraco Negro — OpenGL 3.3

Simulação interativa de um buraco negro de Schwarzschild com disco de acreção,
escrita em C++17 e GLSL. Permite comparar quatro modelos de curvatura da luz
na mesma cena.

| Tecla | Modelo |
|---|---|
| `1` | Heurística de deslocamento em espaço de tela (`1/r²`) |
| `2` | Integração numérica das geodésicas por Runge–Kutta de quarta ordem (RK4) |
| `3` | Lente fina de Einstein |
| `4` | Lente fina calibrada por mínimos quadrados a partir de trajetórias RK4 |

A câmera pode orbitar o buraco negro e ajustar o zoom. A cena inclui céu
procedural, disco de acreção e controles de exposição e antisserrilhado.

[Instalação](#compilar-e-rodar) · [Controles](#controles)

## Compilar e rodar

Requisitos: Git, CMake 3.15 ou superior, compilador C++17 e driver com
OpenGL 3.3 core. GLFW cria a janela/contexto; GLAD **0.1.x** fornece
`glad/glad.h` e `gladLoadGLLoader`. GLAD 2 não é substituto direto.
Não é necessário Python para executar a simulação.

Execute os comandos a partir da raiz do projeto, salvo quando indicado.
Use uma pasta de build nova para cada sistema/compilador: o cache do CMake
contém caminhos absolutos e não pode ser reutilizado entre Windows e Linux.

### Linux (Ubuntu/Debian, x86-64)

Instale os pré-requisitos pelo gerenciador de pacotes:

```bash
sudo apt update
sudo apt install build-essential cmake git curl zip unzip tar pkg-config \
    libglfw3-dev libgl1-mesa-dev xorg-dev
```

Clone o projeto e prepare o GLAD via vcpkg. A revisão abaixo é a do vcpkg
local utilizado na verificação; fixa o catálogo que fornece GLAD 0.1.36.
O vcpkg é baixado separadamente, sem depender do antigo submódulo incompleto.

```bash
git clone https://github.com/denilsobn/black-hole.git
cd black-hole
git clone https://github.com/microsoft/vcpkg.git vcpkg
git -C vcpkg checkout effef16ad89ecc4c58c855613ba66a6089ffee6e
./vcpkg/bootstrap-vcpkg.sh -disableMetrics
./vcpkg/vcpkg install glad:x64-linux
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET=x64-linux
cmake --build build-linux --parallel
(cd build-linux && ./buraco_negro)
```

Este caminho combina GLFW do sistema com GLAD do vcpkg. A versão do GLFW
varia com a distribuição; a verificação local utilizou GLFW 3.3.10.
O bootstrap/instalação requer acesso à internet. Em Wayland, a versão GLFW
X11 fornecida pela distribuição pode precisar de XWayland.

### Windows 10/11 (x86-64, MinGW UCRT64)

1. Instale [Git for Windows](https://git-scm.com/downloads/win),
   [CMake](https://cmake.org/download/) (adicione ao `PATH`) e
   [MSYS2](https://www.msys2.org/).
2. No terminal **MSYS2 UCRT64**, atualize os pacotes com `pacman -Syu`.
   Se solicitado, feche e reabra o terminal e repita a atualização.
   Depois instale o compilador e o Make:

   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make
   ```

3. Abra um **PowerShell** novo e execute o bloco abaixo. Ajuste o caminho
   do MSYS2 se ele foi instalado em outro local. O mesmo ambiente UCRT64
   deve compilar o projeto e suas dependências; não misture MSVC e MinGW.

```powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
git clone https://github.com/denilsobn/black-hole.git
Set-Location black-hole
git clone https://github.com/microsoft/vcpkg.git vcpkg
git -C vcpkg checkout effef16ad89ecc4c58c855613ba66a6089ffee6e
.\vcpkg\bootstrap-vcpkg.bat -disableMetrics
.\vcpkg\vcpkg.exe install glfw3:x64-mingw-dynamic glad:x64-mingw-dynamic --host-triplet=x64-mingw-dynamic
cmake -S . -B build-windows -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release "-DCMAKE_TOOLCHAIN_FILE=$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic -DVCPKG_HOST_TRIPLET=x64-mingw-dynamic
cmake --build build-windows --parallel
$env:Path = "$PWD\vcpkg\installed\x64-mingw-dynamic\bin;" + $env:Path
Push-Location build-windows
.\buraco_negro.exe
Pop-Location
```

Os triplets MinGW são mantidos pela comunidade. Consulte a
[documentação oficial de MinGW no vcpkg](https://learn.microsoft.com/en-us/vcpkg/users/platforms/mingw).
A entrada no `PATH` permite localizar `glfw3.dll`; o caminho UCRT64 permite
localizar as DLLs de runtime do GCC. Para distribuir um executável, inclua
as DLLs necessárias, a pasta `shaders/` e os avisos de licença das dependências.
Feche a simulação antes de recompilar se ocorrer `Permission denied` no `.exe`.

### Validação numérica e calibração (sem OpenGL)

Estas ferramentas precisam apenas de C++17. Na raiz do projeto:

```bash
# Linux
g++ -O2 -std=c++17 tools/valida_rk4.cpp -o build-linux/valida_rk4
./build-linux/valida_rk4
g++ -O2 -std=c++17 tools/calibra_lente.cpp -o build-linux/calibra_lente
./build-linux/calibra_lente B 3 build-linux/lente_coef.glsl
```

```powershell
# Windows, no PowerShell com UCRT64 no PATH
g++ -O2 -std=c++17 tools/valida_rk4.cpp -o build-windows/valida_rk4.exe
.\build-windows\valida_rk4.exe
g++ -O2 -std=c++17 tools/calibra_lente.cpp -o build-windows/calibra_lente.exe
.\build-windows\calibra_lente.exe B 3 build-windows/lente_coef.glsl
```

As pastas de build devem existir (criadas pelo CMake acima). O primeiro
programa imprime comparações numéricas, sem asserções automáticas. O segundo
salva os coeficientes numa cópia para comparação com `shaders/lente_coef.glsl`;
para atualizar o shader, passe esse caminho como último argumento.

### Capturas e dados

No Linux, a partir da raiz:

```bash
./build-linux/buraco_negro 4 --foto build-linux/captura.png --res 1024x768 --tempo 3.0
./build-linux/buraco_negro --erro build-linux/erro
./build-linux/buraco_negro --perfil build-linux/desempenho.dat
```

No PowerShell, substitua `./build-linux/buraco_negro` por
`.\build-windows\buraco_negro.exe` e use `build-windows/` nos caminhos de saída.
Mesmo os modos sem janela visível criam um contexto GLFW/OpenGL: precisam de
driver e de uma sessão gráfica acessível. Não são renderização puramente CPU.
Os números de desempenho dependem de GPU, driver, resolução e parâmetros.

O `Makefile` é um atalho opcional para GNU Make (`make` no Linux,
`mingw32-make` no Windows). Com o vcpkg em `./vcpkg` ou em `VCPKG_ROOT`:

```bash
make BUILD_DIR=build-linux run
make BUILD_DIR=build-linux valida
```

No Windows, use `mingw32-make BUILD_DIR=build-windows run`.
`make calibra` sobrescreve o shader de coeficientes. `figuras`, `erro` e
`perfil` sobrescrevem figuras/dados em `docs/artigo/`; os recortes e a
compressão automática de `figuras` usam Windows PowerShell e `System.Drawing`.
No Linux esses recortes não são executados. Evite alterar dados publicados
sem registrar GPU, driver e parâmetros da nova medição.

### Recarregamento a quente dos shaders

O programa prefere `shaders/` no caminho do código-fonte gravado pelo CMake.
Edite um shader e pressione `F5` para recarregá-lo. Também há uma cópia ao
lado do executável, criada no build. Para usar essa cópia em outra máquina,
execute a partir da pasta do executável. O caminho de compilação fica
embutido no binário; builds locais não são artefatos anônimos.

## Controles

**Mouse**

| Ação | Efeito |
|---|---|
| arrastar com o botão esquerdo | orbitar a câmera |
| roda do mouse | zoom |

**Teclado**

| Tecla | Ação |
|---|---|
| `1` | heurística (`1/r²` em espaço de tela) |
| `2` | RK4 — referência numérica |
| `3` / `4` | lente fina de Einstein / lente fina **calibrada** |
| `←` `→` | orbitar |
| `↑` `↓` | aproximar / afastar |
| `=` / `-` | raio de Schwarzschild (Rs) |
| `G` | céu: estrelas ↔ **grade lat/long** |
| `K` | amostras por pixel: 1×1 → 2×2 → 3×3 (antisserrilhado) |
| `J` | modo legado do disco: mostra a **falha de junção** |
| `D` | liga/desliga o disco de acreção |
| `Z` / `X` | raio externo do disco |
| `C` / `V` | raio interno do disco (ISCO) |
| `[` / `]` | força da distorção heurística |
| `N` / `M` | passo do RK4 em φ (só no geodésico) |
| `,` / `.` | exposição |
| `B` | liga/desliga v-sync — **desligue para medir FPS** |
| `P` | imprime uma linha de benchmark no terminal |
| `R` | volta tudo ao padrão |
| `F5` | recarrega os shaders do disco, sem recompilar |
| `H` | mostra a ajuda |
| `ESC` | sair |


## Organização

- `src/`: aplicação, carregamento de shaders e gravação de PNG.
- `shaders/`: modelos de curvatura, cena compartilhada e coeficientes da lente.
- `tools/`: validação RK4, calibração e scripts de processamento de figuras.
- `docs/artigo/figs/` e `docs/artigo/dados/`: figuras e resultados dos experimentos.
- `CMakeLists.txt` e `Makefile`: configuração e atalhos de compilação.

## Limitações

O modelo descreve um buraco negro sem rotação (Schwarzschild). O disco é plano
e opticamente fino, com Doppler empírico; não há transferência radiativa nem
aberração relativista do observador. A integração RK4 usa passo fixo.
O supersampling reduz o aliasing, mas regiões de grande ampliação ainda podem
apresentar artefatos. As heurísticas são aproximações para visualização.
