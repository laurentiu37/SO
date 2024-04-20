#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Structura pentru metadatele unui fisier sau director
struct Metadata
{
    char name[256];
    time_t last_modified;
    mode_t mode;
    off_t size;
    ino_t inode;
    int is_snapshot; // Indicator pentru a marca fisierele/directoarele de tip snapshot
    int is_directory; // Indicator pentru a marca daca elementul este un director
};

// Functie pentru obtinerea metadatelor unui fisier sau director
struct Metadata get_metadata(const char *path)
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
    meta.is_snapshot = 0; // Initializam cu 0
    meta.is_directory = S_ISDIR(file_stat.st_mode); // Verificam daca elementul este un director

    return meta;
}

// Functie pentru crearea unui snapshot
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

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue; // Se ignoră "." și ".."
        }

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", directory, entry->d_name);

        struct Metadata meta = get_metadata(full_path);

        // Marcajul fisierelor/directoarelor snapshot
        if (strcmp(entry->d_name, "PreviousSnapshot.txt") == 0 || strcmp(entry->d_name, "CurrentSnapshot.txt") == 0)
        {
            meta.is_snapshot = 1;
        }

        // Scriere in snapshot
        fprintf(snapshot_file, "%s %ld %o %lld %lu %d %d\n", meta.name, meta.last_modified, meta.mode, meta.size, meta.inode, meta.is_snapshot, meta.is_directory);

        // Daca este director, apelam recursiv functia pentru a crea snapshot-ul directorului
        if (meta.is_directory)
        {
            creare_snapshot(full_path, snapshot_file);
        }
    }

    closedir(dir);
}

// Functie pentru detectarea adaugarii unui fisier sau director
void detectare_adaugare(struct Metadata current_entry)
{
    if (current_entry.is_directory)
    {
        printf("Adăugare director: %s\n", current_entry.name);
    }
    else
    {
        printf("Adăugare fișier: %s\n", current_entry.name);
    }
}

// Functie pentru detectarea stergerii unui fișier sau director
void detectare_stergere(struct Metadata prev_entry)
{
    if (prev_entry.is_directory)
    {
        printf("Ștergere director: %s\n", prev_entry.name);
    }
    else
    {
        printf("Ștergere fișier: %s\n", prev_entry.name);
    }
}

// Functie pentru detectarea redenumirii unui fisier sau director
void detectare_redenumire(struct Metadata prev_entry, struct Metadata current_entry)
{
    if (prev_entry.is_directory && current_entry.is_directory)
    {
        printf("Redenumire director: %s -> %s\n", prev_entry.name, current_entry.name);
    }
    else if (!prev_entry.is_directory && !current_entry.is_directory)
    {
        printf("Redenumire fișier: %s -> %s\n", prev_entry.name, current_entry.name);
    }
    else
    {
        printf("Conversie între fișier și director: %s -> %s\n", prev_entry.name, current_entry.name);
    }
}

// Functie pentru compararea a două snapshot-uri si detectarea modificarilor
void comparare(FILE *prev_snapshot, FILE *current_snapshot)
{
    struct Metadata prev_entry;
    struct Metadata current_entry;
    int changes_detected = 0; // Variabila pentru a verifica daca s-au detectat modificari

    // Se compara fiecare fisier/director din snapshot-ul curent cu fiecare fisier/director din snapshot-ul anterior
    while (fscanf(current_snapshot, "%255s %ld %o %lld %lu %d %d\n", current_entry.name, &current_entry.last_modified, &current_entry.mode, &current_entry.size, &current_entry.inode, &current_entry.is_snapshot, &current_entry.is_directory) != EOF)
    {
        if (current_entry.is_snapshot == 1)
        {
            continue; // Se ignora fisierele/directoarele de tip snapshot
        }

        int found = 0;
        rewind(prev_snapshot);

        // Se cauta fisierul/directorul din snapshot-ul curent in snapshot-ul anterior
        while (fscanf(prev_snapshot, "%255s %ld %o %lld %lu %d %d\n", prev_entry.name, &prev_entry.last_modified, &prev_entry.mode, &prev_entry.size, &prev_entry.inode, &prev_entry.is_snapshot, &prev_entry.is_directory) != EOF)
        {
            if (current_entry.inode == prev_entry.inode)
            {
                found = 1;
                if (strcmp(current_entry.name, prev_entry.name) != 0)
                {
                    detectare_redenumire(prev_entry, current_entry);
                    changes_detected = 1;
                }
            }
        }
        if (!found)
        {
            detectare_adaugare(current_entry);
            changes_detected = 1;
        }
    }

    // Se verifica posibilele fisiere/directoare sterse
    rewind(prev_snapshot);
    while (fscanf(prev_snapshot, "%255s %ld %o %lld %lu %d %d\n", prev_entry.name, &prev_entry.last_modified, &prev_entry.mode, &prev_entry.size, &prev_entry.inode, &prev_entry.is_snapshot, &prev_entry.is_directory) != EOF)
    {
        if (prev_entry.is_snapshot == 1)
        {
            continue; // Se ignora fisierele/directoarele de tip snapshot
        }

        int found = 0;
        rewind(current_snapshot);
        while (fscanf(current_snapshot, "%255s %ld %o %lld %lu %d %d\n", current_entry.name, &current_entry.last_modified, &current_entry.mode, &current_entry.size, &current_entry.inode, &current_entry.is_snapshot, &current_entry.is_directory) != EOF)
        {
            if (prev_entry.inode == current_entry.inode)
            {
                found = 1;
                break;
            }
        }
        if (!found)
        {
            detectare_stergere(prev_entry);
            changes_detected = 1;
        }
    }

    if (!changes_detected)
    {
        printf("Nu s-au detectat modificări.\n");
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Utilizare: %s <director>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *directory = argv[1];

    FILE *prev_snapshot_file = fopen("PreviousSnapshot.txt", "r");
    if (prev_snapshot_file == NULL)
    {
        printf("Fisierul anterior de snapshot nu a fost gasit. Se creeaza unul nou...\n");
        prev_snapshot_file = fopen("PreviousSnapshot.txt", "w");
        if (prev_snapshot_file == NULL)
        {
            perror("fopen");
            return EXIT_FAILURE;
        }
        fclose(prev_snapshot_file);
        printf("Primul snapshot creat cu succes.\n");
    }

    FILE *current_snapshot_file = fopen("CurrentSnapshot.txt", "w");
    if (current_snapshot_file == NULL)
    {
        perror("fopen");
        return EXIT_FAILURE;
    }

    // Crearea snapshot-ului curent
    creare_snapshot(directory, current_snapshot_file);

    fclose(current_snapshot_file);
    printf("Snapshot creat cu succes.\n");

    // Se compara snapshot-urile anterior și curent
    prev_snapshot_file = fopen("PreviousSnapshot.txt", "r");
    current_snapshot_file = fopen("CurrentSnapshot.txt", "r");

    if (prev_snapshot_file == NULL || current_snapshot_file == NULL)
    {
        perror("fopen");
        return EXIT_FAILURE;
    }

    comparare(prev_snapshot_file, current_snapshot_file);

    fclose(prev_snapshot_file);
    fclose(current_snapshot_file);

    // Se actualizeaza "PreviousSnapshot.txt" prin copierea continutului din cel curent in cel anterior pentru urmatoarea rulare
    current_snapshot_file = fopen("CurrentSnapshot.txt", "r");
    prev_snapshot_file = fopen("PreviousSnapshot.txt", "w");
    if (prev_snapshot_file == NULL || current_snapshot_file == NULL)
    {
        perror("fopen");
        return EXIT_FAILURE;
    }

    int c;
    while ((c = fgetc(current_snapshot_file)) != EOF)
    {
        fputc(c, prev_snapshot_file);
    }

    fclose(prev_snapshot_file);
    fclose(current_snapshot_file);

    return EXIT_SUCCESS;
}
