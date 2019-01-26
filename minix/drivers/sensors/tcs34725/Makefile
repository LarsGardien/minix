# Makefile for the tcs34725 color sensor.

PROG=   tcs34725
SRCS=   tcs34725.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBI2CDRIVER} ${LIBCHARDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-li2cdriver -lchardriver -lsys -ltimers

CPPFLAGS+=	-I${NETBSDSRCDIR}

.include <minix.service.mk>
