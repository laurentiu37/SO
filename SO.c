#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    char name[256];
    size_t size;
    time_t timestamp;
} FileInfo;

void compara(const char *dir_name, FileInfo *prev_info, int *prev_count)
{
    DIR *dir = opendir(dir_name);
    struct dirent *entry;
    int count = 0;
    if(dir == NULL)
    {
        printf("Nu s-a putut deschide directorul.\n");
        exit(EXIT_FAILURE);
    }
    while((entry = readdir(dir)) != NULL)
    {
        char *file_path;
        size_t path_len = strlen(dir_name) + strlen(entry->d_name) + 2;
        file_path = (char *)malloc(path_len);
        if(file_path == NULL)
        {
            printf("Eroare la alocarea memoriei!");
            exit(EXIT_FAILURE);
        }
        snprintf(file_path, sizeof(file_path), "%s %s", dir_name, entry->d_name);
        struct stat file_stat;
        if(stat(file_path, &file_stat) == -1)
        {
            printf("Eroare la obtinerea informatiilor desre %s.\n", entry->d_name);
            free(file_path);
            continue;
        }

        if(S_ISREG(file_stat.st_mode))
        {
            strcpy(prev_info[count].name, entry->d_name);
            prev_info[count].size = file_stat.st_size;
            prev_info[count].timestamp = file_stat.st_mtime;
            count++;
        }
        free(file_path);
    }
    closedir(dir);
    *prev_count = count;
}

int main()
{
    char *dir_name = "/home/student/Proiect_SO/";
    FileInfo prev_info[100];
    int prev_count = 0;
    compara(dir_name, prev_info, &prev_count);
    printf("Modificari intre cele 2 rulari consecutive:\n");
    FileInfo curr_info[100];
    int curr_count = 0;
    compara(dir_name, curr_info, &curr_count);

    for(int i = 0; i < curr_count; i++)
    {
        int found = 0;
        for(int j = 0; i < prev_count; j++)
        {
            if(strcmp(curr_info[i].name, prev_info[j].name) == 0)
            {
                found = 1;
                if(curr_info[i].size != prev_info[i].size || curr_info[i].timestamp != prev_info[j].timestamp)
                {
                    printf("Fisier modificat: %s\n", curr_info[i].name);
                }
                break;
            }
        }
        if(!found)
        {
            printf("Fisier nou adaugat: %s\n", curr_info[i].name);
        }
    }

    return 0;
}
