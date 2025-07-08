// release version
#include <glib.h>
#include <glib-unix.h>
#include <gtk/gtk.h>
#include <signal.h>
#include <errno.h>
//#include <xdo.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/extensions/XTest.h>
#include "textlistwindow.h"
#include "textfilefunc.h"
#include "xmlfunc.h"
#include "conffile.h"


typedef struct 
{
    int argc;
    char **argv;
} Args;


gchar* appName = "MD Clipboard Manager";
GdkPixbuf *pixApp = 0;
GdkPixbuf *pixOffline = 0;
GtkStatusIcon *tray = 0;

enum AppMode { APP_MODE_PRIMARY, APP_MODE_REMOTE };

#define MAX_LIST_LEN 40
#define MAX_LABEL_LEN 45
#define MAX_LABEL_UTF8_LEN  MAX_LABEL_LEN * 4 + 1
#define MAX_TOOLTIP_LEN 800
#define MAX_TOOLTIP_UTF8_LEN MAX_TOOLTIP_LEN * 4 + 1
static GtkClipboard *clip; 
gchar *clipText;
GList* clipList = 0;
FILE* clipFile = 0;
gchar* clipFilePath = 0;
gint maxListLen = MAX_LIST_LEN;
gint maxLabelLen = MAX_LABEL_LEN;
gint maxTooltipLen = MAX_TOOLTIP_LEN;
gboolean isOffline = FALSE;
gint clipSigHandId = 0;
int clipMenuItemCount = 0;
int pinMenuItemCount = 0;
Display* display = NULL;
GList* pinList = 0;
gchar* pinFilePath = 0;
TextListWindow* pinListWIn = 0;
GdkRectangle pinListWinRect = { 0, 0, 0, 0 };
GdkRectangle pinEditorRect = { 0, 0, 0, 0 };
gchar* confPath = 0;
GKeyFile* confFile = 0;


static void saveConfig()
{
    if (!confPath)
    {
        g_printerr("saveConfig:  Config file path is empty. Not saved.\n");
        return;
    }
    
    confFile = confOpen(confPath);

    if (!confFile)
        g_printerr("saveConfig:  Key file was not opened.\n");
    else
    {
        confSetRect(confFile, "private", "pinListWinRect", &pinListWinRect);
        confSetRect(confFile, "private", "pinEditorRect", &pinEditorRect);

        confSetInt(confFile, "public", "maxListLen", maxListLen);
        confSetInt(confFile, "public", "maxLabelLen", maxLabelLen);
        confSetInt(confFile, "public", "maxTooltipLen", maxTooltipLen);

        confSaveToFile(confFile, confPath);
    }
}


static void loadConfig()
{
    if (!confPath)
    {
        g_printerr("loadConfig:  Config file path is empty. Not loaded.\n");
        return;
    }
    
    confFile = confOpen(confPath);

    if (!confFile)
    {
        g_printerr("loadConfig:  Key file was not opened.\n");
        return;
    }
 
    GdkRectangle rect = confGetRect(confFile, "private", "pinListWinRect");
    if (!confRectIsEmpty(&rect))  pinListWinRect = rect;

    rect = confGetRect(confFile, "private", "pinEditorRect");
    if (!confRectIsEmpty(&rect))  pinEditorRect = rect;
    
    maxListLen = confGetInt(confFile, "public", "maxListLen", MAX_LIST_LEN);
    maxLabelLen = confGetInt(confFile, "public", "maxLabelLen", MAX_LABEL_LEN);
    maxTooltipLen = confGetInt(confFile, "public", "maxTooltipLen", MAX_TOOLTIP_LEN);
}


static void fakeKey(Display* disp, KeySym keysym, KeySym modsym)
{
    KeyCode keycode = 0, modcode = 0;
    
    if (!disp || keysym == 0)  return;
    
    keycode = XKeysymToKeycode (disp, keysym);
    modcode = XKeysymToKeycode(disp, modsym);
    
    if (keycode == 0)  return;

    XTestGrabControl (disp, True);
    XTestFakeKeyEvent (disp, modcode, True, 0);
    XTestFakeKeyEvent (disp, keycode, True, 0);
    XTestFakeKeyEvent (disp, keycode, False, 0); 
    XTestFakeKeyEvent (disp, modcode, False, 0);
    XSync (disp, False);
    //XTestGrabControl (disp, False);
    //XFlush(disp); 
}


static gboolean saveClipList()
{
    if (!clipList)
    {
        // clear file
        if (clipFile)  
        {
            fclose(clipFile);
            clipFile = 0;
        }
    }

    if (!clipFile)  
    {
        clipFile = fopen(clipFilePath, "w+");
        if (!clipFile) 
        {
             g_printerr("saveClipList:  fopen error:  %d\n",  errno);
             return FALSE;
        }
    }
        
    if (!clipList)  return TRUE;

    gchar* tmp = g_strdup("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");

    gchar* xml = xmlSetList(tmp, clipList, "clip-list");
    g_free(tmp);

    int xmlSize = strlen(xml) + 1;

    rewind(clipFile);

    size_t written = fwrite(xml, xmlSize, 1, clipFile);
    
    if (written != 1)
    {
         g_printerr("saveClipList:  fwrite error:  %d\n",  errno);
         return FALSE;
    }
    
    fflush (clipFile); 
    
    return TRUE;
}


static void savePinList(gchar* path)
{
    FILE *file = fopen(path, "w+");
    if (!file) 
    {
         g_printerr("savePinList:  fopen error:  %d\n",  errno);
         return;
    }
        
    gchar* tmp = g_strdup("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");

    gchar* xml = xmlSetList(tmp, pinList, "pinned-list");
    g_free(tmp);

    int xmlSize = strlen(xml) + 1;

    rewind(file);

    size_t written = fwrite(xml, xmlSize, 1, file);
    
    if (written != 1)
    {
         g_printerr("savePinList:  fwrite error:  %d\n",  errno);
         //return;
    }
    
    fclose (file);
}


gint compareItemFunc(gchar* item1, gchar* data)
{
    return strcmp(item1, data);
}


static void onClipChanged()
{
    clipText = gtk_clipboard_wait_for_text(clip); 

    if (clipText && strlen(clipText) >= 5)
    {
        while (1)
        {
            GList* list = g_list_find_custom (clipList, clipText, compareItemFunc);
            
            if (!list)  break;
            
            clipList = g_list_remove_all(clipList, list->data);
        }
        
        clipList = g_list_prepend(clipList, clipText);
        
        while (1)
        {
            gint len = g_list_length(clipList);

            if (len <= MAX_LIST_LEN)  break;
            
            GList* last = g_list_last (clipList);
            if (last)
                clipList = g_list_remove(clipList, last->data);
        }
        
        saveClipList();
    }
 }


static void 
onExit(GtkWidget *menuItem, GApplication *app)
{
    saveConfig();
    
    gtk_main_quit();  // +onAppShutdown
}


static void onOfflineToggled(GtkCheckMenuItem* self, gpointer user_data)
{
    isOffline = gtk_check_menu_item_get_active (self);
    g_print("onOfflineToggled:    %d\n", isOffline);

    if (isOffline)
    {
       g_signal_handler_disconnect(clip, clipSigHandId);
       gtk_status_icon_set_from_pixbuf(tray, pixOffline);
    }
    else
    {
       clipSigHandId = g_signal_connect(clip, "owner-change", G_CALLBACK(onClipChanged), NULL);
       gtk_status_icon_set_from_pixbuf(tray, pixApp);
    }
}


static GtkWidget*
trayMenuNew (GtkApplication *app)
{
    GtkWidget *trayMenu = gtk_menu_new();

    gchar* helpTooltip = "For register system hotkey use \nSystem Settings (Preferences) > Keyboard > Shortcuts\n"
        "For Command use \"your-bin-dir/mdclip -m\"";
    
    GtkWidget *miHelp = gtk_menu_item_new_with_label("Help");
    gtk_widget_set_tooltip_text (miHelp, helpTooltip);
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), miHelp);

    GtkWidget *miOffline = gtk_check_menu_item_new_with_label("Offline");
    gtk_widget_set_tooltip_text (miOffline, "Stop clipboard capture");
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), miOffline);
    g_signal_connect(miOffline, "toggled", onOfflineToggled, NULL); 

    GtkWidget *sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), sep);

    GtkWidget *miExit = gtk_menu_item_new_with_label("Quit mdclip");
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), miExit);
    g_signal_connect(miExit, "activate", G_CALLBACK(onExit), app);

    gtk_widget_show_all(trayMenu);

    return trayMenu;
}

// for remote only
static gboolean onExitOnHide(gpointer user_data)
{
    gtk_main_quit();

    return FALSE;
}


static gboolean onFakeKey(gpointer user_data)
{
    fakeKey(display, XK_V, XK_Control_L);

    return FALSE;
}


static void onClipMenuItem(GtkWidget *menuItem, int index)
{
    gchar* itemText = g_list_nth_data (clipList, index);
    
    gtk_clipboard_set_text(clip, itemText, -1);
    
    g_timeout_add (500, onFakeKey, NULL);   // wait menu closed
}


static void onPinMenuItem(GtkWidget *menuItem, int index)
{
    gchar* itemText = g_list_nth_data (pinList, index);
    
    gtk_clipboard_set_text(clip, itemText, -1);
    
    g_timeout_add (500, onFakeKey, NULL);   // wait menu closed
}


void replaceChar(char *str, char from, char to) 
{
    char* pos = str;
    
    while (1) 
    {
        pos = strchr(pos, from);
        if (pos == NULL)  break;
        *pos++ = to;
    }
}


static void onPinListWinClose(TextListWindow* tlw)
{
    printf("onPinListWinClose:  \n");
    
    pinListWinRect = tlw->rect;
    pinEditorRect = tlw->editorRect;
    
    if (tlw->isChanged)
    {
        GList* list = textListWindowGetList(tlw);
        
        g_list_free_full(pinList, g_free);
        pinList = list;
        
        savePinList(pinFilePath);
    }
    
    g_free(tlw);
    pinListWIn = 0;
} 


static void onPinEdit(GtkMenuItem* self, gpointer user_data)
{
    pinListWIn = textListWindowShow(pinList, &pinListWinRect);
    pinListWIn->editorRect = pinEditorRect;
    pinListWIn->onClose = onPinListWinClose; 
}


static GtkWidget* createMenuItem(gchar* item)
{
    gboolean isUtf8 = g_utf8_validate(item, -1, NULL);
    gchar* text = g_malloc(MAX_TOOLTIP_UTF8_LEN);
    
    if (isUtf8)  g_utf8_strncpy(text, item, MAX_LABEL_LEN);  // doc: The value is a NUL terminated UTF-8 string.
    else  strncpy(text, item, MAX_LABEL_LEN);
    
    //replaceChar(text, '\n', ' ');  replaceChar(text, '\r', ' ');  replaceChar(text, '\t', ' ');
    gchar* label = textToLine(text, -1);
    GtkWidget *mi = gtk_menu_item_new_with_label(label);
    g_free(label);
    
    int itemLen;
    if (isUtf8)  itemLen = g_utf8_strlen(item, -1);
    else  itemLen = strlen(item);
    
    if (itemLen > MAX_LABEL_LEN * 2)
    {
        if (isUtf8)  g_utf8_strncpy(text, item, MAX_TOOLTIP_LEN);
        else  strncpy(text, item, MAX_TOOLTIP_LEN);

        gtk_widget_set_tooltip_text (mi, text); 
    }

    g_free(text);

    return mi;
}


static void addClipMenuItem(gchar* item, GtkWidget *menu)
{
    GtkWidget *mi = createMenuItem(item);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(mi, "activate", G_CALLBACK(onClipMenuItem), clipMenuItemCount);
    clipMenuItemCount++;
}


// for remote
static void onClipMenuHide(GtkWidget *menu, gpointer user_data)
{
    g_timeout_add_seconds (1, onExitOnHide, NULL);   // wait x11
}


void clipMenuPosFunc(GtkMenu* menu, gint* x, gint* y, gboolean* push_in, gpointer user_data)
{
    int height = gtk_widget_get_allocated_height (menu);

    *x = gdk_screen_width () / 2;
    *y = gdk_screen_height ()/2 - height/2;
}


static void onClear(GtkMenuItem* self, gpointer user_data)
{
    g_list_free_full(clipList, g_free);
    
    clipList = NULL;
    
    saveClipList();  // clear file
}


static gboolean onMenuTimeout (gpointer data)
{
    onClipMenuHide(NULL, NULL);

    return FALSE;
}


static void addPinMenuItem(gchar* item, GtkWidget *menu)
{
    GtkWidget *mi = createMenuItem(item);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(mi, "activate", G_CALLBACK(onPinMenuItem), pinMenuItemCount);  
    pinMenuItemCount++;
}


void showClipMenu(int mode)
{
    GtkWidget *clipMenu = gtk_menu_new();

    if (mode == APP_MODE_REMOTE)
        g_signal_connect((GObject*)clipMenu, "hide", (GCallback)onClipMenuHide, NULL); 

    GtkWidget *mi = 0;
    clipMenuItemCount = 0;
    
    if (clipList == NULL)
    {
        mi = gtk_menu_item_new_with_label("Empty");
        gtk_widget_set_sensitive(mi, FALSE);
        gtk_menu_shell_append((GtkMenuShell*)clipMenu, mi);
    }
    else
        g_list_foreach (clipList, addClipMenuItem, clipMenu);

    mi = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(clipMenu), mi);
    
    GtkWidget *pinMenu = gtk_menu_new();

    mi = gtk_menu_item_new_with_label("Pinned");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), pinMenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(clipMenu), mi);

    pinMenuItemCount = 0;

    if (pinList == NULL)
    {
        mi = gtk_menu_item_new_with_label("Empty");
        gtk_widget_set_sensitive(mi, FALSE);
        gtk_menu_shell_append((GtkMenuShell*)pinMenu, mi);
    }
    else
        g_list_foreach (pinList, addPinMenuItem, pinMenu);

    mi = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(pinMenu), mi);

    mi = gtk_menu_item_new_with_label("Edit");
    gtk_menu_shell_append(GTK_MENU_SHELL(pinMenu), mi);
    g_signal_connect(mi, "activate", G_CALLBACK(onPinEdit), NULL);
    
    
    if (mode == APP_MODE_PRIMARY)
    {
        GtkWidget *clearMenu = gtk_menu_new();
        mi = gtk_menu_item_new_with_label("Clear list");
        gtk_menu_shell_append(GTK_MENU_SHELL(clearMenu), mi);
        g_signal_connect(mi, "activate", G_CALLBACK(onClear), NULL);

        mi = gtk_menu_item_new_with_label("Clear");
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), clearMenu);
        gtk_menu_shell_append(GTK_MENU_SHELL(clipMenu), mi);
    }
    
    gtk_widget_show_all(clipMenu);
    
    if (mode == APP_MODE_REMOTE)
    {
        gtk_menu_popup((GtkMenu*)clipMenu, NULL, NULL, clipMenuPosFunc, NULL, 1, gtk_get_current_event_time());
        
        // Some shortcuts show nothing. Need exit process.
        g_timeout_add (60 * G_TIME_SPAN_MILLISECOND, onMenuTimeout, NULL); 
    }
    else
        gtk_menu_popup((GtkMenu*)clipMenu, NULL, tray, NULL, NULL, 1, gtk_get_current_event_time());
        
}


static void onTrayActivate(GtkStatusIcon *status_icon, gpointer user_data)
{
    showClipMenu(APP_MODE_PRIMARY);
}


static void onTrayMenu(GtkStatusIcon *status_icon, guint button, guint32 activate_time, gpointer popupMenu)
{
    gtk_menu_popup(GTK_MENU(popupMenu), NULL, NULL, gtk_status_icon_position_menu, status_icon, button, activate_time);
}


static gboolean
onShutdownSignal(gpointer data)
{
    onExit(NULL, data);

    return FALSE;
}


gboolean loadClipList(gchar* path)
{
    FILE * clipFile = fopen(path, "rb" );

    if (clipFile == NULL)
    {
        g_printerr("loadClipList:  fopen error:  %d\n",  errno);
        return FALSE;
    }

    gchar* xml = fileReadAll(clipFile);
    
    if (!xml)
    {
        int err = ferror(clipFile);
        if (err)
            g_printerr("loadClipList:  dataFileReadAll error:  %d\n",  errno);
    }
    
    fclose (clipFile);
    clipFile = 0;

    if (xml)
    {
        clipList = xmlGetList(xml, "clip-list");
        g_free(xml);
    
        if (clipList == NULL)
        {
            g_printerr("loadClipList:  No list data in the file\n");
            return FALSE;
        }
    }
    
    return TRUE;  
}


gboolean loadPinList(gchar* path)
{
    gchar* xml = textFileReadAll(path);
    
    if (!xml)
    {
        g_printerr("loadPinList:  textFileReadAll error:  %d\n",  errno);
        if (errno != 2)  return FALSE;
    }
    else
    {
        pinList = xmlGetList(xml, "pinned-list");
        g_free(xml);
    
        if (pinList == NULL)
        {
            g_printerr("loadPinList:  No list data in the file\n");
            //return FALSE;
        }
    }
    
    return TRUE;  
}


void onAppShutdown(GtkApplication *app)
{
    g_print("appShutdown:   \n");
    
    if (display)
        XCloseDisplay(display); 
    
    if (clipFile)
    {
        fclose(clipFile);
        clipFile = 0;
    }

    if (clipList)
    {
        g_list_free_full(clipList, g_free);
        clipList = 0;
    }
}


static void
onAppActivate (GtkApplication *app, Args* args)
{
    g_print("mdclip 0.1.3      7.07.2025\n");
    
    gchar *baseName = g_path_get_basename(args->argv[0]);
    gchar *configDir = g_build_path(G_DIR_SEPARATOR_S, g_get_user_config_dir(), baseName, NULL);
    
    if (!g_file_test(configDir, G_FILE_TEST_EXISTS))
    {
        if (g_mkdir_with_parents(configDir, 0755) != 0)
        {
            g_warning("Couldn't create config directory:  %s\n", configDir);
            configDir = g_strdup(g_get_user_config_dir());
        }
    }
    
    confPath = g_build_path(G_DIR_SEPARATOR_S, configDir, g_strconcat(baseName, ".conf", NULL), NULL);
    clipFilePath = g_strconcat(configDir, "/clip.xml", NULL);
    pinFilePath = g_strconcat(configDir, "/pin.xml", NULL);

    g_print("configDir=%s\n", configDir);
    
    g_free(configDir);
    g_free(baseName);
    
    g_unix_signal_add(SIGHUP, onShutdownSignal, app);
    g_unix_signal_add(SIGINT, onShutdownSignal, app);
    g_unix_signal_add(SIGTERM, onShutdownSignal, app);

    clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);    
    clipSigHandId = g_signal_connect(clip, "owner-change", G_CALLBACK(onClipChanged), NULL);
    
    display = XOpenDisplay(NULL);
    
    if (args->argc > 1)
    {
        //g_print("appActivate: arg=%s\n", args->argv[1]);
        
        // if remote
        if (!strcmp(args->argv[1], "-m"))
        {
            loadConfig();
            loadClipList(clipFilePath);
            loadPinList(pinFilePath);
            
            showClipMenu(APP_MODE_REMOTE);  

            gtk_main (); 
        }
        
        return;  // appShutdown+
    }
    
    g_autoptr(GError) err = NULL;
    pixApp = gdk_pixbuf_new_from_resource ("/app/icons/mdclip.png", &err);

    if (!pixApp && err)
        g_warning ("Load tray icon error: %s\n", err->message);
        
    err = NULL;
    pixOffline = gdk_pixbuf_new_from_resource ("/app/icons/mdclip-offline.png", &err);
    if (!pixOffline && err)
        g_warning ("Load offline icon error: %s\n", err->message);
        
    tray = gtk_status_icon_new_from_pixbuf(pixApp);

    GtkWidget *trayMenu = trayMenuNew(app);
    g_signal_connect(tray, "popup-menu", G_CALLBACK(onTrayMenu), trayMenu);
    g_signal_connect(tray, "activate", G_CALLBACK(onTrayActivate), app);
    gtk_status_icon_set_visible(tray, TRUE);
    //gtk_status_icon_set_tooltip_text(tray, appName);

    loadConfig();
    loadClipList(clipFilePath);
    loadPinList(pinFilePath);
    
    gtk_main ();  // if no windows
}


int
main (int argc, char **argv)
{
    //g_print("main:   argc=%i\n", argc);

    GtkApplication *app;
    int status;

    Args args;
    args.argc = argc;
    args.argv = argv;
    
    app = gtk_application_new ("org.gtk.mdrctrl", G_APPLICATION_NON_UNIQUE);  // G_APPLICATION_NON_UNIQUE  G_APPLICATION_FLAGS_NONE
    
    g_signal_connect (app, "activate", G_CALLBACK (onAppActivate), &args);
    g_signal_connect (app, "shutdown", G_CALLBACK (onAppShutdown), NULL);
    //status = g_application_run (G_APPLICATION (app), argc, argv);
    status = g_application_run (G_APPLICATION (app), NULL, NULL);
    g_object_unref (app);

    return status;
}
