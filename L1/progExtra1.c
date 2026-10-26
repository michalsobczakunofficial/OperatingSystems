#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void usage(const char *program)
{
    fprintf(stderr, "USAGE: %s [-p directory]... [-o file]\n", program);
    exit(EXIT_FAILURE);
}

int scan_dir(const char *path, FILE *out)
{
    DIR *dir;
    struct dirent *dp;
    struct stat filestat;

    if ((dir = opendir(path)) == NULL)
    {
        fprintf(stderr, "opendir %s: %s\n", path, strerror(errno));
        return -1;
    }

    fprintf(out, "SCIEZKA:\n%s\nLISTA PLIKOW:\n", path);

    int error = 0;

    while (1)
    {
        errno = 0;

        dp = readdir(dir);

        if (dp == NULL)
        {
            if (errno != 0)
            {
                perror("readdir");
                error = 1;
            }
            break;
        }

        if (fstatat(dirfd(dir), dp->d_name, &filestat,
                    AT_SYMLINK_NOFOLLOW) == -1)
        {
            fprintf(stderr, "fstatat %s/%s: %s\n",
                    path, dp->d_name, strerror(errno));
            error = 1;
            continue;
        }

        fprintf(out, "%s %jd\n",
                dp->d_name, (intmax_t)filestat.st_size);
    }

    if (closedir(dir) == -1)
    {
        perror("closedir");
        error = 1;
    }

    return error ? -1 : 0;
}

int main(int argc, char **argv)
{
    int c;
    const char *output_name = NULL;
    const char **paths = malloc(argc * sizeof(*paths));

    if (paths == NULL)
    {
        perror("malloc");
        return EXIT_FAILURE;
    }

    int path_count = 0;

    while ((c = getopt(argc, argv, "p:o:")) != -1)
    {
        switch (c)
        {
        case 'p':
            paths[path_count++] = optarg;
            break;

        case 'o':
            if (output_name != NULL)
            {
                free(paths);
                usage(argv[0]);
            }

            output_name = optarg;
            break;

        default:
            free(paths);
            usage(argv[0]);
        }
    }

    if (optind != argc)
    {
        free(paths);
        usage(argv[0]);
    }

    if (path_count == 0)
    {
        paths[path_count++] = ".";
    }

    FILE *out = stdout;

    if (output_name != NULL)
    {
        out = fopen(output_name, "w");

        if (out == NULL)
        {
            perror("fopen");
            free(paths);
            return EXIT_FAILURE;
        }
    }

    int error = 0;

    for (int i = 0; i < path_count; i++)
    {
        if (scan_dir(paths[i], out) == -1)
            error = 1;
    }

    if (output_name != NULL)
    {
        if (fclose(out) == EOF)
        {
            perror("fclose");
            error = 1;
        }
    }
    else
    {
        if (fflush(out) == EOF)
        {
            perror("fflush");
            error = 1;
        }
    }

    free(paths);

    return error ? EXIT_FAILURE : EXIT_SUCCESS;
}