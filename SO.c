#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>

// Structura pentru metadatele unei intrări (fișier sau director)
struct Metadata {
    char name[256];
    time_t last_modified;
    mode_t mode;
    off_t size;
};

// Funcție pentru a obține metadatele unei intrări
struct Metadata get_metadata(const char *path) {
    struct Metadata meta;
    struct stat file_stat;

    if (stat(path, &file_stat) == -1) {
        perror("stat");
        exit(EXIT_FAILURE);
    }

    strncpy(meta.name, path, 255);
    meta.name[255] = '\0'; // Asigurăm terminarea stringului
    meta.last_modified = file_stat.st_mtime;
    meta.mode = file_stat.st_mode;
    meta.size = file_stat.st_size;

    return meta;
}

// Funcție pentru a crea snapshot-ul și a scrie metadatele într-un fișier
void create_snapshot(const char *directory, FILE *snapshot_file) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(directory);
    if (dir == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    // Iterăm prin intrările directorului
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; // Ignorăm intrările speciale "." și ".."
        }

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", directory, entry->d_name);

        struct Metadata meta = get_metadata(full_path);

        // Scriem metadatele în fișierul snapshot
        fprintf(snapshot_file, "%s\t%ld\t%o\t%lld\n", meta.name, meta.last_modified, meta.mode, meta.size);

        // Dacă este un director, apelăm recursiv funcția pentru a crea snapshot-ul acestuia
        if (S_ISDIR(meta.mode)) {
            create_snapshot(full_path, snapshot_file);
        }
    }

    closedir(dir);
}

// Funcție pentru a compara două snapshot-uri și a evidenția modificările
void compare_snapshots(FILE *prev_snapshot, FILE *current_snapshot) {
    struct Metadata prev_entry;
    struct Metadata current_entry;
    int changes_detected = 0; // Variabila pentru a verifica daca s-au detectat modificari

    // Citim metadatele din ambele snapshot-uri
    while (fscanf(prev_snapshot, "%255s %ld %o %lld\n", prev_entry.name, &prev_entry.last_modified, &prev_entry.mode, &prev_entry.size) != EOF &&
           fscanf(current_snapshot, "%255s %ld %o %lld\n", current_entry.name, &current_entry.last_modified, &current_entry.mode, &current_entry.size) != EOF) {
        // Comparam metadatele pentru a detecta modificari
        if (strcmp(prev_entry.name, current_entry.name) != 0) {
            printf("Adaugare: %s\n", current_entry.name);
            changes_detected = 1;
        } else {
            if (prev_entry.last_modified != current_entry.last_modified) {
                printf("Modificare: %s (ultima modificare)\n", current_entry.name);
                changes_detected = 1;
            }
            if (prev_entry.size != current_entry.size) {
                printf("Modificare: %s (dimensiune)\n", current_entry.name);
                changes_detected = 1;
            }
            // Alte comparatii pot fi adaugate aici, in functie de necesitati
        }
    }

    // Verificam daca exista fisiere sau directoare adaugate in snapshot-ul curent
    while (fscanf(current_snapshot, "%255s %ld %o %lld\n", current_entry.name, &current_entry.last_modified, &current_entry.mode, &current_entry.size) != EOF) {
        printf("Adaugare: %s\n", current_entry.name);
        changes_detected = 1;
    }

    // Verificam daca exista fisiere sau directoare sterse din snapshot-ul curent
    while (fscanf(prev_snapshot, "%255s %ld %o %lld\n", prev_entry.name, &prev_entry.last_modified, &prev_entry.mode, &prev_entry.size) != EOF) {
        printf("Stergere: %s\n", prev_entry.name);
        changes_detected = 1;
    }

    if (!changes_detected) {
        printf("Nu s-au detectat modificari.\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *directory = argv[1];

    FILE *prev_snapshot_file = fopen("PreviousSnapshot.txt", "r");
    if (prev_snapshot_file == NULL) {
        printf("Previous snapshot file not found. Creating a new one...\n");
        prev_snapshot_file = fopen("PreviousSnapshot.txt", "w");
        if (prev_snapshot_file == NULL) {
            perror("fopen");
            return EXIT_FAILURE;
        }
        create_snapshot(directory, prev_snapshot_file);
        fclose(prev_snapshot_file);
        printf("First snapshot created successfully.\n");
        return EXIT_SUCCESS;
    }

    FILE *current_snapshot_file = fopen("CurrentSnapshot.txt", "w");
    if (current_snapshot_file == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    create_snapshot(directory, current_snapshot_file);

    fclose(current_snapshot_file);
    printf("Snapshot created successfully.\n");

    // Comparăm cele două snapshot-uri
    prev_snapshot_file = fopen("PreviousSnapshot.txt", "r");
    if (prev_snapshot_file == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    current_snapshot_file = fopen("CurrentSnapshot.txt", "r");
    if (current_snapshot_file == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    compare_snapshots(prev_snapshot_file, current_snapshot_file);

    // Actualizăm fișierul "PreviousSnapshot.txt"
    fclose(prev_snapshot_file);
    prev_snapshot_file = fopen("PreviousSnapshot.txt", "w");
    if (prev_snapshot_file == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    // Copiem conținutul din "CurrentSnapshot.txt" în "PreviousSnapshot.txt"
    current_snapshot_file = fopen("CurrentSnapshot.txt", "r");
    if (current_snapshot_file == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    int c;
    while ((c = fgetc(current_snapshot_file)) != EOF) {
        fputc(c, prev_snapshot_file);
    }

    fclose(prev_snapshot_file);
    fclose(current_snapshot_file);

    return EXIT_SUCCESS;
}


// de modificat astfel incat sa nu verifice cele 2 snapshot uri, strict fisierele daca au fost sau nu modificate si ce modificari au aparut.