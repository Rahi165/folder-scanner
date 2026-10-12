# =============================================================================
# Makefile — Multithreaded Web Crawler Pipeline
# Cross-platform: macOS (clang/Homebrew) + Linux + Windows (MSYS2/MinGW-w64)
#
# QUICK START
# -----------
# macOS / Linux:
#   make all
#
# Windows (run inside MSYS2 MinGW64 shell):
#   make all
#
# macOS/Linux dependencies:
#   brew install pkg-config curl libxml2        # macOS Homebrew
#   sudo apt install libcurl4-openssl-dev libxml2-dev   # Debian/Ubuntu
#   sudo dnf install libcurl-devel libxml2-devel        # Fedora/RHEL
#
# Windows (MSYS2) dependencies — run once inside the MSYS2 MinGW64 shell:
#   pacman -S mingw-w64-x86_64-curl mingw-w64-x86_64-libxml2 \
#             mingw-w64-x86_64-pkg-config
# =============================================================================

# ---------------------------------------------------------------------------
# Platform detection
# ---------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
  PLATFORM  := windows
  EXE       := .exe
  RM        := del /Q /F
  RMDIR     := rmdir /S /Q
  MKDIR     := mkdir
  PATHSEP   := \\
  # On Windows we need Winsock; AF_UNIX sockets require Windows 10 1803+
  PLAT_LIBS := -lws2_32
else
  UNAME := $(shell uname -s)
  ifeq ($(UNAME),Darwin)
    PLATFORM := macos
  else
    PLATFORM := linux
  endif
  EXE       :=
  RM        := rm -f
  RMDIR     := rm -rf
  MKDIR     := mkdir -p
  PATHSEP   := /
  PLAT_LIBS :=
endif

# ---------------------------------------------------------------------------
# Compiler selection
# ---------------------------------------------------------------------------
ifeq ($(PLATFORM),macos)
  CC := clang
else
  # On Linux and MSYS2/MinGW use gcc (or override: make CC=clang)
  CC := gcc
endif

STD  := -std=c11
WARN := -Wall -Wextra
OPT  := -O2

# _POSIX_C_SOURCE is not meaningful on Windows but doesn't hurt MinGW
ifeq ($(PLATFORM),windows)
  DEFS := -D_WIN32_WINNT=0x0A00
else
  DEFS := -D_POSIX_C_SOURCE=200809L
endif

# ---------------------------------------------------------------------------
# Dependency detection: libcurl + libxml2
# ---------------------------------------------------------------------------
# Strategy (tried in order):
#   1. pkg-config  — works everywhere when libraries are properly installed
#   2. Homebrew keg-only paths (macOS only)
#   3. Bare -l flags (system-wide install, e.g. Linux apt/dnf or MSYS2)

PKGCFG_CF      := $(shell pkg-config --cflags libcurl libxml-2.0 2>/dev/null)
PKGCFG_LIB_ALL := $(shell pkg-config --libs   libcurl libxml-2.0 2>/dev/null)
PKGCFG_LIB_XML := $(shell pkg-config --libs   libxml-2.0         2>/dev/null)

ifneq ($(strip $(PKGCFG_CF)),)
  # pkg-config found both — use it everywhere
  EXT_CFLAGS   := $(PKGCFG_CF)
  LIBS_CURL    := $(PKGCFG_LIB_ALL)
  LIBS_XML2    := $(PKGCFG_LIB_XML)

else ifeq ($(PLATFORM),macos)
  # Homebrew fallback on macOS
  SDK          := $(shell xcrun --show-sdk-path 2>/dev/null)
  BREW_CURL    := $(shell brew --prefix curl    2>/dev/null)
  BREW_XML2    := $(shell brew --prefix libxml2 2>/dev/null)

  ifneq ($(wildcard $(BREW_CURL)/lib/libcurl.dylib $(BREW_CURL)/lib/libcurl.a),)
    EXT_CFLAGS := -I$(BREW_CURL)/include -I$(BREW_XML2)/include/libxml2
    LIBS_CURL  := -L$(BREW_CURL)/lib -lcurl -L$(BREW_XML2)/lib -lxml2
    LIBS_XML2  := -L$(BREW_XML2)/lib -lxml2
  else
    # macOS system SDK always ships curl + libxml2
    EXT_CFLAGS := $(if $(SDK),-I$(SDK)/usr/include/libxml2)
    LIBS_CURL  := -lcurl -lxml2
    LIBS_XML2  := -lxml2
  endif

else
  # Linux or Windows without pkg-config — hope they're on the standard path
  EXT_CFLAGS   :=
  LIBS_CURL    := -lcurl -lxml2
  LIBS_XML2    := -lxml2
endif

# ---------------------------------------------------------------------------
# pthreads
# ---------------------------------------------------------------------------
ifeq ($(PLATFORM),windows)
  # MinGW ships winpthreads; -lpthread links it automatically
  PTHREAD_FLAGS :=
  PTHREAD_LIBS  := -lpthread
else
  PTHREAD_FLAGS :=
  PTHREAD_LIBS  := -lpthread
endif

# ---------------------------------------------------------------------------
# Assembled flags
# ---------------------------------------------------------------------------
# -I. makes "common/ipc_proto.h", "crawler/queue.h", etc. resolve correctly
# from the project root, regardless of which sub-directory the source lives in.
CFLAGS         := $(STD) $(WARN) $(OPT) $(DEFS) -I. $(EXT_CFLAGS)

LIBS_CRAWLER   := $(LIBS_CURL)  $(PTHREAD_LIBS) $(PLAT_LIBS)
LIBS_INDEXER   := $(LIBS_XML2)  $(PTHREAD_LIBS) $(PLAT_LIBS)
LIBS_QUERY     := $(PLAT_LIBS)

# ---------------------------------------------------------------------------
# Source files
# The zip layout is flat (crawler/, indexer/, query/) rather than src/,
# so we reference files from those directories directly.
# ---------------------------------------------------------------------------
CRAWLER_SRCS := crawler/main.c      \
                crawler/queue.c     \
                crawler/visited.c   \
                crawler/fetch.c     \
                crawler/parse.c     \
                crawler/ipc_client.c

INDEXER_SRCS := indexer/main.c      \
                indexer/ipc_server.c \
                indexer/tokenizer.c  \
                indexer/indexer.c

QUERY_SRCS   := query/main.c        \
                query/index_reader.c

# ---------------------------------------------------------------------------
# Output paths
# ---------------------------------------------------------------------------
BUILD   := build
CRAWLER := $(BUILD)/crawler$(EXE)
INDEXER := $(BUILD)/indexer$(EXE)
QUERY   := $(BUILD)/query$(EXE)

# IPC socket / named pipe path differs by platform
ifeq ($(PLATFORM),windows)
  IPC_PATH := \\\\pipe\\\\webcrawler
else
  IPC_PATH := /tmp/webcrawler.sock
endif

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
.PHONY: all clean dirs run help check-deps

all: check-deps dirs $(CRAWLER) $(INDEXER) $(QUERY)
	@echo ""
	@echo "Build complete. Binaries are in $(BUILD)/"
	@echo "Run 'make run' for a quick end-to-end demo."

# Friendly error if critical tools are missing
check-deps:
ifeq ($(PLATFORM),windows)
	@where gcc >NUL 2>&1 || (echo "ERROR: gcc not found. Open an MSYS2 MinGW64 shell." && exit 1)
else
	@command -v $(CC) >/dev/null 2>&1 || \
	  (echo "ERROR: $(CC) not found. Install Xcode Command Line Tools or build-essential." && exit 1)
	@command -v pkg-config >/dev/null 2>&1 || \
	  echo "WARNING: pkg-config not found. Library detection may fall back to bare -l flags."
endif

dirs:
ifeq ($(PLATFORM),windows)
	@if not exist $(BUILD)     $(MKDIR) $(BUILD)
	@if not exist data\pages   $(MKDIR) data\pages
	@if not exist index        $(MKDIR) index
else
	@$(MKDIR) $(BUILD) data/pages index
endif

$(CRAWLER): $(CRAWLER_SRCS) common/ipc_proto.h \
            crawler/queue.h crawler/visited.h   \
            crawler/fetch.h crawler/parse.h     \
            crawler/ipc_client.h
	$(CC) $(CFLAGS) -o $@ $(CRAWLER_SRCS) $(LIBS_CRAWLER)
	@echo "  -> Built $@"

$(INDEXER): $(INDEXER_SRCS) common/ipc_proto.h  \
            indexer/ipc_server.h indexer/tokenizer.h indexer/indexer.h
	$(CC) $(CFLAGS) -o $@ $(INDEXER_SRCS) $(LIBS_INDEXER)
	@echo "  -> Built $@"

$(QUERY): $(QUERY_SRCS) query/index_reader.h
	$(CC) $(CFLAGS) -o $@ $(QUERY_SRCS) $(LIBS_QUERY)
	@echo "  -> Built $@"

clean:
ifeq ($(PLATFORM),windows)
	@if exist $(BUILD) $(RMDIR) $(BUILD)
	@if exist data     $(RMDIR) data
	@if exist index    $(RMDIR) index
else
	$(RMDIR) $(BUILD)/ data/ index/
endif
	@echo "Cleaned."

# ---------------------------------------------------------------------------
# End-to-end demo
# NOTE: On Windows the background & trick doesn't work in cmd.exe.
#       Use an MSYS2 shell or run indexer and crawler in separate terminals.
# ---------------------------------------------------------------------------
run: all
	@echo "=== Starting indexer in background ==="
ifeq ($(PLATFORM),windows)
	@echo "Windows detected: start indexer manually in a second terminal:"
	@echo "  $(INDEXER) --ipc $(IPC_PATH) --out ."
	@echo "Then run the crawler:"
	@echo "  $(CRAWLER) --seed https://example.com --max-depth 2 --max-pages 50 -t 4 --out . --ipc $(IPC_PATH)"
else
	$(INDEXER) --ipc $(IPC_PATH) --out . &
	@sleep 0.5
	@echo "=== Starting crawler ==="
	$(CRAWLER) --seed https://example.com --max-depth 2 \
	    --max-pages 50 -t 4 --out . --ipc $(IPC_PATH)
	@sleep 1
	@echo "=== Querying index ==="
	$(QUERY) --index . web example domain
endif

help:
	@echo "============================================================"
	@echo " Multithreaded Web Crawler — Makefile Help"
	@echo "============================================================"
	@echo ""
	@echo "Targets:"
	@echo "  make all     Build crawler, indexer, and query (default)"
	@echo "  make clean   Remove build/, data/, index/"
	@echo "  make run     Build then run a quick demo against example.com"
	@echo "  make help    Show this message"
	@echo ""
	@echo "Platform detected: $(PLATFORM)"
	@echo "Compiler:          $(CC)"
	@echo ""
	@echo "Usage after build:"
	@echo ""
	@echo "  Terminal 1 — start indexer first:"
	@echo "    $(INDEXER) --ipc $(IPC_PATH) --out <outdir>"
	@echo ""
	@echo "  Terminal 2 — run crawler:"
	@echo "    $(CRAWLER) --seed <url> --max-depth <D> \\"
	@echo "        --max-pages <N> -t <threads> \\"
	@echo "        --out <outdir> --ipc $(IPC_PATH)"
	@echo ""
	@echo "  Query the index:"
	@echo "    $(QUERY) --index <outdir> <term1> [term2 ...]"
	@echo ""
	@echo "Install dependencies:"
	@echo "  macOS:   brew install pkg-config curl libxml2"
	@echo "  Ubuntu:  sudo apt install libcurl4-openssl-dev libxml2-dev"
	@echo "  Fedora:  sudo dnf install libcurl-devel libxml2-devel"
	@echo "  Windows: (in MSYS2 MinGW64 shell)"
	@echo "    pacman -S mingw-w64-x86_64-curl mingw-w64-x86_64-libxml2 \\"
	@echo "              mingw-w64-x86_64-pkg-config"
	@echo "============================================================"
