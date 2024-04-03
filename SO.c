#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

typedef struct metadate {
  char nume[256];
  char tip;
  time_t timp_modificare;
  size_t dimensiune;
} metadate;

int compara_metadate(const void *a, const void *b) {
  const metadate *m1 = (const metadate *)a;
  const metadate *m2 = (const metadate *)b;

  int cmp = strcmp(m1->nume, m2->nume);
  if (cmp != 0) {
    return cmp;
  }

  if (m1->tip != m2->tip) {
    return m1->tip - m2->tip;
  }

  if (m1->timp_modificare != m2->timp_modificare) {
    return m1->timp_modificare - m2->timp_modificare;
  }

  return m1->dimensiune - m2->dimensiune;
}

void afisare_metadate(const metadate *m) {
  printf("%s (%c):\n", m->nume, m->tip);
  printf("  Ultima modificare: %s", ctime(&m->timp_modificare));
  printf("  Dimensiune: %zu octeti\n", m->dimensiune);
}

void generare_snapshot(const char *director) {
  DIR *dir = opendir(director);
  if (dir == NULL) {
    perror("Eroare la deschiderea directorului");
    return;
  }

  metadate *metadate_lista = malloc(sizeof(metadate) * 1024);
  int numar_metadate = 0;

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    struct stat stat_buf;
    if (lstat(entry->d_name, &stat_buf) == -1) {
      perror("Eroare la obtinerea statului fisierului/directorului");
      continue;
    }

    metadate m;
    strcpy(m.nume, entry->d_name);
    m.tip = S_ISDIR(stat_buf.st_mode) ? 'd' : 'f';
    m.timp_modificare = stat_buf.st_mtime;
    m.dimensiune = stat_buf.st_size;

    metadate_lista[numar_metadate++] = m;
  }

  qsort(metadate_lista, numar_metadate, sizeof(metadate), compara_metadate);

  FILE *f = fopen("/home/student/Proiect_SO", "w");
  if (f == NULL) {
    perror("Eroare la scrierea snapshot-ului");
    return;
  }

  for (int i = 0; i < numar_metadate; i++) {
    afisare_metadate(&metadate_lista[i], f);
  }

  fclose(f);

  free(metadate_lista);

  closedir(dir);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Utilizare: %s <director_monitorizat> <fisier_snapshot>\n", argv[0]);
    return 1;
  }

  const char *director = argv[1];
  const char *fisier_snapshot = argv[2];

  generare_snapshot(director, fisier_snapshot);

  return 0;
}

