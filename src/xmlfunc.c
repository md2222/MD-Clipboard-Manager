#include "xmlfunc.h"



gchar* strReplace(const char* str, const char* from, const char* to)
{
    if (!str)  return NULL;
    
    size_t resLen = strlen(str) + 1;
    char* res = g_malloc(resLen);
    *res = 0;
    
    if (!from || !strlen(from))  return strcpy(res, str);
    
    size_t fromLen = strlen(from);
    size_t toLen = strlen(to);
    size_t incLen = toLen > fromLen ? toLen - fromLen : 0;

    const char *p1 = str;
    char *p2 = 0;
    
    while ((p2 = strstr(p1, from))) 
    {
        size_t len = p2 - p1;
        strncat(res, p1, len);

        if (incLen)  
        {
            resLen += incLen;
            res = g_realloc(res, resLen);
        }
        strcat(res, to);
        
        p1 = p2 + fromLen;
    }
    
    strcat(res, p1);
    
    return res;
}

gchar* getTagContent(gchar* xml, gchar* tag, int *pos)
{
    //printf("getTagContent:    \n");
    
    gchar* beginTag = g_strconcat("<", tag, ">", NULL);
    gchar* endTag = g_strconcat("</", tag, ">", NULL);
    gchar* p1 = strstr(xml + *pos, beginTag);
    //printf("getTagContent:    p1=%d\n", p1 - data + *pos);

    if (p1)
    {
        p1 += strlen(beginTag);
        //printf("readClipFile:    p1=%d\n", p1);
        gchar* p2 = strstr(p1, endTag);
        //printf("getTagContent:    p2=%d\n", p2 - p1);
        if (p2 > p1)
        {
            *pos = p2 + strlen(endTag) - xml;
            //printf("getTagContent:    pos=%d\n", *pos);
            gchar* content = g_strndup(p1, p2 - p1);
            if (strlen(content) >= 4)
            {
                gchar* tmp = strReplace(content, "&lt;", "<");
                g_free(content);
                content = strReplace(tmp, "&amp;", "&");
                g_free(tmp);
            }
            return content;
        }
    }

    return NULL;
} 


GList* xmlGetList(gchar* xml, gchar* listTag)
{
    if (xml == NULL || !strlen(xml) || listTag == NULL || !strlen(listTag))  return NULL;

    int pos = 0;
    
    gchar* listElem = getTagContent(xml, listTag, &pos);

    if (listElem == NULL)  return NULL;
    
    GList* list = NULL;
    pos = 0;
    
    while (1)
    {
        gchar* item = getTagContent(listElem, "item", &pos);
        if (!item)  break;
        //printf("readClipFile:    item=%s\n", item);
        
        list = g_list_append(list, item);
    }
    
    free (listElem);  
    
    return list;
}


static void listSetItem (gchar* data, gchar** xml)
{
    //g_print("xmlData:    data=%s\n", data);
    gchar* tmp = strReplace(data, "&", "&amp;");
    gchar* content = strReplace(tmp, "<", "&lt;");
    g_free(tmp);

    gchar* elem = g_strconcat(*xml, "<item>", content, "</item>\n", NULL);
    g_free(content);
    g_free(*xml);
    
    *xml = elem;
}


gchar* xmlSetList(gchar* xml, GList* list, gchar* listName)
{
    // TODO - delete existing
    
    gchar* temp = g_strconcat(xml, "<", listName, ">\n", NULL);

    if (list)
        g_list_foreach (list, listSetItem, &temp);

    gchar* elem = g_strconcat(temp, "</", listName, ">\n", NULL);
    g_free(temp);
    
    return elem;
}
    

