COMPILER=gcc
INPUT=$(shell find src -type f -iname '*.c')
OUTPUT=lime
STANDARD=11
OPTIMIZE=0

build:
	$(COMPILER) -o $(OUTPUT) -std=c$(STANDARD) -O$(OPTIMIZE) $(INPUT) -lm -ldl -Wl,--dynamic-list=export.list
