-include Config.mk

################ Source files ##########################################

EXE	:= ${NAME}
SRCS	:= $(wildcard *.c)
OBJS	:= $(addprefix $O,$(SRCS:.c=.o))
DEPS	:= ${OBJS:.o=.d}

################ Compilation ###########################################

.PHONY: all clean distclean maintainer-clean

all:	Config.mk config.h ${EXE} ${DATAF}

run:	${EXE} ${DATAF}
	@./${EXE}

${EXE}:	${OBJS}
	@echo "Linking $@ ..."
	@${LD} ${LDFLAGS} -o $@ $^ ${LIBS}

$O%.o:	%.c
	@echo "    Compiling $< ..."
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} -MMD -MT "$(<:.c=.s) $@" -o $@ -c $<

%.s:	%.c
	@echo "    Compiling $< to assembly ..."
	@${CC} ${CFLAGS} -S -o $@ -c $<

################ Installation ##########################################

.PHONY:	install uninstall

ifdef BINDIR
EXEI	:= $(addprefix ${BINDIR}/,${EXE})
PAMCNFI	:= ${PAMDIR}/${EXE}
SYSDCFI	:= ${SYSDDIR}/${EXE}@.service
MANI	:= ${MANDIR}/man1/${EXE}.1.gz

install:	${EXEI} ${PAMCNFI} ${SYSDCFI} ${MANI}

${EXEI}:	${EXE}
	@echo "Installing $< as $@ ..."
	@${INSTALLEXE} $< $@

${PAMCNFI}:	conf/${EXE}
	@echo "Installing PAM configuration file ..."
	@${INSTALLDATA} $< $@

${SYSDCFI}:	conf/${EXE}@.service
	@echo "Installing systemd service file ..."
	@${INSTALLDATA} $< $@

${MANI}:	conf/${EXE}.1
	@echo "Installing man page ..."
	@gzip -9 -c $< > $@
	@chmod 644 $@

uninstall:
	@echo "Uninstalling ${EXE} ..."
	@rm -f ${EXEI} ${PAMCNFI} ${SYSDCFI} ${MANI}
endif

################ Maintenance ###########################################

clean:
	@if [ -d $O ]; then\
	    rm -f ${EXE} ${OBJS} ${DEPS};\
	    rmdir $O;\
	fi

distclean:	clean
	@rm -f Config.mk config.h config.status

maintainer-clean: distclean

${OBJS}:		Makefile Config.mk config.h
Config.mk:		Config.mk.in
config.h:		config.h.in
Config.mk config.h:	configure
	@if [ -x config.status ]; then echo "Reconfiguring ..."; ./config.status; \
	else echo "Running configure ..."; ./configure; fi

-include ${DEPS}
