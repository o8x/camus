SITE_ROOT ?= site
REPO_URL ?= git@github.com:o8x/o8x.github.io.git
UNAME_S := $(shell uname -s)

all: build

# 通用二进制 -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
.PHONY: package
package:
ifeq ($(UNAME_S), Linux)
	@cmake -DCMAKE_BUILD_TYPE=Release -DCPACK_GENERATOR="DEB" -G Ninja -B build/release -S .
	@cmake --build build/release --target all package
else ifeq ($(UNAME_S), Darwin)
	@cmake -DCMAKE_BUILD_TYPE=Release -DCPACK_GENERATOR="productbuild" -G Ninja -B build/release -S .
	@cmake --build build/release --target all package
	@cmake -DCMAKE_BUILD_TYPE=Release -DCPACK_GENERATOR="DragNDrop" -G Ninja -B build/release -S .
	@cmake --build build/release --target all package
else
	$(error Unsupported platform: $(UNAME_S))
endif

.PHONY: build
build:
	@cmake -DCMAKE_BUILD_TYPE=Release -G Ninja -B build/release -S .
	@cmake --build build/release --target all

.PHONY: serve
serve: build
	@ctest --extra-verbose -R build-test-site --test-dir build/release
	@echo "listen on http://localhost:8000"
	@python3 -m http.server --bind localhost 8000 --directory $(SITE_ROOT)/html

.PHONY: push
push: build
	@ctest --extra-verbose -R build-test-site --test-dir build/release
	@git -C $(SITE_ROOT)/html init
	@git -C $(SITE_ROOT)/html remote add origin $(REPO_URL)
	@git -C $(SITE_ROOT)/html add .
	@git -C $(SITE_ROOT)/html commit -qm "$(shell date +'%Y-%m-%d %H:%M:%S'), push project"
	@git -C $(SITE_ROOT)/html push -f origin main
