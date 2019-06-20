CC = gcc
CXX = g++-8
CXXFLAGS = -Wall -pedantic -std=gnu++17 -O2 -Wno-unused-result -Wno-misleading-indentation
CFLAGS = -Wall -O2 -I. -pedantic
TARGETS = pgetch purlencode pidler ptoilet pcopy pioperf pabridge
SCRIPTS = pf ptemplate pextract pcols pjoin pid3tag prename pcd pmore
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

pidler: pidler.o
	$(CC) $(CFLAGS) -o $@ $<

pabridge: pabridge.o
	$(CXX) $(CXXFLAGS) -o $@ $< -lstdc++fs

pcopy: pcopy.o
	$(CXX) $(CFLAGS) -o $@ $<

ptoilet: ptoilet.o
	$(CC) $(CFLAGS) -o $@ $< -lm

pgetch: pgetch.o
	$(CC) $(CFLAGS) -o $@ $<

pioperf: pioperf.o
	$(CC) $(CFLAGS) -o $@ $< -lm

purlencode: purlencode.o
	$(CC) $(CFLAGS) -o $@ $<

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
