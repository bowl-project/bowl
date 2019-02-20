COMPILER=gcc
INPUT=src/*.c src/common/*.c src/machine/*.c
OUTPUT=lime
FLAGS=-O3 -std=c11

build:
	$(COMPILER) -o $(OUTPUT) $(FLAGS) $(INPUT) -lm -ldl
	./$(OUTPUT) invoke swap drop assets/bootstrap.lime show read tokens swap continue
	
