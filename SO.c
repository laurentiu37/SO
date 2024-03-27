#include <stdio.h>
#include <dirent.h>
#include <string.h>

int main()
{
    char *dir_name = "/home/student/Proiect_SO/";
    DIR *dir = opendir(dir_name);
    if(dir == NULL)
    {
        printf("Nu s-a putut deschide directorul.\n");
        return 1;
    }

    struct dirent *entry;

    while((entry = readdir(dir)) != NULL)
    {
        if(strcmp(entry->d_name,".") != 0 && strcmp(entry->d_name,"..") != 0)
        {
            printf("%s\n", entry->d_name);
        }
    }

    closedir(dir);
    return 0;
}
