CC = gcc

USR = $(shell logname)
UNP_DIR = /users/cse533/Stevens/unpv13e

LIBS = ${UNP_DIR}/libunp.a

FLAGS = -g -O2

CFLAGS = ${FLAGS} -I${UNP_DIR}/lib

all: tour_${USR} arp_${USR} areq_${USR}

utils.o: utils.c
	${CC} ${CFLAGS} -c utils.c

frame.o: frame.c
	${CC} ${CFLAGS} -c frame.c

ip.o: ip.c
	${CC} ${CFLAGS} -c ip.c

multicast.o: multicast.c
	${CC} ${CFLAGS} -c multicast.c

tour_${USR}: tour.o utils.o ip.o multicast.o
	${CC} ${CFLAGS} -o tour_${USR} tour.o utils.o ip.o multicast.o ${LIBS}

tour.o: tour.c
	${CC} ${CFLAGS} -c tour.c

arp_${USR}: arp.o utils.o get_hw_addrs.o frame.o
	${CC} ${CFLAGS} -o arp_${USR} arp.o utils.o get_hw_addrs.o frame.o ${LIBS}

arp.o: arp.c
	${CC} ${CFLAGS} -c arp.c

areq_${USR}: areq.o
	${CC} ${CFLAGS} -o areq_${USR} areq.o ${LIBS}

areq.o: areq.c
	${CC} ${CFLAGS} -c areq.c

clean:
	rm -f tour_${USR} arp_${USR} areq_${USR} *.o

install:
	~/cse533/deploy_app tour_${USR} arp_${USR} areq_${USR}

