COMPILER=gcc
INPUT=$(shell find src -type f -iname '*.c')
OUTPUT=lime
STANDARD=11
OPTIMIZE=0
INCLUDE=modules/lime-api/include

build: settings/libsettings.a
	$(COMPILER) -o $(OUTPUT) -std=c$(STANDARD) -O$(OPTIMIZE) $(INPUT) -I$(INCLUDE) -Lsettings/ -lsettings -lm -ldl -Wl,--dynamic-list=export.list

settings/libsettings.a:
	$(COMPILER) -c settings/settings.c -o settings/settings.o
	ar -rc settings/libsettings.a settings/settings.o
