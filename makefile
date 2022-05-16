CXX_FLAGS = --std=c++14 -Wall -Wextra -Wpedantic

# External libraries
FMT = vendor/fmt
DOCTEST = vendor/doctest

INCLUDE = -I$(FMT)/include -I$(DOCTEST)/include
LIBS = -lmpi
SRCS = src/*.cpp $(FMT)/src/format.cc $(FMT)/src/os.cc

.DEFAULT_GOAL := build

run: build
	mpirun -np 10 ./bin/winemaker

build: compile
	mpicxx $(CXX_FLAGS) obj/*.o -o winemaker && mv winemaker bin/

compile: clean
	mpicxx -c $(CXX_FLAGS) $(INCLUDE) $(LIBS) $(SRCS) && mv *.o obj/

clean:
	rm -rf obj bin; mkdir obj bin bin/test

test:
	mpicxx -D__WINEMAKER_TEST__ $(CXX_FLAGS) $(INCLUDE) $(LIBS) $(SRCS) -o winemaker && mv winemaker ./bin/test/ && mpirun -np 1 ./bin/test/winemaker