#!/bin/make

SOURCES = resources.c textfilefunc.c textlistwindow.c xmlfunc.c conffile.c main.c

CFLAGS  = `pkg-config --cflags gtk+-3.0`

#CPPFLAGS = -Wno-deprecated-declarations -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-sign-compare -fmax-errors=3

LIBS    = `pkg-config --libs gtk+-3.0` -lX11 -lXtst -lXext 

TARGET  = mdclip


all:
	glib-compile-resources --generate-source resources.xml
	gcc $(CPPFLAGS) $(CFLAGS) $(INC) $(SOURCES) -o $(TARGET) $(LIBS)

clean:
			rm -rf mdclip *.o
