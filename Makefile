# =====================================================================
#  Makefile do projeto Buraco Negro (OpenGL 3.3 core)
#
#  E um invólucro fino em volta do CMake -- quem compila de verdade
#  continua sendo o CMake + vcpkg. A ideia e voce nunca mais precisar
#  lembrar da linha de configuracao com o toolchain.
#
#  NESTA MAQUINA o make se chama "mingw32-make" (vem do MSYS2), entao:
#
#      mingw32-make            compila (configura na primeira vez)
#      mingw32-make run        compila e ja executa a simulacao
#      mingw32-make rebuild    apaga o build/ e recompila do zero
#      mingw32-make shaders    so recopia os shaders (nao recompila C++)
#      mingw32-make clean      apaga os objetos, mantem a configuracao
#      mingw32-make distclean  apaga a pasta build/ inteira
#      mingw32-make info       mostra a configuracao detectada
#      mingw32-make help       lista os alvos
#
#  Em Linux/macOS e so trocar "mingw32-make" por "make".
#
#  Variaveis que dao para sobrescrever na linha de comando, ex.:
#      mingw32-make BUILD=Debug
#      mingw32-make GEN=Ninja run
#      mingw32-make TRIPLET=x64-windows
# =====================================================================

BUILD_DIR ?= build
BUILD     ?= Release
CMAKE     ?= cmake

EXE_BASE  := buraco_negro

ifeq ($(OS),Windows_NT)
    EXE_FILE := $(EXE_BASE).exe
    EXT      := .exe
    # O compilador aqui e o g++ do MSYS2/UCRT64, entao o gerador tem de
    # ser Makefiles do MinGW (e nao o do Visual Studio).
    GEN      ?= MinGW Makefiles
    TRIPLET  ?= x64-mingw-dynamic
else
    EXE_FILE := $(EXE_BASE)
    EXT      :=
    GEN      ?=
    TRIPLET  ?=
endif

VALIDA_FILE := valida_rk4$(EXT)

EXE := $(BUILD_DIR)/$(EXE_FILE)

# ---------------------------------------------------------------------
# Localiza o toolchain do vcpkg: primeiro o clone que esta dentro do
# projeto, depois a variavel de ambiente VCPKG_ROOT. Se nao achar
# nenhum, deixa o CMake se resolver (util em Linux, onde as libs
# costumam vir do gerenciador de pacotes).
# ---------------------------------------------------------------------
LOCAL_VCPKG := vcpkg/scripts/buildsystems/vcpkg.cmake

ifneq ($(wildcard $(LOCAL_VCPKG)),)
    VCPKG_TC   := $(CURDIR)/$(LOCAL_VCPKG)
else ifneq ($(VCPKG_ROOT),)
    VCPKG_TC   := $(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake
else
    VCPKG_TC   :=
endif

ifneq ($(VCPKG_TC),)
    TC_FLAGS := -DCMAKE_TOOLCHAIN_FILE=$(VCPKG_TC)
    ifneq ($(TRIPLET),)
        TC_FLAGS += -DVCPKG_TARGET_TRIPLET=$(TRIPLET)
    endif
else
    TC_FLAGS :=
endif

ifneq ($(GEN),)
    GEN_FLAG  := -G "$(GEN)"
    GEN_LABEL := $(GEN)
else
    GEN_FLAG  :=
    GEN_LABEL := (padrao do CMake)
endif

CONFIGURE_FLAGS := $(GEN_FLAG) $(TC_FLAGS) -DCMAKE_BUILD_TYPE=$(BUILD)

.PHONY: all build configure run shaders valida calibra figuras erro perfil artigo rebuild clean distclean info help

all: build

# ---------------------------------------------------------------------
# Configuracao: roda uma vez e fica em cache. O alvo depende do
# CMakeCache.txt, entao chamadas seguintes nao reconfiguram de graca.
# ---------------------------------------------------------------------
$(BUILD_DIR)/CMakeCache.txt:
	@$(CMAKE) -E echo "[cfg] gerador: $(GEN_LABEL)  |  build: $(BUILD)"
	@$(CMAKE) -E echo "[cfg] toolchain: $(if $(VCPKG_TC),$(VCPKG_TC),nenhum)"
	$(CMAKE) -B $(BUILD_DIR) -S . $(CONFIGURE_FLAGS)

configure: $(BUILD_DIR)/CMakeCache.txt

build: configure
	@$(CMAKE) -E echo "[build] compilando..."
	$(CMAKE) --build $(BUILD_DIR) --parallel
	@$(CMAKE) -E echo "[build] pronto: $(EXE)"

# ---------------------------------------------------------------------
# Executa. Usa "cmake -E chdir" em vez de chamar o .exe direto porque
# isso funciona igual no cmd.exe, no PowerShell, no bash do MSYS2 e no
# terminal do Linux -- nao depende de qual shell o make escolheu.
# ---------------------------------------------------------------------
# Passe argumentos com ARGS, ex.:  mingw32-make run ARGS="2 --sem-disco"
ARGS ?=

run: build
	@$(CMAKE) -E echo "[run] iniciando a simulacao (ESC para sair)"
	@$(CMAKE) -E chdir $(BUILD_DIR) ./$(EXE_FILE) $(ARGS)

# Atalho para quando voce mexeu SO em .frag/.vert/.glsl: recopia os
# shaders sem passar pelo compilador C++. (Dentro do programa, F5
# recarrega direto de shaders/ sem nem precisar disto.)
shaders:
	@$(CMAKE) -E echo "[shaders] copiando shaders/ para $(BUILD_DIR)/shaders"
	@$(CMAKE) -E copy_directory shaders $(BUILD_DIR)/shaders

# ---------------------------------------------------------------------
# Verificacao numerica do integrador RK4 contra os valores analiticos
# de Schwarzschild. Nao usa OpenGL nem vcpkg -- so g++. E daqui que
# saem os numeros da tabela de validacao do README.
# ---------------------------------------------------------------------
valida:
	@$(CMAKE) -E make_directory $(BUILD_DIR)
	@$(CMAKE) -E echo "[valida] compilando tools/valida_rk4.cpp"
	$(CXX) -O2 -std=c++17 tools/valida_rk4.cpp -o $(BUILD_DIR)/$(VALIDA_FILE)
	@$(CMAKE) -E chdir $(BUILD_DIR) ./$(VALIDA_FILE)

# ---------------------------------------------------------------------
# Calibracao da heuristica: integra 200 raios por RK4 (em double, na
# CPU), ajusta a deflexao alpha(b) por minimos quadrados e grava os
# coeficientes em shaders/lente_coef.glsl, que o lens.frag inclui.
# ---------------------------------------------------------------------
calibra:
	@$(CMAKE) -E make_directory $(BUILD_DIR)
	@$(CMAKE) -E echo "[calibra] compilando tools/calibra_lente.cpp"
	$(CXX) -O2 -std=c++17 tools/calibra_lente.cpp -o $(BUILD_DIR)/calibra_lente$(EXT)
	@$(CMAKE) -E chdir $(BUILD_DIR) ./calibra_lente$(EXT) B 3 ../shaders/lente_coef.glsl

# ---------------------------------------------------------------------
# Figuras e dados do artigo. Tudo offscreen, em estado fixo (--tempo),
# entao o resultado nao depende de a tela estar acordada.
#   figuras : renders dos 4 modelos + recortes + figuras do README
#   erro    : mapas de erro angular (26 Rs) e estatisticas (10/26/60 Rs)
#   perfil  : tempo de GPU por resolucao -> dados/desempenho.dat
# Os recortes e a compressao usam PowerShell (so no Windows).
# ---------------------------------------------------------------------
FIG_DIR  := docs/artigo/figs
DADOS    := docs/artigo/dados
RUN      := $(CMAKE) -E chdir $(BUILD_DIR) ./$(EXE_FILE)
PS       := powershell -NoProfile -ExecutionPolicy Bypass -File

figuras: build
	@$(CMAKE) -E echo "[figuras] gerando em $(FIG_DIR)"
	@$(CMAKE) -E make_directory $(FIG_DIR)
	@$(RUN) 2 --spp2 --foto ../$(FIG_DIR)/r_rk4.png       --tempo 3.0
	@$(RUN) 4 --spp2 --foto ../$(FIG_DIR)/r_calibrada.png --tempo 3.0
	@$(RUN) 3 --spp2 --foto ../$(FIG_DIR)/r_einstein.png  --tempo 3.0
	@$(RUN) 1 --spp2 --foto ../$(FIG_DIR)/r_legado.png    --tempo 3.0
	@$(RUN) 1 --grade --sem-disco --spp2 --foto ../$(FIG_DIR)/fig_grade_heur.png --tempo 3.0
	@$(RUN) 2 --grade --sem-disco --spp2 --foto ../$(FIG_DIR)/fig_grade_geo.png  --tempo 3.0
	@$(RUN) 1 --legado --spp2 --foto ../$(FIG_DIR)/fig_juncao_legado.png    --tempo 3.0
	@$(RUN) 1 --spp2          --foto ../$(FIG_DIR)/fig_juncao_corrigida.png --tempo 3.0
ifeq ($(OS),Windows_NT)
	@$(PS) tools/recorta.ps1 -In $(FIG_DIR)/r_rk4.png       -Out $(FIG_DIR)/r_rk4_c.png       -Fracao 0.55
	@$(PS) tools/recorta.ps1 -In $(FIG_DIR)/r_calibrada.png -Out $(FIG_DIR)/r_calibrada_c.png -Fracao 0.55
	@$(PS) tools/recorta.ps1 -In $(FIG_DIR)/r_einstein.png  -Out $(FIG_DIR)/r_einstein_c.png  -Fracao 0.55
	@$(PS) tools/recorta.ps1 -In $(FIG_DIR)/r_legado.png    -Out $(FIG_DIR)/r_legado_c.png    -Fracao 0.55
	@$(PS) tools/recorta.ps1 -In $(FIG_DIR)/fig_juncao_legado.png    -Out $(FIG_DIR)/fig_costura_legado.png    -X 600 -Y 300 -W 210 -H 170 -Escala 2
	@$(PS) tools/recorta.ps1 -In $(FIG_DIR)/fig_juncao_corrigida.png -Out $(FIG_DIR)/fig_costura_corrigida.png -X 600 -Y 300 -W 210 -H 170 -Escala 2
	@$(PS) tools/comprime_figuras.ps1
else
	@$(CMAKE) -E echo "[figuras] recortes e compressao usam PowerShell; rode-os no Windows"
endif
	@$(CMAKE) -E echo "[figuras] prontas em $(FIG_DIR)"

erro: build
	@$(CMAKE) -E make_directory $(DADOS)
	@$(RUN) --erro ../$(FIG_DIR)/erro
	@$(CMAKE) -E rename $(FIG_DIR)/erro_estatisticas.txt $(DADOS)/erro_26Rs.txt
	@$(RUN) --dist 5  --erro ../$(DADOS)/erro_10Rs
	@$(RUN) --dist 30 --erro ../$(DADOS)/erro_60Rs
	@$(CMAKE) -E rename $(DADOS)/erro_10Rs_estatisticas.txt $(DADOS)/erro_10Rs.txt
	@$(CMAKE) -E rename $(DADOS)/erro_60Rs_estatisticas.txt $(DADOS)/erro_60Rs.txt
	@$(CMAKE) -E rm -f $(DADOS)/erro_10Rs_legado.png $(DADOS)/erro_10Rs_einstein.png $(DADOS)/erro_10Rs_calibrada.png
	@$(CMAKE) -E rm -f $(DADOS)/erro_60Rs_legado.png $(DADOS)/erro_60Rs_einstein.png $(DADOS)/erro_60Rs_calibrada.png
ifeq ($(OS),Windows_NT)
	@$(PS) tools/comprime_figuras.ps1
endif
	@$(CMAKE) -E echo "[erro] estatisticas em $(DADOS)/erro_*Rs.txt"

perfil: build
	@$(CMAKE) -E make_directory $(DADOS)
	@$(RUN) --perfil ../$(DADOS)/desempenho.dat

# ---------------------------------------------------------------------
# Compila o resumo expandido (pdflatex -> biber -> pdflatex x2).
# ---------------------------------------------------------------------
PDFLATEX ?= pdflatex
BIBER    ?= biber

artigo:
	@$(CMAKE) -E echo "[artigo] compilando docs/artigo/main.tex"
	@$(CMAKE) -E make_directory $(FIG_DIR)
	@$(CMAKE) -E copy_if_different figs/header.pdf $(FIG_DIR)/header.pdf
	@$(CMAKE) -E chdir docs/artigo $(PDFLATEX) -interaction=nonstopmode main.tex
	@$(CMAKE) -E chdir docs/artigo $(BIBER) main
	@$(CMAKE) -E chdir docs/artigo $(PDFLATEX) -interaction=nonstopmode main.tex
	@$(CMAKE) -E chdir docs/artigo $(PDFLATEX) -interaction=nonstopmode main.tex
	@$(CMAKE) -E echo "[artigo] pronto: docs/artigo/main.pdf"

rebuild:
	@$(MAKE) distclean
	@$(MAKE) build

clean:
	@$(CMAKE) -E echo "[clean] apagando objetos (mantendo a configuracao)"
	-@$(CMAKE) --build $(BUILD_DIR) --target clean

distclean:
	@$(CMAKE) -E echo "[distclean] removendo $(BUILD_DIR)/"
	@$(CMAKE) -E rm -rf $(BUILD_DIR)

info:
	@$(CMAKE) -E echo "sistema .......... $(if $(filter Windows_NT,$(OS)),Windows,$(shell uname -s))"
	@$(CMAKE) -E echo "gerador .......... $(GEN_LABEL)"
	@$(CMAKE) -E echo "tipo de build .... $(BUILD)"
	@$(CMAKE) -E echo "triplet vcpkg .... $(if $(TRIPLET),$(TRIPLET),n/a)"
	@$(CMAKE) -E echo "toolchain ........ $(if $(VCPKG_TC),$(VCPKG_TC),nenhum)"
	@$(CMAKE) -E echo "executavel ....... $(EXE)"
	@$(CMAKE) -E echo "configurado ...... $(if $(wildcard $(BUILD_DIR)/CMakeCache.txt),sim,nao)"

help:
	@$(CMAKE) -E echo "Alvos disponiveis:"
	@$(CMAKE) -E echo "  (nenhum)   compila o projeto (configura na primeira vez)"
	@$(CMAKE) -E echo "  run        compila e executa a simulacao"
	@$(CMAKE) -E echo "  rebuild    apaga build/ e recompila do zero"
	@$(CMAKE) -E echo "  shaders    recopia apenas os shaders"
	@$(CMAKE) -E echo "  valida     compila e roda a verificacao numerica do RK4"
	@$(CMAKE) -E echo "  calibra    reajusta a heuristica calibrada (shaders/lente_coef.glsl)"
	@$(CMAKE) -E echo "  figuras    gera as figuras do artigo (offscreen, estado fixo)"
	@$(CMAKE) -E echo "  erro       mapas de erro angular contra o RK4 + estatisticas"
	@$(CMAKE) -E echo "  perfil     tempo de GPU por resolucao (docs/artigo/dados)"
	@$(CMAKE) -E echo "  artigo     compila docs/artigo/main.tex (pdflatex + biber)"
	@$(CMAKE) -E echo "  clean      apaga os objetos, mantem a configuracao"
	@$(CMAKE) -E echo "  distclean  apaga a pasta build/ inteira"
	@$(CMAKE) -E echo "  info       mostra a configuracao detectada"
	@$(CMAKE) -E echo "  help       esta lista"
	@$(CMAKE) -E echo ""
	@$(CMAKE) -E echo "Variaveis: BUILD=Release|Debug  GEN=<gerador>  TRIPLET=<triplet>"
	@$(CMAKE) -E echo "Neste PC o make se chama mingw32-make (MSYS2)."
