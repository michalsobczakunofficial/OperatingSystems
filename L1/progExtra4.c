#define _XOPEN_SOURCE 700

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

struct Book
{
    char *author;
    char *title;
    char *genre;
};

struct Record
{
    unsigned int size;
    char title[64];
};

char *join_paths(const char *path1, const char *path2)
{
    size_t length = strlen(path1) + strlen(path2) + 2;

    char *path = malloc(length);

    if (path == NULL)
        ERR("malloc");

    snprintf(path, length, "%s/%s", path1, path2);

    return path;
}

void free_book(struct Book *book)
{
    free(book->author);
    free(book->title);
    free(book->genre);
}

void read_metadata(const char *name, struct Book *book)
{
    FILE *s1 = fopen(name, "r");

    if (s1 == NULL)
        ERR("fopen");

    book->author = NULL;
    book->title = NULL;
    book->genre = NULL;

    char *line = NULL;
    size_t len = 0;

    while (getline(&line, &len, s1) != -1)
    {
        char *separator = strchr(line, ':');

        if (separator == NULL)
            continue;

        *separator = '\0';

        char *key = line;
        char *value = separator + 1;

        value[strcspn(value, "\r\n")] = '\0';

        char **field = NULL;

        if (strcmp(key, "author") == 0)
            field = &book->author;
        else if (strcmp(key, "title") == 0)
            field = &book->title;
        else if (strcmp(key, "genre") == 0)
            field = &book->genre;

        if (field != NULL)
        {
            char *copy = strdup(value);

            if (copy == NULL)
                ERR("strdup");

            free(*field);
            *field = copy;
        }
    }

    if (ferror(s1))
        ERR("getline");

    free(line);

    if (fclose(s1) == EOF)
        ERR("fclose");
}

void check_file(const char *name)
{
    struct Book book;

    read_metadata(name, &book);

    printf("author: %s\n", book.author != NULL ? book.author : "missing!");
    printf("title: %s\n", book.title != NULL ? book.title : "missing!");
    printf("genre: %s\n", book.genre != NULL ? book.genre : "missing!");

    free_book(&book);
}

char *short_name(const char *text)
{
    size_t len = strlen(text);

    if (len > 64)
        len = 64;

    char *copy = malloc(len + 1);

    if (copy == NULL)
        ERR("malloc");

    memcpy(copy, text, len);
    copy[len] = '\0';

    if (len == 0 ||
        strcmp(copy, ".") == 0 ||
        strcmp(copy, "..") == 0 ||
        strchr(copy, '/') != NULL)
    {
        fprintf(stderr, "Invalid title or genre: %s\n", text);
        free(copy);
        exit(EXIT_FAILURE);
    }

    return copy;
}

void make_link(const char *dir, const char *filename, const char *target)
{
    char *linkpath = join_paths(dir, filename);

    if (symlink(target, linkpath) == -1)
    {
        if (errno == EEXIST)
            fprintf(stderr, "Duplicate index name: %s\n", linkpath);
        else
            perror("symlink");

        free(linkpath);
        exit(EXIT_FAILURE);
    }

    free(linkpath);
}

void scan_dir(const char *path)
{
    DIR *dirp;
    struct dirent *dp;
    struct stat filestat;

    if ((dirp = opendir(path)) == NULL)
        ERR("opendir");

    while (1)
    {
        errno = 0;

        dp = readdir(dirp);

        if (dp == NULL)
        {
            if (errno != 0)
                ERR("readdir");

            break;
        }

        if (strcmp(dp->d_name, ".") == 0 ||
            strcmp(dp->d_name, "..") == 0)
            continue;

        char *fullpath = join_paths(path, dp->d_name);

        if (lstat(fullpath, &filestat) == -1)
            ERR("lstat");

        if (S_ISDIR(filestat.st_mode))
        {
            scan_dir(fullpath);
        }
        else if (S_ISREG(filestat.st_mode))
        {
            // ETAP 2 - indeks by-visible-title

            char *target2 = join_paths("../..", fullpath);

            make_link("index/by-visible-title", dp->d_name, target2);

            // ETAP 3 - odczyt metadanych

            struct Book book;

            read_metadata(fullpath, &book);

            if (book.title != NULL)
            {
                char *title = short_name(book.title);

                // Indeks by-title

                make_link("index/by-title", title, target2);

                // Indeks by-genre

                if (book.genre != NULL)
                {
                    char *genre = short_name(book.genre);

                    char *genre_path = join_paths("index/by-genre", genre);

                    if (mkdir(genre_path, 0777) == -1)
                    {
                        if (errno != EEXIST)
                            ERR("mkdir");

                        struct stat st;

                        if (stat(genre_path, &st) == -1 ||
                            !S_ISDIR(st.st_mode))
                        {
                            fprintf(stderr, "Invalid genre directory\n");
                            exit(EXIT_FAILURE);
                        }
                    }

                    char *target3 = join_paths("../../..", fullpath);

                    make_link(genre_path, title, target3);

                    free(target3);
                    free(genre_path);
                    free(genre);
                }

                free(title);
            }

            free_book(&book);
            free(target2);
        }

        free(fullpath);
    }

    if (closedir(dirp) == -1)
        ERR("closedir");
}

// ETAP 4 - sprawdzanie pojedynczej książki

void ready(struct Record *record)
{
    char title[65];

    memcpy(title, record->title, 64);
    title[64] = '\0';

    // Odrzucamy niepoprawne nazwy plików

    if (title[0] == '\0' ||
        strcmp(title, ".") == 0 ||
        strcmp(title, "..") == 0 ||
        strchr(title, '/') != NULL)
    {
        printf("Book \"%s\" is missing\n", title);
        return;
    }

    char *path = join_paths("index/by-title", title);

    struct stat filestat;

    if (stat(path, &filestat) == -1)
    {
        if (errno == ENOENT)
        {
            printf("Book \"%s\" is missing\n", title);
        }
        else
        {
            ERR("stat");
        }

        free(path);
        return;
    }

    if (!S_ISREG(filestat.st_mode))
    {
        printf("Book \"%s\" is missing\n", title);
        free(path);
        return;
    }

    if (filestat.st_size != (off_t)record->size)
    {
        printf("Book \"%s\" size mismatch (%u vs %jd)\n",
               title,
               record->size,
               (intmax_t)filestat.st_size);
    }

    free(path);
}

// ETAP 4 - odczytywanie binarnej bazy książek

void check_database(const char *name)
{
    FILE *s1 = fopen(name, "rb");

    if (s1 == NULL)
        ERR("fopen");

    struct Record record;

    // Rekord musi mieć dokładnie 68 bajtów

    if (sizeof(record) != 68)
    {
        fprintf(stderr, "Invalid record structure size\n");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        size_t ret = fread(&record, 1, sizeof(record), s1);

        if (ret == 0)
        {
            if (ferror(s1))
                ERR("fread");

            break;
        }

        if (ret != sizeof(record))
        {
            fprintf(stderr, "Invalid database file size\n");
            exit(EXIT_FAILURE);
        }

        ready(&record);
    }

    if (fclose(s1) == EOF)
        ERR("fclose");
}

void usage(char *pname)
{
    fprintf(stderr, "USAGE: %s [DATABASE_FILE]\n", pname);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    if (argc > 2)
        usage(argv[0]);

    // ETAP 2 - tworzenie indeksu

    if (mkdir("index", 0777) == -1)
        ERR("mkdir index");

    if (mkdir("index/by-visible-title", 0777) == -1)
        ERR("mkdir by-visible-title");

    // ETAP 3 - dodatkowe indeksy

    if (mkdir("index/by-title", 0777) == -1)
        ERR("mkdir by-title");

    if (mkdir("index/by-genre", 0777) == -1)
        ERR("mkdir by-genre");

    scan_dir("library");

    // ETAP 4 - sprawdzanie bazy

    if (argc == 2)
    {
        check_database(argv[1]);
    }

    return EXIT_SUCCESS;
}