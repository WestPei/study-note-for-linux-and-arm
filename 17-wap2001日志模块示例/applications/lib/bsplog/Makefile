#source file
SOURCE  := $(wildcard *.c)
OBJS    := $(patsubst %.c,%.o,$(SOURCE))

CURDIR   = $(shell pwd)
OUTDIR   = $(subst $(TOPDIR), $(OBJSDIR), $(CURDIR))

all: $(OBJS)


$(OBJS):%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $(OUTDIR)/$@