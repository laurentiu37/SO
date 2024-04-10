#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>

struct Metadata {
    char name[256];
    time_t last_modified;
    mode_t mode;
};

struct Metadata colecteaza_date(const char *path) 
{
    struct Metadata meta;
    struct stat file_stat;

    if (stat(path, &file_stat) == -1) 
    {
        perror("stat");
        exit(EXIT_FAILURE);
    }

    strncpy(meta.name, path, 255);
    meta.name[255] = '\0';
    meta.last_modified = file_stat.st_mtime;
    meta.mode = file_stat.st_mode;

    return meta;
}

void creare_snapshot(const char *directory, FILE *snapshot_file) 
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir(directory);
    if (dir == NULL) 
    {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    // Iterăm prin intrările directorului
    while ((entry = readdir(dir)) != NULL) 
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) 
        {
            continue; 
        }

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", directory, entry->d_name);

        struct Metadata meta = colecteaza_date(full_path);

        fprintf(snapshot_file, "%s\n", meta.name);

        if (S_ISDIR(meta.mode)) 
        {
            creare_snapshot(full_path, snapshot_file);
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) 
{
    if (argc != 2) 
    {
        printf("Usage: %s <directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *directory = argv[1];

    FILE *snapshot_file = fopen("Snapshot.txt", "w");
    if (snapshot_file == NULL) 
    {
        perror("fopen");
        return EXIT_FAILURE;
    }

    creare_snapshot(directory, snapshot_file);

    fclose(snapshot_file);
    printf("Snapshot created successfully.\n");

    return EXIT_SUCCESS;
}
