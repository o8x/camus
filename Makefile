SITE_ROOT ?= site
REPO_URL ?= git@github.com:o8x/o8x.github.io.git

all: build

.PHONY: build
build:
	@cmake -DCMAKE_BUILD_TYPE=MinSizeRel -G Ninja -B build/release -S .
	@cmake --build build/release --target all -j 10

.PHONY: serve
serve: build
	@build/release/camus/Camus $(SITE_ROOT)
	@echo "listen on http://localhost:8000"
	@python3 -m http.server --bind localhost 8000 --directory $(SITE_ROOT)/html

.PHONY: push
push: build
	@build/release/camus/Camus $(SITE_ROOT)
	@git -C $(SITE_ROOT)/html init
	@git -C $(SITE_ROOT)/html remote add origin $(REPO_URL)
	@git -C $(SITE_ROOT)/html add .
	@git -C $(SITE_ROOT)/html commit -qm "$(shell date +'%Y-%m-%d %H:%M:%S'), push project"
	@git -C $(SITE_ROOT)/html push -f origin main
