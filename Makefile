
#
# Makefile for lgp
#

# Use gcc compiler 
CC = gcc 
# compile for debug
# (to profile, also use -pg, then can run:  gprof lgp)
# the -m32 is needed to compile for IA32, we use IA32 FPU operations.
# also as of Ubuntu 16.04 need to:  apt-get install gcc-multilib
CFLAGS = -g -m32
#CFLAGS = -O4 -m32 -funroll-loops -I.

# Requires:
#   libdisasm.so, supplied with libdisasm_0.21-pre2 from bastard.sf.net
# (https://sourceforge.net/projects/bastard/files/libdisasm/)
#LIBS = -ldisasm -lm
# For cluster use, static libraries are in this directory
LIBS = ./libdisasm.a -lm

OBJS =	\
	compute_error.o \
	decompile.o	\
	disasm.o	\
	load.o \
	plot.o \
	util.o

all: lgp decompiled simple

simple: simple.o
	$(CC) $(CFLAGS) -o simple simple.o $(LIBS)

lgp: lgp.o $(OBJS)
	$(CC) $(CFLAGS) -o lgp lgp.o $(OBJS) $(LIBS)

# standalone program to call decompiled code (in f.c) 
decompiled: decompiled.o load.o util.o f.o
	$(CC) $(CFLAGS) -o decompiled decompiled.o load.o util.o f.o $(LIBS)

# this module, which calls the machine code
# MUST BE COMPILED WITHOUT OPTIMIZATION
compute_error.o: compute_error.c lgp.h disasm.h util.h
	$(CC) -m32 -g -c compute_error.c

lgp.o:	lgp.h disasm.h opcodes.h load.h util.h
load.o:	lgp.h

clean:
	rm -f *.o core* lgp typescript plotcmds gmon.out decompiled simple
