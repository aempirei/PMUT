CC = gcc
CXX = g++
CFLAGS = -O2 -pedantic 
CXXFLAGS = -O2 -pedantic --std=gnu++2a -lstdc++fs
CPPFLAGS = -Wall -I. -DVERSION=\"1.32" -DPROGRAM=\"$(basename $(notdir $@))\" -Wno-deprecated-declarations
CTARGETS = purlencode pidler ptoilet pioperf pgetch
CXXTARGETS = pcopy psync pabridge
TARGETS = pkey
SCRIPTS = pf ptemplate pextract pcols pjoin pid3tag prename pcd pmore pssj pscan
MODULES = PMUT
INSTALL_PATH = /usr/local/bin

.PHONY: all clean install wipe

all: $(CXXTARGETS) $(CTARGETS) $(TARGETS) $(MODULES) $(SCRIPTS)

%.o : %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

%.o : %.cc
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $< -o $@

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

$(CTARGETS): % : %.o
	$(CC) $(CFLAGS) -o $@ $< -lm

$(CXXTARGETS): % : %.o
	$(CXX) $(CFLAGS) -o $@ $< -lm -lstdc++fs

pkey: pkey.o
	$(CXX) $(CFLAGS) -o $@ $< -lssl -lcrypto

clean:
	if [ -f PMUT/Makefile ]; then ( cd PMUT ; make clean ); fi
	rm -f *.o *~
	rm -f PMUT/Makefile.old
	rm -f PMUT/lib/PMUT.pm
	rm -f PMUT.pm

install: all
	install -m755 $(CXXTARGETS) $(CTARGETS) $(TARGETS) $(SCRIPTS) $(INSTALL_PATH)
	( cd PMUT ; make install )

wipe: clean
	rm -f $(CTARGETS) $(CXXTARGETS)
