#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>

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

// Functie pentru crearea unui snapshot intr-un proces copil
void creare_snapshot_copil(const char *directory, const char *snapshot_path)
{
    FILE *snapshot_file = fopen(snapshot_path, "w");
    if (snapshot_file == NULL)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

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
            creare_snapshot_copil(full_path, snapshot_path);
        }
    }

    closedir(dir);
    fclose(snapshot_file);
}

// Functie pentru crearea unui snapshot in procesul parinte
void creare_snapshot_parinte(const char *directory, const char *output_directory, pid_t child_pid)
{
    int status;
    waitpid(child_pid, &status, 0);  // Așteaptă terminarea procesului copil

    if (WIFEXITED(status))
    {
        printf("Snapshot pentru %s creat cu succes.\n", directory);
        printf("Proces copil terminat cu PID %d și exit code %d.\n", child_pid, WEXITSTATUS(status));
    }
    else
    {
        printf("Eroare: Procesul copil cu PID %d a fost întrerupt în mod neașteptat.\n", child_pid);
    }
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

    for (int i = 0; i < num_directories; i++)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork");
            return EXIT_FAILURE;
        }
        else if (pid == 0)  // Proces copil
        {
            char snapshot_dir[256];
            snprintf(snapshot_dir, sizeof(snapshot_dir), "%s/%s_snapshot", output_directory, directories[i]);
            char current_snapshot_path[256];
            snprintf(current_snapshot_path, sizeof(current_snapshot_path), "%s/CurrentSnapshot.txt", snapshot_dir);
            
            // Creare director pentru snapshot, dacă nu există
            mkdir(snapshot_dir, 0777);

            // Actualizare snapshot pentru director
            creare_snapshot_copil(directories[i], current_snapshot_path);
            exit(EXIT_SUCCESS);  // Termină procesul copil
        }
        else  // Proces parinte
        {
            creare_snapshot_parinte(directories[i], output_directory, pid);
        }
    }

    return EXIT_SUCCESS;
}
