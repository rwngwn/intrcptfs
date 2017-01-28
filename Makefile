BIN=/usr/local/bin
MAN=/usr/share/man/man1
TARG=libintrcptfs.so 
OBJS=lib/9pfs/9p.o\
	lib/9pfs/util.o\
	lib/9pfs/lib/strecpy.o\
	lib/9pfs/lib/convD2M.o\
	lib/9pfs/lib/convM2D.o\
	lib/9pfs/lib/convM2S.o\
	lib/9pfs/lib/convS2M.o\
	lib/9pfs/lib/read9pmsg.o\
	lib/9pfs/lib/readn.o\
	lib/9pfs/lib/auth_proxy.o\
	lib/9pfs/lib/auth_rpc.o\
	lib/9pfs/lib/auth_getkey.o\
	intrcptfs.o
CC=	gcc
DEBUG=	-g
CFLAGS=	-O2 -pipe\
		${DEBUG} -Wall -fPIC\
		-D_GNU_SOURCE\
		-I lib/9pfs/
LDFLAGS= -shared 
LDADD= -ldl

all:	${TARG}


${TARG}:	${OBJS}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LDADD}

.c.o:
	${CC} -c -o $@ ${CFLAGS} $<

clean:
	rm -f ${TARG} ${OBJS}
