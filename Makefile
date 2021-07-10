CC=gcc
LIBSOCKET=-lnsl
CCFLAGS=-Wall -g
SRV=server
CLT=client

all: $(SRV) $(CLT)

$(SRV):$(SRV).c
	$(CC) -o $(SRV) $(LIBSOCKET) $(SRV).c

$(CLT):	$(CLT).c
	$(CC) -o $(CLT) $(LIBSOCKET) $(CLT).c

clean:
	rm -f *.o *~
	rm -f $(SRV) $(CLT)