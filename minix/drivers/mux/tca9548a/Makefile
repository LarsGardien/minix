# Makefile for the tca9548a I2C multiplexer.

PROG=   tca9548a
SRCS=   tca9548a.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBI2CDRIVER} ${LIBCHARDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-li2cdriver -lchardriver -lsys -ltimers

CPPFLAGS+=	-I${NETBSDSRCDIR}

.include <minix.service.mk>
