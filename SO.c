#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define MAX_DIRS 10 // Numărul maxim de directoare acceptate

// Structura pentru metadatele unui fisier sau director
struct Metadata
{
    char name[256];
    time_t last_modified;
    mode_t mode;
    off_t size;
    ino_t inode;
    time_t snapshot_time; // Marcă temporală pentru snapshot
};

// Functie pentru obtinerea metadatelor unui fisier sau director
struct Metadata get_metadata(const char *path, time_t snapshot_time)
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
    meta.size = file_stat.st_size;
    meta.inode = file_stat.st_ino;
    meta.snapshot_time = snapshot_time; // Adăugăm marca temporală pentru snapshot

    return meta;
}

// Functie pentru crearea unui snapshot pentru un fisier
void creare_snapshot_fisier(const char *file_path, FILE *snapshot_file, time_t snapshot_time)
{
    struct Metadata meta = get_metadata(file_path, snapshot_time);

    // Scriere in snapshot
    fprintf(snapshot_file, "// Fisier: %s\n", meta.name);
    fprintf(snapshot_file, "Marca temporala: %ld\n", meta.snapshot_time);
    fprintf(snapshot_file, "Ultima modificare: %ld\n", meta.last_modified);
    fprintf(snapshot_file, "Dimensiune: %lld octeti\n", meta.size);
    fprintf(snapshot_file, "Permisiuni: %#o\n", meta.mode);
    fprintf(snapshot_file, "Numar inode: %lu\n\n", meta.inode);
}

// Functie pentru crearea unui snapshot pentru un director
void creare_snapshot_director(const char *directory, const char *output_directory, time_t snapshot_time)
{
    DIR *dir;
    struct dirent *entry;
    dir = opendir(directory);
    if (dir == NULL)
    {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue; // Se ignoră "." și ".."
        }

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", directory, entry->d_name);

        if (entry->d_type == DT_REG) // Verificăm dacă este un fișier regulat
        {
            // Construim calea pentru fișierul de snapshot
            char current_snapshot_file_path[512];
            snprintf(current_snapshot_file_path, sizeof(current_snapshot_file_path), "%s/CurrentSnapshot_%s_%s.txt", output_directory, directory, entry->d_name);

            FILE *current_snapshot_file = fopen(current_snapshot_file_path, "w");
            if (current_snapshot_file == NULL)
            {
                perror("fopen");
                exit(EXIT_FAILURE);
            }

            // Crearea snapshot-ului curent pentru fișier
            creare_snapshot_fisier(full_path, current_snapshot_file, snapshot_time);

            fclose(current_snapshot_file);
            printf("Snapshot pentru fisierul %s creat cu succes.\n", entry->d_name);

            // Construim calea pentru fișierul de snapshot anterior
            char prev_snapshot_file_path[512];
            snprintf(prev_snapshot_file_path, sizeof(prev_snapshot_file_path), "%s/PreviousSnapshot_%s_%s.txt", output_directory, directory, entry->d_name);

            // Copiem fișierul de snapshot curent în fișierul de snapshot anterior
            FILE *prev_snapshot_file = fopen(prev_snapshot_file_path, "w");
            if (prev_snapshot_file == NULL)
            {
                perror("fopen");
                exit(EXIT_FAILURE);
            }

            // Deschidem fișierul de snapshot curent pentru citire și copiem conținutul în fișierul de snapshot anterior
            current_snapshot_file = fopen(current_snapshot_file_path, "r");
            if (current_snapshot_file == NULL)
            {
                perror("fopen");
                exit(EXIT_FAILURE);
            }

            int c;
            while ((c = fgetc(current_snapshot_file)) != EOF)
            {
                fputc(c, prev_snapshot_file);
            }

            fclose(prev_snapshot_file);
            fclose(current_snapshot_file);
        }
    }

    closedir(dir);
}

// Functie pentru compararea a două snapshot-uri
void comparare(FILE *prev_snapshot, FILE *current_snapshot)
{
    char line_prev[256];
    char line_curr[256];

    printf("Comparare între snapshot-uri:\n");

    while (fgets(line_prev, sizeof(line_prev), prev_snapshot) && fgets(line_curr, sizeof(line_curr), current_snapshot))
    {
        if (strcmp(line_prev, line_curr) != 0)
        {
            printf("Modificare detectată:\n%s", line_curr);
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3 || argc > MAX_DIRS + 3) // Verificăm dacă numărul de argumente este între 3 și MAX_DIRS + 3
    {
        printf("Utilizare: %s -o <director_iesire> <director1> <director2> ... <directorN>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *output_directory = NULL;
    char *directories[MAX_DIRS];
    int dir_count = 0;

    // Parsăm argumentele din linia de comandă
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-o") == 0)
        {
            if (i + 1 < argc)
            {
                output_directory = argv[i + 1];
                i++; // Sărim peste directorul de ieșire pentru a nu-l trata ca și director de intrare
            }
            else
            {
                printf("Eroare: Argumentul -o necesita specificarea unui director de iesire.\n");
                return EXIT_FAILURE;
            }
        }
        else
        {
            directories[dir_count++] = argv[i];
        }
    }

    if (output_directory == NULL)
    {
        printf("Eroare: Directorul de iesire nu este specificat.\n");
        return EXIT_FAILURE;
    }

    time_t snapshot_time = time(NULL); // Obținem marca temporală pentru snapshot

    // Procesăm fiecare director de intrare
    for (int i = 0; i < dir_count; ++i)
    {
        // Creăm directorul de ieșire dacă nu există deja
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "mkdir -p %s", output_directory);
        system(cmd);

        // Crearea snapshot-ului pentru fiecare fișier din directorul dat ca argument
        creare_snapshot_director(directories[i], output_directory, snapshot_time);
    }

    // Comparăm snapshot-urile și afișăm modificările
    for (int i = 0; i < dir_count; ++i)
    {
        // Construim calea pentru fișierul de snapshot curent
        char current_snapshot_file_path[512];
        snprintf(current_snapshot_file_path, sizeof(current_snapshot_file_path), "%s/CurrentSnapshot_%s_%s.txt", output_directory, directories[i], "file.txt");

        // Construim calea pentru fișierul de snapshot anterior
        char prev_snapshot_file_path[512];
        snprintf(prev_snapshot_file_path, sizeof(prev_snapshot_file_path), "%s/PreviousSnapshot_%s_%s.txt", output_directory, directories[i], "file.txt");

        // Deschidem fișierele de snapshot curent și anterior pentru comparare
        FILE *prev_snapshot_file = fopen(prev_snapshot_file_path, "r");
        FILE *current_snapshot_file = fopen(current_snapshot_file_path, "r");

        if (prev_snapshot_file == NULL || current_snapshot_file == NULL)
        {
            perror("fopen");
            return EXIT_FAILURE;
        }

        // Comparăm snapshot-urile și afișăm modificările
        comparare(prev_snapshot_file, current_snapshot_file);

        fclose(prev_snapshot_file);
        fclose(current_snapshot_file);
    }

    return EXIT_SUCCESS;
}
