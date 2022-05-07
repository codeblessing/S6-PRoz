FILES = main.cpp winemaker.cpp
OBJ_FILES = obj/main.o obj/winemaker.o

CXX_FLAGS = --std=c++11 -Wall -Wextra -Wpedantic

.DEFAULT_GOAL := build

build: compile
	mpicxx $(CXX_FLAGS) $(OBJ_FILES) -o winemaker

compile:
	mpicxx -c $(CXX_FLAGS) $(FILES) && mv *.o obj/

clean:
	rm -f obj/*.o