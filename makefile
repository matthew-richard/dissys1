CC=gcc

CFLAGS = -g -c -Wall -pedantic
#CFLAGS = -ansi -c -Wall -pedantic

all: rcp ncp

rcv: rcv.o
	    $(CC) -o rcv rcv.o  

ncp: ncp.o
	    $(CC) -o ncp ncp.o

clean:
	rm *.o
	rm ncp
	rm rcv

%.o:    %.c
	$(CC) $(CFLAGS) $*.c


