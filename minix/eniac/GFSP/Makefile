PROG=	GFSP
BINDIR=	/etc/eniac/GFSP
SRCS+=	main.c fsm_parser.c
MAN=

.include <bsd.prog.mk>

beforeinstall: 
	${INSTALL} -d $(DESTDIR)$(BINDIR)
