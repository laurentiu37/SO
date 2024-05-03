#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libgen.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_PATH_LEN 1024

int is_keyword(const char *line)
 {
    const char *keywords[] = {"corrupted", "dangerous", "risk", "attack", "malware", "malicious"};
    const int num_keywords = sizeof(keywords) / sizeof(keywords[0]);
    for (int i = 0; i < num_keywords; i++) 
    {
        if (strstr(line, keywords[i]) != NULL)
            return 1;
    }
    return 0;
}

int has_non_ascii(const char *line) 
{
    while (*line) 
    {
        if (!isascii(*line))
            return 1;
        line++;
    }
    return 0;
}

void move_file(const char *file_path, const char *target_dir) 
{
    char target_path[MAX_PATH_LEN];
    snprintf(target_path, sizeof(target_path), "%s/%s", target_dir, basename((char *)file_path));
    if (rename(file_path, target_path) == 0) 
    {
        printf("File %s moved to directory %s.\n", file_path, target_dir);
    } 
    else 
    {
        perror("rename");
        printf("Failed to move file %s to directory %s.\n", file_path, target_dir);
    }
}

void analyze_file(const char *file_path, const char *isolated_dir) 
{
    FILE *file = fopen(file_path, "r");
    if (file == NULL) 
    {
        perror("fopen");
        return;
    }

    char line[1024];
    int line_count = 0;
    int keyword_found = 0;
    int non_ascii_found = 0;

    while (fgets(line, sizeof(line), file) != NULL) 
    {
        line_count++;
        if (!keyword_found && is_keyword(line)) 
        {
            printf("File %s contains a keyword on line %d.\n", file_path, line_count);
            keyword_found = 1;
        }
        if (!non_ascii_found && has_non_ascii(line)) 
        {
            printf("File %s contains non-ASCII characters on line %d.\n", file_path, line_count);
            non_ascii_found = 1;
        }
        // se verifica doar daca unul dintre criterii este indeplinit
        if (keyword_found || non_ascii_found) 
        {
            break;
        }
    }

    fclose(file);

    // daca fisierul e corupt, se muta in directorul pt fisiere corupte
    if (keyword_found || non_ascii_found) 
    {
        move_file(file_path, isolated_dir);
    } 
    else 
    {
        printf("File %s is clean.\n", file_path);
    }
}

void check_directory_recursive(const char *dir_path, const char *output_dir, const char *isolated_dir) 
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir(dir_path);
    if (dir == NULL) 
    {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL) 
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char file_path[MAX_PATH_LEN];
        snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);

        struct stat file_stat;
        if (stat(file_path, &file_stat) == -1) 
        {
            perror("stat");
            continue;
        }

        if (S_ISDIR(file_stat.st_mode)) 
        {
            check_directory_recursive(file_path, output_dir, isolated_dir); // se verifica recursiv
        } 
        else 
        {
            // verific permisiunile existente
            if (!(file_stat.st_mode & S_IRUSR) || !(file_stat.st_mode & S_IWUSR) || !(file_stat.st_mode & S_IXUSR) ||
                !(file_stat.st_mode & S_IRGRP) || !(file_stat.st_mode & S_IWGRP) || !(file_stat.st_mode & S_IXGRP) ||
                !(file_stat.st_mode & S_IROTH) || !(file_stat.st_mode & S_IWOTH) || !(file_stat.st_mode & S_IXOTH)) 
                {
                printf("File %s has missing permissions: ", file_path);
                if (!(file_stat.st_mode & S_IRUSR)) printf("User Read ");
                if (!(file_stat.st_mode & S_IWUSR)) printf("User Write ");
                if (!(file_stat.st_mode & S_IXUSR)) printf("User Execute ");
                if (!(file_stat.st_mode & S_IRGRP)) printf("Group Read ");
                if (!(file_stat.st_mode & S_IWGRP)) printf("Group Write ");
                if (!(file_stat.st_mode & S_IXGRP)) printf("Group Execute ");
                if (!(file_stat.st_mode & S_IROTH)) printf("Others Read ");
                if (!(file_stat.st_mode & S_IWOTH)) printf("Others Write ");
                if (!(file_stat.st_mode & S_IXOTH)) printf("Others Execute");
                printf("\n");

                // procesul pt analiza sintactica a fisierelor
                pid_t pid = fork();
                if (pid == -1) 
                {
                    perror("fork");
                    continue;
                } 
                else if (pid == 0) 
                {
                    // procesul copil
                    analyze_file(file_path, isolated_dir);
                    exit(EXIT_SUCCESS);
                } 
                else 
                {
                    // procesul parinte, se asteapta dupa cel copil
                    int status;
                    wait(&status);
                }
            }
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) 
{
    if (argc < 5 || strcmp(argv[1], "-o") != 0 || strcmp(argv[3], "-s") != 0) 
    {
        printf("Usage: %s -o output_dir -s isolated_space_dir dir1 dir2 dir3\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *output_dir = argv[2];
    const char *isolated_dir = argv[4];

    // se creeaza directorul pt fisiere izolate
    mkdir(isolated_dir, 0700);

    // se trece prin toate directoarele, prin toate subdirectoarele si se verifica fiecare fisier
    for (int i = 5; i < argc; i++) 
    {
        check_directory_recursive(argv[i], output_dir, isolated_dir);
    }

    return 0;
}
