COMPILER=gcc
INPUT=$(shell find src -type f -iname '*.c')
OUTPUT=bowl
STANDARD=11
OPTIMIZE=0
INCLUDE=modules/bowl-api/include

build:
	$(COMPILER) -o $(OUTPUT) -std=c$(STANDARD) -O$(OPTIMIZE) $(INPUT) -I$(INCLUDE) -lm -ldl -Wl,--dynamic-list=export.list
