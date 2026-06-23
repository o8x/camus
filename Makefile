REPO_URL ?= git@github.com:o8x/o8x.github.io.git
REPO_DOMAIN ?= stdout.com.cn

all: build

configure:
	@cmake -G Ninja -B cmake-build-debug -S .

build: configure
	@cmake --build cmake-build-debug --target all -j 10

release:
	@cmake -DCMAKE_BUILD_TYPE=MinSizeRel -G Ninja -B cmake-build-release -S .
	@cmake --build cmake-build-release --target all -j 10

push: build
	cmake-build-debug/Camus site
	echo "$(REPO_DOMAIN)" > site/html/CNAME
	git -C site/html init
	git -C site/html remote add origin $(REPO_URL)
	git -C site/html add .
	git -C site/html commit -qm "init project"
	git -C site/html push -f origin main
