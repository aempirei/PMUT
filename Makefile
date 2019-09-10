CC = gcc
CXX = g++-8
CFLAGS = -O2 -pedantic
CXXFLAGS = -O2 -pedantic --std=gnu++17
CPPFLAGS = -Wall -I. -DVERSION=\"$(file < VERSION)\" -DPROGRAM=\"$(basename $(notdir $@))\"
CTARGETS = pgetch purlencode pidler ptoilet pioperf
CXXTARGETS = pcopy psync pabridge
SCRIPTS = pf ptemplate pextract pcols pjoin pid3tag prename pcd pmore
MODULES = PMUT
INSTALL_PATH = /usr/local/bin

.PHONY: all clean install wipe

all: $(CXXTARGETS) $(CTARGETS) $(MODULES) $(SCRIPTS)

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
	$(CXX) $(CFLAGS) -o $@ $< -lstdc++fs -lm

clean:
	if [ -f PMUT/Makefile ]; then ( cd PMUT ; make clean ); fi
	rm -f *.o *~
	rm -f PMUT/Makefile.old
	rm -f PMUT/lib/PMUT.pm
	rm -f PMUT.pm

install: all
	install -m755 $(CXXTARGETS) $(CTARGETS) $(SCRIPTS) $(INSTALL_PATH)
	( cd PMUT ; make install )

wipe: clean
	rm -f $(CTARGETS) $(CXXTARGETS)
