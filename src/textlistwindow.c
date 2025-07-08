#include "textlistwindow.h"


static TextListWindow* textListWin;
static GdkRectangle defRect = { 670, 320, 600, 400 };
//static GdkRectangle defEditorRect = { 0, 0, 600, 400 };


enum {
  VIEW_COL = 0,
  TEXT_COL,
  N_COL
};

static const GtkTargetEntry drag_targets = { 
    "STRING", GTK_TARGET_SAME_WIDGET, 0
};
static guint n_targets = 1;

gchar* textDialogShow(TextListWindow* tlw, const gchar* text);


gboolean rectIsNull(GdkRectangle* rect)
{
    return (rect->width == 0 || rect->height == 0);
}

void rectSetNull(GdkRectangle* rect)
{
    rect->width = 0;
    rect->height = 0;
}


gchar* textToLine(const gchar* text, int len)
{
    gchar* esc = "\n\r\t";
    gchar* res = g_strdup(text);
    gchar* p1 = res;
    
    for (; *p1; p1++)
    {
        gchar* p2 = esc;
        
        for (; *p2; p2++)
            if (*p1 == *p2)
                *p1 = ' ';
                
        if (len > 0 && p1 - res > len) 
        {
            *p1 = 0; 
            break;      
        }
    }
    
    return res;
}


static void onDragDataReceived(GtkWidget *wgt, GdkDragContext *context, int x, int y,
                        GtkSelectionData *seldata, guint info, guint time, gpointer userdata)
{
    TextListWindow* tlw = (TextListWindow*)userdata;

    GtkTreeIter iter;
    GtkTreeModel *model;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(wgt));

    if (!gtk_tree_selection_get_selected(selection, &model, &iter))  return;
    
    GtkTreePath* path = gtk_tree_model_get_path (model, &iter);
        
    GtkTreePath* destPath;
    GtkTreeIter destIter;
    
    gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(wgt), x, y, &destPath, 0);
    
    if (!destPath)  return;
    
    if (!gtk_tree_path_compare(destPath, path))  return;
    
    if (destPath && gtk_tree_model_get_iter(model, &destIter, destPath))
    {
        gtk_list_store_move_before (model, &iter, &destIter);  // only works with unsorted stores
        tlw->isChanged = TRUE;
    }
}
   

static void onAddNewItem (GtkWidget *menuitem, gpointer userdata)
{
    TextListWindow* tlw = (TextListWindow*)userdata;

    gchar *newText = textDialogShow(tlw, NULL);
    
    if (newText)
    {
        gchar* escText = textToLine(newText, 80);
        
        GtkTreeIter iter;
        gtk_list_store_append(tlw->store, &iter);
        gtk_list_store_set(tlw->store, &iter, VIEW_COL, escText, TEXT_COL, newText, -1);
        tlw->isChanged = TRUE;
    }
}


static void onEditItem (GtkWidget *menuitem, gpointer userdata)
{
    TextListWindow* tlw = (TextListWindow*)userdata;
    GtkTreePath *path = tlw->path;
    GtkTreeIter iter;
    
    if (gtk_tree_model_get_iter(tlw->store, &iter, path))
    {
        gchar *text;

        gtk_tree_model_get(tlw->store, &iter, TEXT_COL, &text, -1);

        gchar *newText = textDialogShow(tlw, text);
        
        g_free(text);
        
        if (newText)
        {
            gchar* escText = textToLine(newText, 80);
            
            gtk_list_store_set(tlw->store, &iter, VIEW_COL, escText, TEXT_COL, newText, -1);
            tlw->isChanged = TRUE;
        }
    }
}


static void onDeleteItem (GtkWidget *menuitem, gpointer userdata)
{
    TextListWindow* tlw = (TextListWindow*)userdata;
    GtkTreePath *path = tlw->path;
    GtkTreeIter iter;
    
    if (gtk_tree_model_get_iter(tlw->store, &iter, path))
    {
        gtk_list_store_remove(tlw->store, &iter);
        tlw->isChanged = TRUE;
    }
}


// https://zetcode.com/gui/gtk2/menusandtoolbars/
static void showMenu (GtkWidget *treeview, GdkEventButton *event, gpointer userdata)
{
    TextListWindow* tlw = (TextListWindow*)userdata;
    gboolean sensitive = tlw->path != NULL;

    GtkWidget *menu, *menuitem, *submenuitem;

    menu = gtk_menu_new();
    
    menuitem = gtk_menu_item_new_with_label("Add new item");
    g_signal_connect(menuitem, "activate", (GCallback)onAddNewItem, userdata);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    menuitem = gtk_menu_item_new_with_label("Edit selected item");
    g_signal_connect(menuitem, "activate", (GCallback)onEditItem, userdata);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    gtk_widget_set_sensitive(menuitem, sensitive);

    GtkWidget *delMenu = gtk_menu_new();
    menuitem = gtk_menu_item_new_with_label("Delete selected item");
    gtk_menu_shell_append(GTK_MENU_SHELL(delMenu), menuitem);
    g_signal_connect(menuitem, "activate", (GCallback)onDeleteItem, userdata);

    menuitem = gtk_menu_item_new_with_label("Delete");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), delMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    gtk_widget_set_sensitive(menuitem, sensitive);

    gtk_widget_show_all(menu);

    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                 (event != NULL) ? event->button : 0,
                 gdk_event_get_time((GdkEvent*)event));
}


static gboolean onButtonPressed (GtkWidget *treeview, GdkEventButton *event, gpointer userdata)
{
    TextListWindow* tlw = (TextListWindow*)userdata;

    // doc: Normally button 1 is the left mouse button, 2 is the middle button, and 3 is the right button. 
    if (event->type == GDK_BUTTON_PRESS  &&  event->button == 3)
    {
        GtkTreePath *path = 0;

        GtkTreeSelection *selection;

        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

        if (gtk_tree_selection_count_selected_rows(selection)  <= 1)
        {
            if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview), (gint) event->x, (gint) event->y, &path, NULL, NULL, NULL))
            {
                gtk_tree_selection_unselect_all(selection);
                gtk_tree_selection_select_path(selection, path);
                //gtk_tree_path_free(path);
            }
        }

        tlw->path = path;

        showMenu(treeview, event, tlw);

        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}


static void onRowActivated(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn  *col, gpointer userdata)
{
    TextListWindow* tlw = (TextListWindow*)userdata;
    GtkTreeModel *model;
    GtkTreeIter   iter;

    model = gtk_tree_view_get_model(treeview);

    if (gtk_tree_model_get_iter(model, &iter, path))
    {
        tlw->path = path;
        
        onEditItem (NULL, tlw);
    }
}


static void addListItem(gchar* item, TextListWindow* tlw)
{
    gchar* escText = textToLine(item, 80);
    
    GtkTreeIter iter;
    gtk_list_store_append(tlw->store, &iter);
    gtk_list_store_set(tlw->store, &iter, VIEW_COL, escText, TEXT_COL, item, -1);
    tlw->isChanged = TRUE;
}


static gboolean onListTooltip (GtkWidget* widget, gint x, gint y, gboolean keyboard_mode, GtkTooltip* tooltip, gpointer data)
{
    TextListWindow* tlw = (TextListWindow*)data;
    GtkTreeModel* model = 0;
    GtkTreePath* path = 0;
    GtkTreeIter iter;
    
    gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), x, y, &path, 0, 0, 0);  // if no header

    if (!path || !gtk_tree_model_get_iter(tlw->store, &iter, path))  return FALSE;
    
    gchar *text = 0;
    gtk_tree_model_get(tlw->store, &iter, TEXT_COL, &text, -1);
    
    if (!text || text[0] == 0)  return FALSE;
    
    gtk_tooltip_set_text(tooltip, text);
    g_free(text);

    return TRUE;
}


static gboolean onClose(GtkWidget *win, GdkEvent* event, TextListWindow* tlw)
{
    GdkRectangle rect;
    gtk_window_get_position(win, &rect.x, &rect.y);
    gtk_window_get_size(win, &rect.width, &rect.height);
    tlw->rect = rect;
    
    if (tlw->onClose)
        tlw->onClose(tlw);
    else
        g_free(tlw);

    return FALSE;
}


TextListWindow* textListWindowShow(GList* list, GdkRectangle* rect)
{
    textListWin = g_malloc(sizeof(TextListWindow));
    textListWin->rect = defRect;
    textListWin->path = 0;
    textListWin->isChanged = FALSE;
    textListWin->editorRect = textListWin->rect;
    textListWin->data = 0;
    textListWin->onClose = 0;
    
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (win), "Pinned");
    if (rect && !rectIsNull(rect))  textListWin->rect = *rect;
    gtk_window_set_default_size(GTK_WINDOW(win), textListWin->rect.width, textListWin->rect.height);
    gtk_window_move(GTK_WINDOW(win), textListWin->rect.x, textListWin->rect.y);
    gtk_container_set_border_width (GTK_CONTAINER (win), 8);
    
    g_signal_connect(win, "delete-event", G_CALLBACK(onClose), textListWin);

    textListWin->window = win;

    GtkWidget *sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_ETCHED_IN);
    
    gtk_container_add (GTK_CONTAINER (win), sw);
   
    // https://docs.gtk.org/gtk3/class.ListStore.html
    // https://en.wikibooks.org/wiki/GTK%2B_By_Example/Tree_View/Tree_Models
    // https://zetcode.com/gui/gtk2/gtktreeview/
    GtkListStore *store = gtk_list_store_new (N_COL, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget *listView = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
    gtk_tree_view_set_grid_lines(listView, GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
    //gtk_tree_view_set_reorderable(GTK_TREE_VIEW(listView), TRUE);
    gtk_tree_view_set_headers_visible(listView, FALSE);
    gtk_widget_set_has_tooltip(listView, TRUE);
    g_object_unref (G_OBJECT (store));
    
    gtk_tree_view_enable_model_drag_source (listView, GDK_BUTTON1_MASK, &drag_targets, n_targets, GDK_ACTION_DEFAULT|GDK_ACTION_MOVE);
    gtk_tree_view_enable_model_drag_dest (listView, &drag_targets, n_targets, GDK_ACTION_DEFAULT|GDK_ACTION_MOVE);
    
    textListWin->store = store;
    textListWin->view = listView;
    
    g_signal_connect(listView, "button-press-event", G_CALLBACK(onButtonPressed), textListWin);
    g_signal_connect(listView, "row-activated", (GCallback)onRowActivated, textListWin);
    g_signal_connect(listView, "drag_data_received", G_CALLBACK(onDragDataReceived), textListWin);
    g_signal_connect(listView, "query-tooltip", G_CALLBACK(onListTooltip), textListWin);
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
    //g_object_set (renderer, "editable", TRUE, NULL);
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes ("Hint", renderer, "text", VIEW_COL, NULL);
    //gtk_tree_view_column_set_sort_column_id (column, VIEW_COL);  // no sort
    gtk_tree_view_append_column (listView, column);
    
    gtk_container_add (GTK_CONTAINER (sw), listView);
    
    gtk_widget_show_all (win);
  
    g_list_foreach (list, addListItem, textListWin);
    textListWin->isChanged = FALSE;  // it's load
    
    return textListWin;
}


static gboolean getListDataFunc(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter, GList** list)
{
    if (gtk_tree_model_get_iter(model, iter, path))
    {
        gchar *text;

        gtk_tree_model_get(model, iter, TEXT_COL, &text, -1);
        
        *list = g_list_append(*list, text);
    }
    
    return FALSE;
}


GList* textListWindowGetList(TextListWindow* tlw)
{
    if (!tlw)  return NULL;
    
    GList* list = 0;
    
    gtk_tree_model_foreach(tlw->store, getListDataFunc, &list);
    
    return list;
}

//----------------------------------------------------------------------------------------------------------------------

gchar* textDialogShow(TextListWindow* tlw, const gchar* text)
{
    GtkWidget *content_area;
    GtkWidget *dialog;
    GtkWidget *vbox;
    GtkWidget *image;
    GtkWidget *table;
    GtkWidget *local_entry1;
    GtkWidget *local_entry2;
    GtkWidget *label;
    gint response;

    dialog = gtk_dialog_new_with_buttons ("Item text",
                                        tlw->window, 
                                        GTK_DIALOG_MODAL| GTK_DIALOG_DESTROY_WITH_PARENT,
                                        "_OK",
                                        GTK_RESPONSE_OK,
                                        "_Cancel",
                                        GTK_RESPONSE_CANCEL,
                                        NULL);
                                        
    GdkRectangle rect;
    if (tlw->editorRect.width > 0 && tlw->editorRect.height > 0)  rect = tlw->editorRect;
    else  rect = tlw->rect;
    gtk_window_set_default_size(GTK_WINDOW(dialog), rect.width, rect.height);

    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_container_set_border_width (GTK_CONTAINER (content_area), 5);
    
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
    
    gtk_box_pack_start (GTK_BOX (content_area), vbox, 1, 1, 0);
    
    GtkWidget *scrollWin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollWin),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    GtkWidget *textView = gtk_text_view_new ();
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW (textView), 5);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW (textView), 5);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW (textView), 5);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW (textView), 5);
 
    
    gtk_container_add (GTK_CONTAINER (scrollWin), GTK_WIDGET (textView));

    gtk_box_pack_start (vbox, scrollWin, 1, 1, 0);

    
    GtkTextBuffer *textBuf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textView));
    //gtk_text_view_set_wrap_mode (GtkTextView* text_view, GtkWrapMode wrap_mode)  // GTK_WRAP_NONE  GTK_WRAP_WORD

    if (text)
        gtk_text_buffer_set_text (textBuf, text, -1);

    gtk_widget_show_all (dialog);
  
    gchar *newText = 0;
    
    response = gtk_dialog_run (GTK_DIALOG (dialog));

    if (response == GTK_RESPONSE_OK)
    {
        GtkTextIter start;
        GtkTextIter end;

        gtk_text_buffer_get_start_iter (textBuf, &start);
        gtk_text_buffer_get_end_iter (textBuf, &end);

        newText = gtk_text_buffer_get_text (textBuf, &start, &end, FALSE);
    }

    gtk_window_get_size(dialog, &tlw->editorRect.width, &tlw->editorRect.height);

    gtk_widget_destroy (dialog);
    
    return newText;
}
