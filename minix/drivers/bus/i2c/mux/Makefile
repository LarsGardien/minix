# Makefile for I2C support

PROG=   mux_i2c
SRCS=   mux_i2c.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+= ${LIBI2CDRIVER} ${LIBCHARDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+= -li2cdriver -lchardriver -lsys -ltimers

.include <minix.service.mk>
