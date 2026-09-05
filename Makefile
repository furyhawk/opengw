# OpenGW - Open Geometry Wars
#
# Canonical cross-platform makefile. `make` auto-detects the OS and picks up
# SDL3 from either the macOS framework install or pkg-config (Homebrew /
# Linux). Platform-specific makefiles (makefile.macos/.linux/.win/.os4) are
# also provided and remain supported.
#
# Targets:
#   make        - build the game (produces ./opengw)
#   make run    - build and run the game
#   make clean  - remove build objects and the binary
#   make help   - show this help text

CXX      ?= c++
NAME     := opengw
OBJDIR   := obj

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
        SDL_CFLAGS := $(shell pkg-config --cflags sdl3)
        SDL_LIBS   := $(shell pkg-config --libs sdl3)
    endif
    LIBS := $(SDL_LIBS) -framework OpenGL
else
    SDL_CFLAGS := $(shell pkg-config --cflags sdl3)
    SDL_LIBS   := $(shell pkg-config --libs sdl3)
    LIBS       := $(SDL_LIBS) -lGL
endif

CPPFLAGS += $(SDL_CFLAGS)

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
	@echo "OpenGW build targets:"
	@echo "  make          - build the game (./$(NAME))"
	@echo "  make run      - build and run the game"
	@echo "  make clean    - remove build objects and the binary"
	@echo "  make help     - show this help text"

run: $(NAME)
	./$(NAME)

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

.PHONY: all help run clean
