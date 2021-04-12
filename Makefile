PORT=59206
CFLAGS= -DPORT=\$(PORT) -g -Wall
DEPENDENCIES = xmodemserver.h crc16.h

all: xmodemserver client1

xmodemserver: crc16.o xmodemserver.o helper.o
	gcc ${FLAGS} -o $@ $^

client1: crc16.o client1.o
	gcc ${FLAGS} -o $@ $^

clean:
	rm -f *.o xmodemserver client1