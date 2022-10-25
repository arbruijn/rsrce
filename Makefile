OBJS= resource.o translate.o command.o main.o
CC= gcc
CFLAGS= -Wall -g -O2
RM= rm -f

DESTDIR=
LOCAL=/local
USR_BIN=$(DESTDIR)/usr$(LOCAL)/bin

rsrce: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

install: rsrce
	install -m755 rsrce $(USR_BIN)/rsrce

clean:
	$(RM) $(OBJS) rsrce

resource.o: rsrc-fmt.h

