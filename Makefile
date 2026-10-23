# Linyrics — local development build (SDL3 + libcurl).
#   make          -> build/linyrics
#   make run      -> build + run
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -g0 -Wall -Wextra -Wno-unused-parameter
PKGS      = sdl3 libcurl
VERSION    = 1.0.0

PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS))
PKG_LIBS   := $(shell pkg-config --libs $(PKGS))

SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp,build/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

all: build/linyrics

build/linyrics: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(PKG_LIBS)

build/%.o: src/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -MMD -MP -DLINYRICS_VERSION='"$(VERSION)"' $(PKG_CFLAGS) -c $< -o $@

run: build/linyrics
	./build/linyrics

clean:
	rm -rf build

-include $(DEPS)

.PHONY: all run clean
