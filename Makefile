CC = gcc

USR = $(shell logname)
UNP_DIR = /users/cse533/Stevens/unpv13e

LIBS = ${UNP_DIR}/libunp.a

FLAGS = -g -O2

CFLAGS = ${FLAGS} -I${UNP_DIR}/lib

all: tour_${USR}

utils.o: utils.c
	${CC} ${CFLAGS} -c utils.c

ip.o: ip.c
	${CC} ${CFLAGS} -c ip.c

multicast.o: multicast.c
	${CC} ${CFLAGS} -c multicast.c

tour_${USR}: tour.o utils.o ip.o multicast.o
	${CC} ${CFLAGS} -o tour_${USR} tour.o utils.o ip.o multicast.o ${LIBS}

tour.o: tour.c
	${CC} ${CFLAGS} -c tour.c

clean:
	rm -f tour_${USR} *.o

install:
	~/cse533/deploy_app tour_${USR}

