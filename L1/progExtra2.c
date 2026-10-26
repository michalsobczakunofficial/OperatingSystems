#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(char *pname)
{
    fprintf(stderr, "USAGE:%s -p Directory -e Extension -d Depth -o\n", pname);
    exit(EXIT_FAILURE);
}

int scan_dir(const char *path, FILE *out, const char *extension, int depth)
{
    DIR *dirp;
    struct dirent *dp;
    struct stat filestat;
    int error = 0;
    char fullpath[PATH_MAX];

    if ((dirp = opendir(path)) == NULL)
    {
        perror(path);
        return -1;
    }

    fprintf(out, "path: %s\n", path);

    do
    {
        errno = 0;

        if ((dp = readdir(dirp)) != NULL)
        {
            if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
                continue;

            if (snprintf(fullpath, sizeof(fullpath), "%s/%s", path, dp->d_name) >= sizeof(fullpath))
            {
                fprintf(stderr, "Path too long\n");
                error = 1;
                continue;
            }

            if (lstat(fullpath, &filestat) == -1)
            {
                perror("lstat");
                error = 1;
                continue;
            }

            if (S_ISDIR(filestat.st_mode))
            {
                if (depth > 1)
                {
                    if (scan_dir(fullpath, out, extension, depth - 1) == -1)
                        error = 1;
                }
            }
            else if (S_ISREG(filestat.st_mode))
            {
                if (extension == NULL)
                {
                    fprintf(out, "%s %ld\n", dp->d_name, (long)filestat.st_size);
                }
                else
                {
                    char *dot = strrchr(dp->d_name, '.');

                    if (dot != NULL && strcmp(dot + 1, extension) == 0)
                    {
                        fprintf(out, "%s %ld\n", dp->d_name, (long)filestat.st_size);
                    }
                }
            }
        }

    } while (dp != NULL);

    if (errno != 0)
    {
        perror("readdir");
        error = 1;
    }

    if (closedir(dirp))
    {
        perror("closedir");
        error = 1;
    }

    return error ? -1 : 0;
}

int main(int argc, char **argv)
{
    int c;
    const char *extension_name = NULL;
    const char *output_name = NULL;
    const char **paths = malloc(argc * sizeof(*paths));

    if (paths == NULL)
        ERR("malloc");

    int depth = 1;
    int path_count = 0;
    int output_flag = 0;

    while ((c = getopt(argc, argv, "p:e:d:o")) != -1)
    {
        switch (c)
        {
        case 'p':
            paths[path_count++] = optarg;
            break;

        case 'o':
            if (output_flag)
            {
                free(paths);
                usage(argv[0]);
            }

            output_flag = 1;
            output_name = getenv("L1_OUTPUTFILE");
            break;

        case 'e':
            if (extension_name != NULL)
            {
                free(paths);
                usage(argv[0]);
            }

            extension_name = optarg;
            break;

        case 'd':
            depth = atoi(optarg);

            if (depth < 1)
            {
                free(paths);
                usage(argv[0]);
            }
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

    if (output_flag && output_name != NULL)
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
        if (scan_dir(paths[i], out, extension_name, depth) == -1)
            error = 1;
    }

    if (out != stdout)
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