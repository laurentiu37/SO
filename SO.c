#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_DIRECTORIES 10

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
    meta.is_snapshot = 0;    // Initializam cu 0
    meta.is_directory = S_ISDIR(file_stat.st_mode);  // Verificam daca elementul este un director

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
            continue;  // Se ignoră "." și ".."
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
int comparare(FILE *prev_snapshot, FILE *current_snapshot)
{
    struct Metadata prev_entry;
    struct Metadata current_entry;
    int changes_detected = 0;   // Variabila pentru a verifica daca s-au detectat modificari
    int any_changes_detected = 0;  
    
    // Se compara fiecare fisier/director din snapshot-ul curent cu fiecare fisier/director din snapshot-ul anterior
    while (fscanf(current_snapshot, "%255s %ld %o %lld %lu %d %d\n", current_entry.name, &current_entry.last_modified, &current_entry.mode, &current_entry.size, &current_entry.inode, &current_entry.is_snapshot, &current_entry.is_directory) != EOF)
    {
        if (current_entry.is_snapshot == 1)
        {
            continue;  // Se ignora fisierele/directoarele de tip snapshot
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
                    any_changes_detected = 1;
                }
            }
        }
        if (!found)
        {
            detectare_adaugare(current_entry);
            changes_detected = 1;
            any_changes_detected = 1;
        }
    }
    
    // Se verifica posibilele fisiere/directoare sterse
    rewind(prev_snapshot);
    while (fscanf(prev_snapshot, "%255s %ld %o %lld %lu %d %d\n", prev_entry.name, &prev_entry.last_modified, &prev_entry.mode, &prev_entry.size, &prev_entry.inode, &prev_entry.is_snapshot, &prev_entry.is_directory) != EOF)
    {
        if (prev_entry.is_snapshot == 1)
        {
            continue;  // Se ignora fisierele/directoarele de tip snapshot
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
            any_changes_detected = 1;
        }
    }

    if (!any_changes_detected)
    {
        return 0;
    }

    if (changes_detected)
    {
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 4 || argc > MAX_DIRECTORIES + 2)
    {
        printf("Utilizare: %s -o <director_iesire> <director1> <director2> ... (maxim 10 directoare)\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *output_directory;
    char *directories[MAX_DIRECTORIES];
    int num_directories = argc - 3;
    int dir_index = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-o") == 0)
        {
            if (i + 1 >= argc)
            {
                printf("Eroare: Argumentul '-o' trebuie să fie urmat de un director de ieșire.\n");
                return EXIT_FAILURE;
            }
            output_directory = argv[++i];
        }
        else
        {
            directories[dir_index++] = argv[i];
        }
    }

    if (num_directories > MAX_DIRECTORIES)
    {
        printf("Eroare: Numărul maxim de directoare permis este %d.\n", MAX_DIRECTORIES);
        return EXIT_FAILURE;
    }

    FILE *prev_snapshot_file;
    FILE *current_snapshot_file;

    char prev_snapshot_path[256];
    char current_snapshot_path[256];

    snprintf(prev_snapshot_path, sizeof(prev_snapshot_path), "%s/PreviousSnapshot.txt", output_directory);
    snprintf(current_snapshot_path, sizeof(current_snapshot_path), "%s/CurrentSnapshot.txt", output_directory);

    prev_snapshot_file = fopen(prev_snapshot_path, "r");
    if (prev_snapshot_file == NULL)
    {
        printf("Fisierul anterior de snapshot nu a fost gasit. Se creeaza unul nou...\n");
        prev_snapshot_file = fopen(prev_snapshot_path, "w");
        if (prev_snapshot_file == NULL)
        {
            perror("fopen");
            return EXIT_FAILURE;
        }
        fclose(prev_snapshot_file);
        printf("Primul snapshot creat cu succes.\n");
    }

    current_snapshot_file = fopen(current_snapshot_path, "w");
    if (current_snapshot_file == NULL)
    {
        perror("fopen");
        return EXIT_FAILURE;
    }
    
    for (int i = 0; i < num_directories; i++)
    {
    	// Crearea snapshot-ului curent
        creare_snapshot(directories[i], current_snapshot_file);
    }

    fclose(current_snapshot_file);

    int any_changes_detected = 0;

    for (int i = 0; i < num_directories; i++)
    {
        snprintf(prev_snapshot_path, sizeof(prev_snapshot_path), "%s/PreviousSnapshot.txt", output_directory);
        snprintf(current_snapshot_path, sizeof(current_snapshot_path), "%s/CurrentSnapshot.txt", output_directory);
        
        // Se compara snapshot-urile anterior și curent
        prev_snapshot_file = fopen(prev_snapshot_path, "r");
        current_snapshot_file = fopen(current_snapshot_path, "r");

        if (prev_snapshot_file == NULL || current_snapshot_file == NULL)
        {
            perror("fopen");
            return EXIT_FAILURE;
        }

        if (comparare(prev_snapshot_file, current_snapshot_file))
        {
            any_changes_detected = 1;
        }

        fclose(prev_snapshot_file);
        fclose(current_snapshot_file);
        
        // Se actualizeaza "PreviousSnapshot.txt" prin copierea continutului din cel curent in cel anterior pentru urmatoarea rulare
        current_snapshot_file = fopen(current_snapshot_path, "r");
        prev_snapshot_file = fopen(prev_snapshot_path, "w");
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
    }

    if (!any_changes_detected)
    {
        printf("Nu s-au detectat modificări.\n");
    }

    return EXIT_SUCCESS;
}
