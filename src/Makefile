#!/bin/make

SOURCES = resources.c xmlfunc.c main.c

CFLAGS  = `pkg-config --cflags gtk+-3.0`

LIBS    = `pkg-config --libs gtk+-3.0` -lX11 -lXtst -lXext 

TARGET  = mdclip


all:
	glib-compile-resources --generate-source resources.xml
	gcc $(CFLAGS) $(INC) $(SOURCES) -o $(TARGET) $(LIBS)

clean:
			rm -rf mdclip *.o
