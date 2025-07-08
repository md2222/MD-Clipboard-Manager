#ifndef __TEXTLISTWINDOW_H__
#define __TEXTLISTWINDOW_H__

#include <gtk/gtk.h> 


typedef struct
{
    GtkWindow *window;
    GdkRectangle rect;
    GtkListStore *store;
    GtkWidget *view;
    //int headerHeight;
    GtkTreePath* path;
    gboolean isChanged;
    GdkRectangle editorRect;
    gpointer data;  // user data
    void (*onClose)(gpointer data);
} TextListWindow; 


TextListWindow* textListWindowShow(GList* list, GdkRectangle* rect);

GList* textListWindowGetList(TextListWindow* tlw);

gchar* textToLine(const gchar* text, int len);

gboolean rectIsNull(GdkRectangle* rect);
void rectSetNull(GdkRectangle* rect);


#endif
