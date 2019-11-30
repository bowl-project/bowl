COMPILER=gcc
INPUT=$(shell find src -type f -iname '*.c')
OUTPUT=io.so
STANDARD=11
OPTIMIZE=0

build:
	$(COMPILER) -shared -fPIC -o $(OUTPUT) -std=c$(STANDARD) -O$(OPTIMIZE) $(INPUT)


