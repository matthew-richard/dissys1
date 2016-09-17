CC=gcc

CFLAGS = -g -c -Wall -pedantic
#CFLAGS = -ansi -c -Wall -pedantic

all: rcp ncp

rcp: rcp.o
	    $(CC) -o rcp rcp.o  

ncp: ncp.o
	    $(CC) -o ncp ncp.o

clean:
	rm *.o
	rm ncp
	rm rcp

%.o:    %.c
	$(CC) $(CFLAGS) $*.c


