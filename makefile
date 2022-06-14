CXX_FLAGS = --std=c++17 -Wall -Wextra -Wpedantic -pthread

# External libraries
FMT = vendor/fmt
DOCTEST = vendor/doctest
SPDLOG = vendor/spdlog
TOML = vendor/toml

INCLUDE = -I$(FMT)/include -I$(SPDLOG)/include -I$(TOML)
LIBS = -L$(SPDLOG)/build -lspdlog -lmpi
SRCS = src/*.cpp $(FMT)/src/format.cc $(FMT)/src/os.cc

.DEFAULT_GOAL := build

run:
	mpirun -np 10 --oversubscribe ./bin/winemaker

build: compile
	mpicxx $(CXX_FLAGS) $(LIBS) *.o -o winemaker && mv winemaker bin/

compile: clean
	mpicxx -c $(CXX_FLAGS) -DNOUVEAUX_DEBUG $(INCLUDE) $(SRCS)

clean:
	rm -rf bin ./*.o; mkdir bin bin/test

test:
	mpicxx -D__WINEMAKER_TEST__ $(CXX_FLAGS) $(INCLUDE) $(LIBS) $(SRCS) -o winemaker && mv winemaker ./bin/test/ && mpirun -np 4 ./bin/test/winemaker

build_test_debug:
	mpicxx -D__WINEMAKER_TEST__ $(CXX_FLAGS) $(INCLUDE) $(LIBS) $(SRCS) -o winemaker && mv winemaker ./bin/test/

setup:
	mkdir vendor/spdlog/build && cd vendor/spdlog/build && cmake .. && make -j && cd ../../