
TOOLCHAINDIR= toolchain
#CC = $(TOOLCHAINDIR)/bin/aarch64-linux-gcc -g
#AR = $(TOOLCHAINDIR)/bin/aarch64-linux-ar
OUT = ./out

ifeq (,$(CC))
	CC = gcc
endif

export CC AR TOOLCHAINDIR

PWD = $(shell pwd)
SOURCE = $(wildcard src/*.c)
OBJS = $(patsubst %.c,%.o,$(SOURCE))     # 把 SOURCE 中的.c 改成.o赋值给OBJS

TARGET = test

CFLAGS = -I./inc

LDFLAG = -L./lib
LDFLAG += -lpthread -lasound

all:$(TARGET)

$(TARGET):$(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

%.o:%.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	$(RM) $(OBJS) $(TARGET)
