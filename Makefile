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
USE_BGFX ?= 0
# bgfx builds add -DUSE_BGFX_RENDERER, which changes the compiled result, so
# they get their own object tree.  Sharing one tree would silently reuse
# objects built with the other setting (or fail to link).
ifeq ($(USE_BGFX),1)
OBJDIR   := obj-bgfx
else
OBJDIR   := obj
endif
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
    # The bgfx backend renders through bgfx itself (Metal on macOS) instead of
    # the game's own OpenGL context.  The embedded shaders are generated for
    # Metal (see tools/compile_shaders.sh), so the bgfx build is currently
    # macOS-only.
    ifneq ($(UNAME_S),Darwin)
        $(error USE_BGFX=1 is currently supported on macOS only (the bgfx backend renders with Metal; run the default `make` here, or generate shaders for another bgfx backend with tools/compile_shaders.sh))
    endif

    BGFX_HOME ?= $(HOME)/projects/bgfx
    ifeq ($(UNAME_S),Darwin)
        BGFX_PLATFORM_DIR ?= $(if $(filter x86_64,$(shell uname -m)),osx-x64,osx-arm64)
    else
        BGFX_PLATFORM_DIR ?= linux64
    endif
    BGFX_LIBDIR ?= $(BGFX_HOME)/.build/$(BGFX_PLATFORM_DIR)/bin

    BGFX_CFLAGS ?= -I$(BGFX_HOME)/include -I$(BGFX_HOME)/../bx/include -I$(BGFX_HOME)/../bimg/include
    # A statically linked bgfx calls into bimg (image parsing, format queries)
    # and bx, so all three archives must be on the link line -- a partial list
    # only shows up as "undefined symbols: bimg::..." much later.  If your
    # bgfx build additionally splits bimg into decode/encode archives, append
    # them to BGFX_LIBS.
    BGFX_LIBS ?= \
        $(BGFX_LIBDIR)/libbgfxRelease.a \
        $(BGFX_LIBDIR)/libbimgRelease.a \
        $(BGFX_LIBDIR)/libbxRelease.a
    ifeq ($(UNAME_S),Darwin)
        BGFX_LIBS += -framework Cocoa -framework IOKit -framework OpenGL -framework QuartzCore -weak_framework Metal -weak_framework MetalKit -weak_framework VideoToolbox -weak_framework CoreMedia -weak_framework CoreVideo
    endif


    CPPFLAGS += -DUSE_BGFX_RENDERER $(BGFX_CFLAGS)
    LIBS += $(BGFX_LIBS)
endif

# ---------------------------------------------------------------------------
# Sources / objects / dependencies
# ---------------------------------------------------------------------------
SRC_DIRS  := core entities render audio ui math vendor
SRC_FILES := $(foreach d,$(SRC_DIRS),$(wildcard src/$(d)/*.cpp))
# The bgfx build replaces the OpenGL backend with the bgfx one (whichever is not
# used would not even compile without the other's headers).
ifeq ($(USE_BGFX),1)
SRC_FILES := $(filter-out src/render/gl3.cpp,$(SRC_FILES))
else
SRC_FILES := $(filter-out src/render/bgfx_backend.cpp,$(SRC_FILES))
endif
OBJS      := $(patsubst %.cpp,$(OBJDIR)/%.o,$(SRC_FILES))
DEPS      := $(OBJS:.o=.d)

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
all: $(NAME)

bgfx-env:
	@echo "bgfx detection (USE_BGFX=$(USE_BGFX)):"
	@echo "  BGFX_HOME   = $(if $(strip $(BGFX_HOME)),$(BGFX_HOME),<not found>)"
	@echo "  BGFX_CFLAGS = $(if $(strip $(BGFX_CFLAGS)),$(BGFX_CFLAGS),<none>)"
	@echo "  BGFX_LIBDIR = $(if $(strip $(BGFX_LIBDIR)),$(BGFX_LIBDIR),<not found>)"
	@echo "  BGFX_LIBS   = $(if $(strip $(BGFX_LIBS)),$(BGFX_LIBS),<none>)"
	@echo "  OBJDIR      = $(OBJDIR)"
	@echo "A static bgfx needs libbgfx, libbimg and libbx on the link line"
	@echo "(bgfx calls into bimg and bx); pass BGFX_LIBS to override."

help:
	@echo "Trigonometry Wars build targets:"
	@echo "  make          - build the game (./$(NAME))"
	@echo "  make run      - build and run the game"
	@echo "  make bgfx     - build with the bgfx renderer enabled"
	@echo "  make run-bgfx - build and run with the bgfx renderer enabled"
	@echo "  make clean    - remove build objects and the binary"
	@echo "  make bgfx-env - show the detected bgfx paths/flags"
	@echo "  make help     - show this help text"

run: $(NAME)
	./$(NAME)

bgfx:
	$(MAKE) USE_BGFX=1 all

run-bgfx:
	$(MAKE) USE_BGFX=1 run

# ---------------------------------------------------------------------------
# Validated bgfx link flags (a partial list is the classic cause of
# "undefined symbols: bimg::..."); a no-op when building without bgfx.
# ---------------------------------------------------------------------------
bgfx-libs-check:
	@if [ -n "$(strip $(BGFX_LIBS))" ]; then sh tools/check_bgfx_libs.sh $(BGFX_LIBS); fi

$(NAME): $(OBJS) | bgfx-libs-check
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
	rm -rf obj obj-bgfx $(NAME)

# Load generated dependency files (skip during clean)
ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

.PHONY: all help bgfx-env run bgfx run-bgfx clean
