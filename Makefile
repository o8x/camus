all: configure build

configure: 
	@cmake -G Ninja -B cmake-build-debug -S .

build:
	@cmake --build cmake-build-debug --target all -j 10

release:
	@cmake -DCMAKE_BUILD_TYPE=MinSizeRel -G Ninja -B cmake-build-release -S .
	@cmake --build cmake-build-release --target all -j 10