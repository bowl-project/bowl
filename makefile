COMPILER=gcc
INPUT=src/*.c src/common/*.c src/machine/*.c
OUTPUT=lime
FLAGS=-O3 -std=c11

build:
	$(COMPILER) -o $(OUTPUT) $(FLAGS) $(INPUT) -lm -ldl
	clear
	time ./$(OUTPUT) nil quote continue prepend quote swap prepend quote tokens prepend quote read prepend quote show prepend quote assets/bootstrap.lime prepend quote drop prepend quote swap prepend invoke
