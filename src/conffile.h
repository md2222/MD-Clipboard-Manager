#ifndef __CONFFILE_H__
#define __CONFFILE_H__

#include <gtk/gtk.h>

// http://www.gtkbook.com/gtkbook/keyfile.html
// https://developer.gnome.org/glib/stable/glib-File-Utilities.html

#define LT_INT 1


GKeyFile* confOpen(const char* fileName);
gboolean confSaveToFile(GKeyFile* file, const char* fileName);

gchar* confGetString(GKeyFile* file, const char* group, const char* name, const char* def);
void confSetString(GKeyFile* file, const char* group, const char* name, const char* val);

int confGetInt(GKeyFile* file, const char* group, const char* name, gint def);
void confSetInt(GKeyFile* file, const char* group, const char* name, gint val);

void confSetIntList(GKeyFile* file, const char* group, const char* name, int type, void* list, gsize size);
void* confGetIntList(GKeyFile* file, const char* group, const char* name, int type, gsize size);

void confSetRect(GKeyFile* file, const char* group, const char* name, GdkRectangle* rect);
GdkRectangle confGetRect(GKeyFile* file, const char* group, const char* name);
gboolean confRectIsEmpty(GdkRectangle* rect);  // empty when area <= 0


#endif
