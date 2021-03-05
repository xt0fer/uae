#
# makefile
#
# Anthony's Editor January 93
#
# Public Domain 1991, 1993 by Anthony Howe.  No warranty.
#

CC      = cc
CFLAGS  = -O -DBSD=1

LD      = cc
LDFLAGS =
LIBS    = -lcurses -ltermcap

CP      = cp
MV      = mv
RM      = rm

E       =
O       = .o

OBJ     = command$(O) data$(O) display$(O) gap$(O) key$(O) main$(O)

ae$(E) : $(OBJ)
        $(LD) $(LDFLAGS) -o ae$(E) $(OBJ) $(LIBS)

header.h : key.h

command$(O): command.c header.h
        $(CC) $(CFLAGS) -c command.c

data$(O): data.c header.h
        $(CC) $(CFLAGS) -c data.c

display$(O): display.c header.h
        $(CC) $(CFLAGS) -c display.c

gap$(O): gap.c header.h
        $(CC) $(CFLAGS) -c gap.c

key$(O): key.c header.h
        $(CC) $(CFLAGS) -c key.c

main$(O): main.c header.h
        $(CC) $(CFLAGS) -c main.c

clean:
        -$(RM) $(OBJ) ae$(E)

install:
        -$(MV) ae$(E) $(HOME)/$(HOSTNAME)/bin

