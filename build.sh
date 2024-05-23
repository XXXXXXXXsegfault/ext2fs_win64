#!/bin/bash -ev
build_program()
{
	gcc $1/main.c -S -o build/cc.s -nostdlib -fno-builtin -O2 -Dgetch=_getch -Dmain=__main
	sed -i "s/.*call.*__chkstk_ms$//" build/cc.s
	as build/cc.s -o build/cc.o
	ld build/cc.o -o build/$1.exe -L/cygdrive/c/Windows/System32 -lmsvcrt -lkernel32 --image-base=0x400000 --subsystem=3 -e__main
	strip build/$1.exe 
}
mkdir -p build
build_program ext2_cli
build_program ext2_format
build_program ext4_format
