# Makefile — Disease's Doomsday (Linux / macOS)
# Equivalente multiplataforma ao compilar.bat (Windows).
#
# Uso:
#   make           # compila -> ./rpg_game
#   make run       # compila e executa
#   make clean     # remove o executável
#
# Requer a Raylib instalada e detectável via pkg-config (Linux) ou Homebrew (macOS).
#   Linux:  sudo apt install libraylib-dev   (ou compile a Raylib do código-fonte)
#   macOS:  brew install raylib

CC      ?= gcc
TARGET   = rpg_game
CFLAGS   = -O2 -Wall -Wno-missing-braces -Iinclude

# Lista explícita de fontes (mesma do compilar.bat). Os arquivos
# telas_common.c / debug_patch.c são legado e NÃO entram no build.
SRCS = \
  src/main.c \
  src/logic/update_gameplay.c \
  src/logic/fsm.c \
  src/gameplay/input_controller.c \
  src/gameplay/spatial_grid.c \
  src/render/render_gameplay.c \
  src/entities/player.c \
  src/entities/enemy.c \
  src/entities/projectiles.c \
  src/systems/combat_system.c \
  src/systems/wave_manager.c \
  src/telas/telas.c \
  src/telas/tela_quiz.c \
  src/telas/tela_upgrade.c \
  src/telas/tela_arsenal.c \
  src/telas/tela_skins.c \
  src/telas/tela_admin.c \
  src/assets/asset_manager.c \
  src/assets/sprite_manager.c \
  Assets/@models/player_model.c \
  Assets/@models/enemy_model.c \
  Assets/@models/weapons_model.c \
  Assets/@models/doctor_model.c \
  Assets/@models/cosmetics.c \
  Assets/Maps/map_seringa.c \
  Assets/Maps/map_body.c

# ---------------------------------------------------------------------------
# Detecção de plataforma e flags de link da Raylib
# ---------------------------------------------------------------------------
UNAME_S := $(shell uname -s)

# Tenta usar pkg-config; se existir, ele resolve includes e libs corretamente.
PKG_RAYLIB := $(shell pkg-config --exists raylib 2>/dev/null && echo yes)

ifeq ($(PKG_RAYLIB),yes)
  CFLAGS += $(shell pkg-config --cflags raylib)
  LIBS    = $(shell pkg-config --libs raylib) -lm
else ifeq ($(UNAME_S),Darwin)
  # macOS sem pkg-config: usa frameworks do sistema
  CFLAGS += -I/opt/homebrew/include -I/usr/local/include
  LIBS = -L/opt/homebrew/lib -L/usr/local/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit \
         -framework CoreAudio -framework CoreVideo -lm
else
  # Linux sem pkg-config: link manual padrão da Raylib
  LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif

# ---------------------------------------------------------------------------
.PHONY: all run clean test collision-mask
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(SRCS) -o $(TARGET) $(CFLAGS) $(LIBS)
	@echo "[OK] Compilacao concluida -> ./$(TARGET)"

run: $(TARGET)
	./$(TARGET)

# Regenera Assets/Maps/map_body_mask.h a partir de Assets/Sprites/Map/corpo.png
# usando a MESMA transformacao imagem->mundo do renderer (map_body.h). Determinis-
# tico e reproduzivel: rode sempre que corpo.png ou a transformacao mudar.
collision-mask:
	$(CC) -O2 $(CFLAGS) tools/bake_collision_mask.c -o bake_collision_mask $(LIBS)
	./bake_collision_mask
	@echo "[OK] map_body_mask.h regenerado a partir de corpo.png"

# Teste de ACEITACAO (Etapa 2): simula muitos spawns de chefe em posicoes
# aleatorias e valida que TODOS os Nucleos de Infeccao nascem dentro do corpo e
# sao alcancaveis por melee. Usa stubs de desenho (nao precisa de janela/X11).
# Compile com AddressSanitizer adicionando: make test CC="gcc -fsanitize=address,undefined"
test:
	$(CC) -O2 $(CFLAGS) tests/test_core_spawns.c Assets/Maps/map_body.c -o test_core_spawns -lm
	./test_core_spawns
	$(CC) -O2 $(CFLAGS) tests/test_map_body.c Assets/Maps/map_body.c -o test_map_body -lm
	./test_map_body
	$(CC) -O2 $(CFLAGS) tests/test_rifle_range.c src/entities/projectiles.c Assets/Maps/map_body.c -o test_rifle_range -lm
	./test_rifle_range
	$(CC) -O2 $(CFLAGS) tests/test_cosmetics.c Assets/@models/cosmetics.c -o test_cosmetics -lm
	./test_cosmetics
	$(CC) -O2 $(CFLAGS) tests/test_virus_waves.c src/systems/wave_manager.c src/entities/enemy.c Assets/Maps/map_body.c -o test_virus_waves -lm
	./test_virus_waves

# ---------------------------------------------------------------------------
# Build de RELEASE para Windows (.exe) — usado pelo GitHub Actions.
# Reutiliza a MESMA lista $(SRCS) acima, entao NUNCA fica desatualizado em
# relacao ao codigo. Gera um rpg_game.exe AUTONOMO (raylib + runtime do GCC
# linkados estaticamente): o amigo so baixa 1 arquivo e da dois cliques.
# Requer MSYS2/MINGW64 com: gcc, raylib, pkgconf, make.
# ---------------------------------------------------------------------------
win-release: $(SRCS)
	$(CC) $(SRCS) -o $(TARGET).exe -O2 -Wall -Wno-missing-braces -Iinclude \
	  $(shell pkg-config --cflags raylib) \
	  $(shell pkg-config --static --libs raylib) \
	  -lopengl32 -lgdi32 -lwinmm -lm -static -static-libgcc
	@echo "[OK] $(TARGET).exe (standalone) gerado."

clean:
	rm -f $(TARGET) test_core_spawns test_map_body test_rifle_range test_cosmetics test_virus_waves bake_collision_mask
	@echo "[OK] Limpeza concluida."
