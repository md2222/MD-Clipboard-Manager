// lite xml functions

#ifndef __XMLFUNC_H__
#define __XMLFUNC_H__

#include <gtk/gtk.h>


gchar* strReplace(const char* str, const char* from, const char* to);

gchar* getTagContent(gchar* data, gchar* tag, int *pos);

GList* xmlGetList(gchar* xml, gchar* listTag);

gchar* xmlSetList(gchar* xml, GList* list, gchar* listName);

#endif
