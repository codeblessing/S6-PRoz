CXX_FLAGS = --std=c++17 -Wall -Wextra -Wpedantic -pthread

# External libraries
SPDLOG = vendor/spdlog
TOML = vendor/toml

INCLUDE = -I$(SPDLOG)/include -I$(TOML)
LIBS = -L$(SPDLOG)/build -lspdlog -lmpi
SRCS = src/*.cpp

.DEFAULT_GOAL := build

run:
	mpirun -np 10 --oversubscribe ./bin/winemaker

build: compile
	mpicxx -O3 $(CXX_FLAGS) $(LIBS) *.o -o winemaker && mv winemaker bin/

compile: clean
	mpicxx -c $(CXX_FLAGS) -DNOUVEAUX_DEBUG $(INCLUDE) $(SRCS)

clean:
	rm -rf bin ./*.o; mkdir bin

setup:
	mkdir vendor/spdlog/build && cd vendor/spdlog/build && cmake .. && make -j && cd ../../