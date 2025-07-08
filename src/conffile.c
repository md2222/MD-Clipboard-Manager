#include "conffile.h"



GKeyFile* confOpen(const char* fileName)
{
    GKeyFile* file = g_key_file_new ();

    //GError *err = NULL;
    g_autoptr(GError) err = NULL;  // pointer variable with automatic cleanup

    if (!g_key_file_load_from_file(file, fileName, G_KEY_FILE_NONE, &err))
    {
        if (err && !g_error_matches(err, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        {
            g_warning ("Error loading key file: %s", err->message);
            //g_error_free(err);
        }
    }

    return file;
}


gboolean confSaveToFile(GKeyFile* file, const char* fileName)
{
    if (!file)  return FALSE;

    if (!g_file_test(fileName, G_FILE_TEST_EXISTS))
    {
        GFile* file = g_file_new_for_path (fileName);
        printf("Config file created:  %s\n", fileName);
    }

    g_autoptr(GError) err = NULL;

    if (!g_key_file_save_to_file(file, fileName, &err))
    {
        if (err)
        {
            g_warning ("Error saving key file: %s\n%s\n", err->message, fileName);
        }
            
        return FALSE;
    }

    //g_key_file_free(file);

    return TRUE;
}


gchar* confGetString(GKeyFile* file, const char* group, const char* name, const char* def)
{
    g_autoptr(GError) err = NULL;

    gchar *val = g_key_file_get_string(file, group, name, &err);

    if (val == NULL || err)
    {
        g_warning ("Error finding key in key file: %s", err->message);
        val = g_strdup(def);
    }

    return val;
}


void confSetString(GKeyFile* file, const char* group, const char* name, const char* val)
{
    g_key_file_set_string (file, group, name, val);
}


int confGetInt(GKeyFile* file, const char* group, const char* name, gint def)
{
    g_autoptr(GError) err = NULL;

    gint val = g_key_file_get_integer(file, group, name, &err);

    if (err)
    {
        g_warning ("Error finding key in key file: %s", err->message);
        val = def;
    }

    return val;
}


void confSetInt(GKeyFile* file, const char* group, const char* name, gint val)
{
    g_key_file_set_integer(file, group, name, val);
}


void confSetList(GKeyFile* file, const char* group, const char* name, int type, void* list, gsize size)
{
    if (type == LT_INT)
    {
        g_key_file_set_integer_list(file, group, name, (gint*)list, size);
    }
}


void* confGetList(GKeyFile* file, const char* group, const char* name, int type, gsize size)
{
    gsize sz = 0;
    void* list = NULL;
    void* res = NULL;

    if (type == LT_INT)
    {
        g_autoptr(GError) err = NULL;
        
        list = g_key_file_get_integer_list(file, group, name, &sz, &err);

        if (sz && sz == size)
        {
            gsize len = sizeof(gint) * sz;
            res = g_malloc(len);
            memcpy(res, list, len);
            g_free(list);
        }
        
        if (err)
        {
            g_warning ("confGetList:    error=%s", err->message);
        }
    }

    return res;
}


void confSetRect(GKeyFile* file, const char* group, const char* name, GdkRectangle* rect)
{
    g_key_file_set_integer_list(file, group, name, (gint*)rect, 4);
}


GdkRectangle confGetRect(GKeyFile* file, const char* group, const char* name)
{
    GdkRectangle rect = { -1, -1, -1, -1 };
    gsize sz = 0;
    g_autoptr(GError) err = NULL;

    gint* list = g_key_file_get_integer_list(file, group, name, &sz, &err);

    if (sz && sz == 4)
    {
        rect.x = list[0];  rect.y = list[1];  rect.width = list[2];  rect.height = list[3];
        g_free(list);
    }
    
    if (err)
    {
        g_warning ("confGetRect:    error=%s", err->message);
    }

    return rect;
}


gboolean confRectIsEmpty (GdkRectangle* rect)
{
     return (rect->width <= 0 || rect->height <= 0);
}
