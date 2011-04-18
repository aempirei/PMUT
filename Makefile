CC = gcc
CCC = g++
CCFLAGS = -Wall -O2 -I.
CFLAGS = -Wall -O2 -I.
CPPFLAGS = -Wall -O2 -I.
TARGETS = pgetch purlencode pidler
SCRIPTS = pf ptemplate pextract pcols pjoin pid3tag prename pcd
MODULES = PMUT
INSTALL_PATH = /usr/local/bin

.PHONY: all clean install wipe

all: $(TARGETS) $(MODULES) $(SCRIPTS)

*.c: version.h

version.h: VERSION
	printf '#define PMUTVERSION "%s"\n' `cat VERSION` > version.h

PMUT.pm: PMUT
	cp PMUT/blib/lib/PMUT.pm .
	chmod 644 PMUT.pm

PMUT: PMUT/Makefile
	( cd PMUT ; make )

PMUT/lib:
	mkdir -p PMUT/lib

PMUT/Makefile:	PMUT/lib/PMUT.pm
	( cd PMUT ; perl Makefile.PL )

PMUT/lib/PMUT.pm: PMUT.pm.template VERSION PMUT/lib
	perl -pe "s/___V___/`cat VERSION`/g" < PMUT.pm.template > $@

pgetch: pgetch.o
	$(CCC) $(CCFLAGS) -o $@ $<

purlencode: purlencode.o
	$(CCC) $(CCFLAGS) -o $@ $<

clean:
	if [ -f PMUT/Makefile ]; then ( cd PMUT ; make clean ); fi
	rm -f *.o *~
	rm -f version.h
	rm -f PMUT/Makefile.old
	rm -f PMUT/lib/PMUT.pm
	rm -f PMUT.pm

install: all
	install -m755 $(TARGETS) $(SCRIPTS) $(INSTALL_PATH)
	( cd PMUT ; make install )

wipe: clean
	rm -f $(TARGETS)
