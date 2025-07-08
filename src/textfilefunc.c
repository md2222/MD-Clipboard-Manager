#include "textfilefunc.h"


gchar* textFileReadAll(gchar* path)
{
    FILE *file = fopen(path, "rb" );

    if (file == NULL)
    {
        g_printerr("textFileReadAll:  fopen error:  %d\n",  errno);
        return NULL;
    }
    
    char *buf = fileReadAll(file); 

    fclose (file);

    return buf;
}


gchar* fileReadAll(FILE* file)
{
    if(file == NULL) return NULL;
    
    fseek(file, 0 , SEEK_END); 
    long size = ftell(file);   
    
    if (size == 0) return NULL;
    
    char *buf = (char*) malloc(sizeof(char) * size + 2); 
    if (buf == NULL)
    {
        g_printerr("fileReadAll:  malloc error:  %d\n",  errno);
        return NULL;
    }

    rewind (file);  

    *buf = 0;
    size_t result = fread(buf, 1, size, file); 
    if (result != size)
    {
        g_printerr("fileReadAll:  fread error:  %d\n",  errno);
        free(buf);
        return NULL;
    }
    *(buf + size) = 0;

    return buf;
}
