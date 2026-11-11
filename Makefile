BUILD_DIR ?= build
BUILD     ?= Release
CMAKE     ?= cmake

EXE_BASE  := buraco_negro

ifeq ($(OS),Windows_NT)
    EXE_FILE := $(EXE_BASE).exe
    EXT      := .exe
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

$(BUILD_DIR)/CMakeCache.txt:
	@$(CMAKE) -E echo "[cfg] gerador: $(GEN_LABEL)  |  build: $(BUILD)"
	@$(CMAKE) -E echo "[cfg] toolchain: $(if $(VCPKG_TC),$(VCPKG_TC),nenhum)"
	$(CMAKE) -B $(BUILD_DIR) -S . $(CONFIGURE_FLAGS)

configure: $(BUILD_DIR)/CMakeCache.txt

build: configure
	@$(CMAKE) -E echo "[build] compilando..."
	$(CMAKE) --build $(BUILD_DIR) --parallel
	@$(CMAKE) -E echo "[build] pronto: $(EXE)"

ARGS ?=

run: build
	@$(CMAKE) -E echo "[run] iniciando a simulacao (ESC para sair)"
	@$(CMAKE) -E chdir $(BUILD_DIR) ./$(EXE_FILE) $(ARGS)

shaders:
	@$(CMAKE) -E echo "[shaders] copiando shaders/ para $(BUILD_DIR)/shaders"
	@$(CMAKE) -E copy_directory shaders $(BUILD_DIR)/shaders

valida:
	@$(CMAKE) -E make_directory $(BUILD_DIR)
	@$(CMAKE) -E echo "[valida] compilando tools/valida_rk4.cpp"
	$(CXX) -O2 -std=c++17 tools/valida_rk4.cpp -o $(BUILD_DIR)/$(VALIDA_FILE)
	@$(CMAKE) -E chdir $(BUILD_DIR) ./$(VALIDA_FILE)

calibra:
	@$(CMAKE) -E make_directory $(BUILD_DIR)
	@$(CMAKE) -E echo "[calibra] compilando tools/calibra_lente.cpp"
	$(CXX) -O2 -std=c++17 tools/calibra_lente.cpp -o $(BUILD_DIR)/calibra_lente$(EXT)
	@$(CMAKE) -E chdir $(BUILD_DIR) ./calibra_lente$(EXT) B 3 ../shaders/lente_coef.glsl

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
