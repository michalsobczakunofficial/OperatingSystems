#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(char *pname)
{
    fprintf(stderr, "USAGE: %s -c -v ENVIRONMENT\n", pname);
    fprintf(stderr, "       %s -v ENVIRONMENT [-v ENVIRONMENT...] -i PACKAGE==VERSION\n", pname);
    fprintf(stderr, "       %s -v ENVIRONMENT [-v ENVIRONMENT...] -r PACKAGE\n", pname);
    exit(EXIT_FAILURE);
}

char *make_path(const char *directory, const char *name)
{
    size_t length = strlen(directory) + strlen(name) + 2;

    char *path = malloc(length);

    if (path == NULL)
        ERR("malloc");

    snprintf(path, length, "%s/%s", directory, name);

    return path;
}

void make_file(char *name, ssize_t size, mode_t perms)
{
    FILE *s1;

    int fd = open(name, O_WRONLY | O_CREAT | O_EXCL, 0600);

    if (fd == -1)
        ERR("open");

    if ((s1 = fdopen(fd, "wb")) == NULL)
    {
        close(fd);
        ERR("fdopen");
    }

    for (ssize_t i = 0; i < size; i++)
    {
        unsigned char c = rand() % 256;

        if (fwrite(&c, 1, 1, s1) != 1)
            ERR("fwrite");
    }

    if (fclose(s1))
        ERR("fclose");

    if (chmod(name, perms) == -1)
        ERR("chmod");
}

int package_exists(char *path, char *package)
{
    FILE *s1 = fopen(path, "r");

    if (s1 == NULL)
        ERR("fopen");

    char *line = NULL;
    size_t size = 0;
    ssize_t ret;
    int found = 0;

    while ((ret = getline(&line, &size, s1)) != -1)
    {
        size_t len = strlen(package);

        if (strncmp(line, package, len) == 0 &&
            isspace((unsigned char)line[len]))
        {
            found = 1;
            break;
        }
    }

    if (ferror(s1))
        ERR("getline");

    free(line);

    if (fclose(s1))
        ERR("fclose");

    return found;
}

void create_environment(const char *name)
{
    if (mkdir(name, 0777) == -1)
        ERR("mkdir");

    char *path = make_path(name, "requirements");

    FILE *s1 = fopen(path, "w");

    if (s1 == NULL)
        ERR("fopen");

    if (fclose(s1))
        ERR("fclose");

    printf("Created environment: %s\n", name);

    free(path);
}

void install_package(const char *name, char *package, char *version)
{
    struct stat st;

    if (stat(name, &st) == -1 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "Environment does not exist: %s\n", name);
        exit(EXIT_FAILURE);
    }

    char *path = make_path(name, "requirements");

    if (package_exists(path, package))
    {
        fprintf(stderr, "Package already installed: %s\n", package);
        free(path);
        exit(EXIT_FAILURE);
    }

    char *package_path = make_path(name, package);

    if (lstat(package_path, &st) == 0)
    {
        fprintf(stderr, "Package file already exists: %s\n", package_path);
        free(package_path);
        free(path);
        exit(EXIT_FAILURE);
    }

    if (errno != ENOENT)
        ERR("lstat");

    make_file(package_path, 1000, 0444);

    FILE *s1 = fopen(path, "a");

    if (s1 == NULL)
        ERR("fopen");

    if (fprintf(s1, "%s %s\n", package, version) < 0)
        ERR("fprintf");

    if (fclose(s1))
        ERR("fclose");

    printf("Installed %s version %s in %s\n", package, version, name);

    free(package_path);
    free(path);
}

void remove_package(const char *name, char *package)
{
    struct stat st;

    if (stat(name, &st) == -1 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "Environment does not exist: %s\n", name);
        exit(EXIT_FAILURE);
    }

    char *path = make_path(name, "requirements");

    if (!package_exists(path, package))
    {
        fprintf(stderr, "Package not installed: %s\n", package);
        free(path);
        exit(EXIT_FAILURE);
    }

    char *package_path = make_path(name, package);

    if (lstat(package_path, &st) == -1)
        ERR("lstat");

    if (!S_ISREG(st.st_mode))
    {
        fprintf(stderr, "Package is not a regular file: %s\n", package);
        exit(EXIT_FAILURE);
    }

    FILE *s1 = fopen(path, "r");

    if (s1 == NULL)
        ERR("fopen");

    char *temp_path = make_path(name, "requirementsXXXXXX");

    int fd = mkstemp(temp_path);

    if (fd == -1)
        ERR("mkstemp");

    FILE *s2 = fdopen(fd, "w");

    if (s2 == NULL)
    {
        close(fd);
        unlink(temp_path);
        ERR("fdopen");
    }

    char *line = NULL;
    size_t size = 0;
    ssize_t ret;

    while ((ret = getline(&line, &size, s1)) != -1)
    {
        size_t len = strlen(package);

        if (strncmp(line, package, len) == 0 &&
            isspace((unsigned char)line[len]))
        {
            continue;
        }

        if (fputs(line, s2) == EOF)
            ERR("fputs");
    }

    if (ferror(s1))
        ERR("getline");

    free(line);

    if (fclose(s1))
        ERR("fclose");

    if (fclose(s2))
        ERR("fclose");

    if (unlink(package_path) == -1)
        ERR("unlink");

    if (rename(temp_path, path) == -1)
        ERR("rename");

    printf("Removed package %s from %s\n", package, name);

    free(temp_path);
    free(package_path);
    free(path);
}

int valid_package(const char *package)
{
    if (package == NULL || *package == '\0')
        return 0;

    if (strcmp(package, ".") == 0 ||
        strcmp(package, "..") == 0 ||
        strcmp(package, "requirements") == 0)
        return 0;

    for (int i = 0; package[i] != '\0'; i++)
    {
        unsigned char c = package[i];

        if (!isalnum(c) && c != '_' && c != '-' && c != '.')
            return 0;
    }

    return 1;
}

int main(int argc, char **argv)
{
    int opt;
    int create = 0;
    int env_count = 0;

    const char **environs = malloc(argc * sizeof(*environs));

    if (environs == NULL)
        ERR("malloc");

    char *package = NULL;
    char *version = NULL;
    char *remove_name = NULL;

    srand(time(NULL));

    while ((opt = getopt(argc, argv, "v:ci:r:")) != -1)
    {
        switch (opt)
        {
        case 'c':
            if (create)
                usage(argv[0]);

            create = 1;
            break;

        case 'v':
            environs[env_count++] = optarg;
            break;

        case 'i':
        {
            if (package != NULL)
                usage(argv[0]);

            char *separator = strstr(optarg, "==");

            if (separator == NULL ||
                separator == optarg ||
                separator[2] == '\0')
            {
                fprintf(stderr, "Invalid package format\n");
                free(environs);
                return EXIT_FAILURE;
            }

            *separator = '\0';

            package = optarg;
            version = separator + 2;

            if (!valid_package(package) ||
                strchr(version, '/') != NULL ||
                strchr(version, '=') != NULL ||
                strchr(version, '\n') != NULL ||
                strchr(version, ' ') != NULL ||
                strchr(version, '\t') != NULL)
            {
                fprintf(stderr, "Invalid package name or version\n");
                free(environs);
                return EXIT_FAILURE;
            }

            break;
        }

        case 'r':
            if (remove_name != NULL)
                usage(argv[0]);

            remove_name = optarg;

            if (!valid_package(remove_name))
            {
                fprintf(stderr, "Invalid package name\n");
                free(environs);
                return EXIT_FAILURE;
            }

            break;

        default:
            free(environs);
            usage(argv[0]);
        }
    }

    if (optind != argc || env_count == 0)
    {
        free(environs);
        usage(argv[0]);
    }

    if (create)
    {
        if (env_count != 1 || package != NULL || remove_name != NULL)
        {
            free(environs);
            usage(argv[0]);
        }
    }
    else
    {
        if ((package == NULL && remove_name == NULL) ||
            (package != NULL && remove_name != NULL))
        {
            free(environs);
            usage(argv[0]);
        }
    }

    for (int i = 0; i < env_count; i++)
    {
        const char *name = environs[i];

        if (create)
        {
            create_environment(name);
        }
        else if (package != NULL)
        {
            install_package(name, package, version);
        }
        else if (remove_name != NULL)
        {
            remove_package(name, remove_name);
        }
    }

    free(environs);

    return EXIT_SUCCESS;
}