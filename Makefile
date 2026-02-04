.PHONY: all build release test install clean

all: test

build:
	mkdir -p build
	cd build && cmake -DCMAKE_BUILD_TYPE=Debug -G "Unix Makefiles" ..
	cd build && make

release:
	mkdir -p build
	cd build && cmake -G "Unix Makefiles" ..
	cd build && make

test: build
	./build/tnc_test

install: release
	cd build && sudo make install

clean:
	rm -rf build
