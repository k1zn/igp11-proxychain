# Makefile for proxychain (MinGW-w64 / w64devkit, x86_64 only)
#
# Usage:
#   1. Generate the .def for your specific original DLL:
#        python tools/gen_def.py path/to/iGP11.Direct3D11.dll iGP11_Direct3D11_real src/proxy.def
#   2. Build:
#        make
#   3. The resulting iGP11.Direct3D11.dll is in the project root.

OUT      := iGP11.Direct3D11.dll
DEF      := src/proxy.def
SOURCES  := src/dllmain.cpp src/config.cpp src/log.cpp src/iat_patch.cpp

# Renamed original DLL name. Must match the forward target you passed to
# gen_def.py (with .dll suffix). Override on the command line, e.g.:
#   make REAL_DLL=iGP11_orig.dll
#
# IMPORTANT: the renamed-original filename must NOT contain dots beyond the
# .dll extension. Windows' loader splits forwarder strings on the first dot,
# so a name like "iGP11.Direct3D11.real.dll" makes forwarders unresolvable.
REAL_DLL ?= iGP11_Direct3D11_real.dll

CXX      := g++
CXXFLAGS := -std=c++17 -Os -Wall -Wextra -Wno-unused-parameter \
            -fno-exceptions -fno-rtti -fvisibility=hidden \
            -ffunction-sections -fdata-sections \
            -DPROXYCHAIN_REAL_DLL_W=L\"$(REAL_DLL)\" \
            -DUNICODE -D_UNICODE \
            -DWIN32_LEAN_AND_MEAN -DNOMINMAX
LDFLAGS  := -shared -m64 \
            -static -static-libgcc -static-libstdc++ \
            -Wl,--kill-at \
            -Wl,--enable-stdcall-fixup \
            -Wl,--gc-sections \
            -Wl,-s
LIBS     := -lkernel32 -luser32

OBJS     := $(SOURCES:src/%.cpp=build/%.o)

.PHONY: all clean check-def help

all: $(OUT)

help:
	@echo "iGP11 proxychain - build targets"
	@echo "  make             - build $(OUT) (forwards to $(REAL_DLL))"
	@echo "  make clean       - remove build/ and the output DLL"
	@echo "  make REAL_DLL=X  - override the renamed-original filename"
	@echo
	@echo "Before building, ensure src/proxy.def exists. The committed copy"
	@echo "matches stock iGP11 (3 exports: get, start, stop). For a different"
	@echo "iGP11 build, regenerate it:"
	@echo "  python tools/gen_def.py path/to/iGP11.Direct3D11.dll \\"
	@echo "                          $(basename $(REAL_DLL)) $(DEF)"

build:
	@mkdir -p build

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT): check-def $(OBJS) $(DEF)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(DEF) $(LIBS)
	@echo
	@echo "  built $(OUT) -> forwards to $(REAL_DLL)"
	@echo

check-def:
	@if [ ! -f "$(DEF)" ]; then \
	    echo "ERROR: $(DEF) not found. Run 'make help' for instructions."; \
	    exit 1; \
	fi

clean:
	rm -rf build $(OUT)
