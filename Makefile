
VERSION = 3.2a

#CC ?= gcc
CPPFLAGS += -DVERSION_STR=\"$(VERSION)\"
CFLAGS += -Wall -g

LD = $(CC)
LDFLAGS ?= -g
LDLIBS ?=

all: picocomrs
OBJS =

## This is the maximum size (in bytes) the output (e.g. copy-paste)
## queue is allowed to grow to. Zero means unlimitted.
TTY_Q_SZ = 0
CPPFLAGS += -DTTY_Q_SZ=$(TTY_Q_SZ)

## Comment this out to disable high-baudrate support
CPPFLAGS += -DHIGH_BAUD

## Normally you should NOT enable both: UUCP-style and flock(2)
## locking.

## Comment this out to disable locking with flock
CPPFLAGS += -DUSE_FLOCK

## Comment these out to disable UUCP-style lockdirs
#UUCP_LOCK_DIR=/var/lock
#CPPFLAGS += -DUUCP_LOCK_DIR=\"$(UUCP_LOCK_DIR)\"

## Comment these out to disable "linenoise"-library support
HISTFILE = .picocom_history
CPPFLAGS += -DHISTFILE=\"$(HISTFILE)\" \
	    -DLINENOISE
OBJS += linenoise-1.0/linenoise.o
linenoise-1.0/linenoise.o : linenoise-1.0/linenoise.c linenoise-1.0/linenoise.h

## Comment this in to enable (force) custom baudrate support
## even on systems not enabled by default.
#CPPFLAGS += -DUSE_CUSTOM_BAUD

## Comment this in to disable custom baudrate support
## on ALL systems (even on these enabled by default).
#CPPFLAGS += -DNO_CUSTOM_BAUD

## Comment this IN to remove help strings (saves ~ 4-6 Kb).
#CPPFLAGS += -DNO_HELP

## RS485 transmit-enable (TE / DE) control via a GPIO line
## (--te-gpio, --te-polarity). Enabled automatically when building
## on a Raspberry Pi. Override with "make TE_GPIO=1" (e.g. when
## cross-compiling for a Pi; needs Linux >= 4.8 GPIO headers) or
## "make TE_GPIO=0".
TE_GPIO = 1

ifeq ($(TE_GPIO),1)
CPPFLAGS += -DUSE_TE_GPIO
OBJS += te_gpio.o
endif
te_gpio.o : te_gpio.c te_gpio.h


OBJS += picocom.o term.o fdio.o split.o custbaud.o termios2.o custbaud_bsd.o
picocomrs : $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

picocom.o : picocom.c term.h fdio.h split.h custbaud.h te_gpio.h
term.o : term.c term.h termios2.h custbaud_bsd.h custbaud.h
split.o : split.c split.h
fdio.o : fdio.c fdio.h
termios2.o : termios2.c termios2.h termbits2.h custbaud.h
custbaud_bsd.o : custbaud_bsd.c custbaud_bsd.h custbaud.h
custbaud.o : custbaud.c custbaud.h

## Create the object's directory first, for out-of-tree builds:
##   cd <builddir> && make -f <srcdir>/Makefile VPATH=<srcdir>
.c.o :
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<


doc : picocomrs.1.html picocomrs.1 picocomrs.1.pdf

picocomrs.1 : picocomrs.1.md
	sed 's/\*\*\[/\*\*/g;s/\]\*\*/\*\*/g' $? \
	| pandoc -s -t man \
	    -Vfooter="Picocom $(VERSION)" -Vdate="`date -I`" \
	    -Vadjusting='l' \
	    -Vhyphenate='' \
	    -o $@

picocomrs.1.html : picocomrs.1.md
	pandoc -s -t html \
	    --template ~/.pandoc/tmpl/manpage.html \
	    -c ~/.pandoc/css/normalize-noforms.css \
	    -c ~/.pandoc/css/manpage.css \
	    --self-contained \
	    -Vversion="v$(VERSION)" -Vdate="`date -I`" \
	    -o $@ $?

picocomrs.1.pdf : picocom.1
	groff -man -Tpdf $? > $@


clean:
	rm -f picocom.o term.o fdio.o split.o
	rm -f linenoise-1.0/linenoise.o
	rm -f custbaud.o termios2.o custbaud_bsd.o te_gpio.o
	rm -f *~
	rm -f \#*\#

distclean: clean
	rm -f picocom

realclean: distclean
	rm -f picocomrs.1
	rm -f picocomrs.1.html
	rm -f picocomrs.1.pdf
	rm -f CHANGES
