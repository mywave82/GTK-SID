CC=gcc
CXX=g++
CFLAGS=-g `pkg-config gtk+-3.0 cairo libpulse-mainloop-glib --cflags`
CXXFLAGS=-g
LD=g++
LIBS=-g `pkg-config gtk+-3.0 cairo libpulse-mainloop-glib --libs` -lm

all: mainwindow

clean:
	rm -f *.o mainwindow

mainwindow.o: \
	mainwindow.c \
	pulse.h \
	sid-instance.h
	$(CC) $(CFLAGS) $< -o $@ -c

pulse.o: \
	pulse.c \
	pulse.h
	$(CC) $(CFLAGS) $< -o $@ -c

sid-instance.o: \
	sid-instance.cc \
	sid-instance.h
	$(CXX) $(CXXFLAGS) $< -o $@ -c

mainwindow: \
	mainwindow.o \
	pulse.o \
	sid-instance.o \
	dac.o \
	envelope.o \
	extfilt.o \
	filter8580new.o \
	pot.o \
	sid.o \
	version.o \
	voice.o \
	wave.o
	$(LD) $^ $(LIBS) -o $@

dac.o: \
	resid/dac.cc \
	resid/dac.h
	$(CXX) $(CXXFLAGS) $< -o $@ -c

envelope.o: \
	resid/envelope.cc \
	resid/envelope.h
	$(CXX) $(CXXFLAGS) $< -o $@ -c

extfilt.o: \
	resid/extfilt.cc \
	resid/extfilt.h
	$(CXX) $(CXXFLAGS) $< -o $@ -c

filter8580new.o: \
	resid/filter8580new.cc \
	resid/filter8580new.h
	$(CXX) $(CXXFLAGS) $< -o $@ -c

filter.o: \
	resid/filter.cc \
	resid/filter.h
	$(CXX) $(CXXFLAGS) $< -o $@ -c

pot.o: \
	resid/pot.cc \
	resid/pot.h
	$(CXX) $(CXXFLAGS) $< -o $@ -c

sid.o: \
	resid/sid.cc \
	resid/sid.h
	$(CXX) $(CXXFLAGS) $< -o $@ -c

version.o: \
	resid/version.cc
	$(CXX) $(CXXFLAGS) $< -o $@ -c

voice.o: \
	resid/voice.cc \
	resid/voice.h
	$(CXX) $(CXXFLAGS) $< -o $@ -c

wave.o: \
	resid/wave.cc \
	resid/wave.h
	$(CXX) $(CXXFLAGS) $< -o $@ -c
