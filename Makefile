# Trigonometry Wars
#
# Canonical cross-platform makefile. `make` auto-detects the OS and picks up
# SDL3 from either the macOS framework install or pkg-config (Homebrew /
# Linux). Platform-specific makefiles (makefile.macos/.linux/.win/.os4) are
# also provided and remain supported.
#
# Targets:
#   make        - build the game (produces ./trigwars)
#   make run    - build and run the game
#   make bgfx   - build with the bgfx renderer enabled
#   make run-bgfx - build and run with the bgfx renderer enabled
#   make clean  - remove build objects and the binary
#   make help   - show this help text

CXX      ?= c++
NAME     := trigwars
OBJDIR   := obj
USE_BGFX ?= 0
PKG_CONFIG ?= pkg-config

CXXFLAGS := -std=c++20 -Wall -Wextra -O3 -ggdb
CPPFLAGS := -Isrc

# Optional Clang analyzer / sanitizers (off by default, e.g.):
#   make CLANG_ADDRESS=-fsanitize=address
CLANG_ANALYZE   :=
CLANG_THREAD    :=
CLANG_ADDRESS   :=
CLANG_MEMORY    :=
CLANG_UNDEFINED :=
CLANG_FLAGS     := $(CLANG_ANALYZE) $(CLANG_THREAD) $(CLANG_ADDRESS) \
                   $(CLANG_MEMORY) $(CLANG_UNDEFINED)

# ---------------------------------------------------------------------------
# Platform detection
# ---------------------------------------------------------------------------
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    # Prefer the official SDL3.framework if installed; otherwise fall back to
    # pkg-config (e.g. Homebrew: `brew install sdl3`).
    SDL3_FRAMEWORK := $(wildcard /Library/Frameworks/SDL3.framework)
    ifneq ($(SDL3_FRAMEWORK),)
        SDL_CFLAGS := -F/Library/Frameworks
        SDL_LIBS   := -F/Library/Frameworks -framework SDL3 -rpath /Library/Frameworks
    else
        SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl3)
        SDL_LIBS   := $(shell $(PKG_CONFIG) --libs sdl3)
    endif
    LIBS := $(SDL_LIBS) -framework OpenGL
else
    SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl3)
    SDL_LIBS   := $(shell $(PKG_CONFIG) --libs sdl3)
    LIBS       := $(SDL_LIBS) -lGL
endif

CPPFLAGS += $(SDL_CFLAGS)

ifeq ($(USE_BGFX),1)
    BGFX_HOME ?=

    BGFX_PKG ?= $(shell \
        if $(PKG_CONFIG) --exists bgfx 2>/dev/null; then \
            echo bgfx; \
        elif $(PKG_CONFIG) --exists bgfx-shared 2>/dev/null; then \
            echo bgfx-shared; \
        fi)

    ifneq ($(strip $(BGFX_HOME)),)
        BGFX_CFLAGS ?= -I$(BGFX_HOME)/include -I$(BGFX_HOME)/../bx/include -I$(BGFX_HOME)/../bimg/include
        BGFX_LIBDIR ?= $(shell \
            if [ -d "$(BGFX_HOME)/.build/osx-arm64/bin" ]; then \
                echo "$(BGFX_HOME)/.build/osx-arm64/bin"; \
            elif [ -d "$(BGFX_HOME)/.build/linux64/bin" ]; then \
                echo "$(BGFX_HOME)/.build/linux64/bin"; \
            elif [ -d "$(BGFX_HOME)/.build/x64/bin" ]; then \
                echo "$(BGFX_HOME)/.build/x64/bin"; \
            elif [ -d "$(BGFX_HOME)/.build/bin" ]; then \
                echo "$(BGFX_HOME)/.build/bin"; \
            fi)
        BGFX_LIBS ?= $(if $(strip $(BGFX_LIBDIR)), \
            $(foreach lib,bgfx bimg bx, \
                $(firstword $(wildcard $(BGFX_LIBDIR)/lib$(lib)Release.a $(BGFX_LIBDIR)/lib$(lib).a)) \
            ) \
        )
        ifeq ($(UNAME_S),Darwin)
            BGFX_LIBS += -framework Cocoa -framework IOKit -framework OpenGL -framework QuartzCore -weak_framework Metal -weak_framework MetalKit -weak_framework VideoToolbox -weak_framework CoreMedia -weak_framework CoreVideo
        endif
    else ifneq ($(strip $(BGFX_CFLAGS) $(BGFX_LIBS)),)
        # Explicit bgfx flags were provided directly; do not guess a source tree.
    else
        BGFX_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags $(BGFX_PKG) 2>/dev/null)
        BGFX_LIBS   ?= $(shell $(PKG_CONFIG) --libs $(BGFX_PKG) 2>/dev/null)
    endif

    ifeq ($(strip $(BGFX_CFLAGS) $(BGFX_LIBS)),)
        $(error USE_BGFX=1 requested, but bgfx metadata or local checkout was not found. Set BGFX_HOME to a bgfx source root, or set BGFX_CFLAGS and BGFX_LIBS explicitly.)
    endif
    CPPFLAGS += -DUSE_BGFX_RENDERER $(BGFX_CFLAGS)
    LIBS += $(BGFX_LIBS)
endif

# ---------------------------------------------------------------------------
# Sources / objects / dependencies
# ---------------------------------------------------------------------------
SRC_DIRS  := core entities render audio ui math vendor
SRC_FILES := $(foreach d,$(SRC_DIRS),$(wildcard src/$(d)/*.cpp))
OBJS      := $(patsubst %.cpp,$(OBJDIR)/%.o,$(SRC_FILES))
DEPS      := $(OBJS:.o=.d)

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
all: $(NAME)

help:
	@echo "Trigonometry Wars build targets:"
	@echo "  make          - build the game (./$(NAME))"
	@echo "  make run      - build and run the game"
	@echo "  make bgfx     - build with the bgfx renderer enabled"
	@echo "  make run-bgfx - build and run with the bgfx renderer enabled"
	@echo "  make clean    - remove build objects and the binary"
	@echo "  make help     - show this help text"

run: $(NAME)
	./$(NAME)

bgfx:
	$(MAKE) USE_BGFX=1 all

run-bgfx:
	$(MAKE) USE_BGFX=1 run

$(NAME): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LIBS) $(CLANG_FLAGS)

# ---------------------------------------------------------------------------
# Pattern rules
# ---------------------------------------------------------------------------
$(OBJDIR):
	mkdir -p $@

$(OBJDIR)/%.d: %.cpp | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CXX) -MM -MP -MT $(@:.d=.o) -o $@ $< $(CXXFLAGS) $(CPPFLAGS)

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CXX) -o $@ -c $< $(CXXFLAGS) $(CLANG_FLAGS) $(CPPFLAGS)

# ---------------------------------------------------------------------------
# Cleanup
# ---------------------------------------------------------------------------
clean:
	rm -rf $(OBJDIR) $(NAME)

# Load generated dependency files (skip during clean)
ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

.PHONY: all help run bgfx run-bgfx clean
