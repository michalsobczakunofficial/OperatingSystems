#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define FILE_BUF_LEN 2048
#define LINE_LEN 64
#define MAX_TOWERS 5
#define MAX_MESSAGE_LEN 127

typedef struct
{
    const char *name;
    char message[MAX_MESSAGE_LEN + 1];
    size_t message_len;
    int too_long;
} Tower;

void fatal(const char *source)
{
    int saved_errno = errno;
    char output[256];
    int len = snprintf(output, sizeof(output), "%s: %s\n",
                       source, strerror(saved_errno));

    if (len > 0)
    {
        size_t count = (size_t)len;

        if (count >= sizeof(output))
            count = sizeof(output) - 1;

        (void)write(STDERR_FILENO, output, count);
    }

    exit(EXIT_FAILURE);
}

void write_all(int fd, const char *buffer, size_t len)
{
    size_t written = 0;

    while (written < len)
    {
        ssize_t n = write(fd, buffer + written, len - written);

        if (n == -1)
        {
            if (errno == EINTR)
                continue;

            fatal("write");
        }

        if (n == 0)
        {
            const char msg[] = "Blad zapisu\n";

            (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }

        written += (size_t)n;
    }
}

void usage(const char *program)
{
    char output[256];

    int len = snprintf(output, sizeof(output),
                       "USAGE: %s recipe tower1 [tower2 ... tower5]\n",
                       program);

    if (len > 0)
    {
        size_t count = (size_t)len;

        if (count >= sizeof(output))
            count = sizeof(output) - 1;

        write_all(STDERR_FILENO, output, count);
    }

    exit(EXIT_FAILURE);
}

void invalid_message(const char *message, size_t len)
{
    const char prefix[] = "Babcia sie zagadala: ";

    write_all(STDERR_FILENO, prefix, sizeof(prefix) - 1);
    write_all(STDERR_FILENO, message, len);
    write_all(STDERR_FILENO, "\n", 1);
}

void save_step(int recipe_fd, const char *message, size_t message_len)
{
    if (message_len < 3 ||
        message[0] < '0' || message[0] > '9' ||
        message[1] < '0' || message[1] > '9' ||
        message[2] != ':')
    {
        invalid_message(message, message_len);
        return;
    }

    int step = (message[0] - '0') * 10
             + (message[1] - '0');

    if (step > 31)
    {
        invalid_message(message, message_len);
        return;
    }

    char line[LINE_LEN];

    memset(line, ' ', LINE_LEN - 1);
    line[LINE_LEN - 1] = '\n';

    size_t content_len = message_len - 3;

    if (content_len > LINE_LEN - 1)
        content_len = LINE_LEN - 1;

    memcpy(line, message + 3, content_len);

    if (lseek(recipe_fd, (off_t)step * LINE_LEN, SEEK_SET) == (off_t)-1)
        fatal("lseek");

    write_all(recipe_fd, line, LINE_LEN);

    char output[32];

    int len = snprintf(output, sizeof(output),
                       "Krok %02d zapisany\n", step);

    write_all(STDOUT_FILENO, output, (size_t)len);
}

void process_data(int recipe_fd, Tower *tower,
                  const char *buffer, ssize_t bytes_read)
{
    for (ssize_t i = 0; i < bytes_read; i++)
    {
        char c = buffer[i];

        if (c == '\n')
        {
            if (!tower->too_long)
            {
                tower->message[tower->message_len] = '\0';

                save_step(recipe_fd,
                          tower->message,
                          tower->message_len);
            }
            else
            {
                invalid_message(tower->message,
                                tower->message_len);
            }

            tower->message_len = 0;
            tower->too_long = 0;
        }
        else if (!tower->too_long)
        {
            if (tower->message_len < MAX_MESSAGE_LEN)
            {
                tower->message[tower->message_len] = c;
                tower->message_len++;
            }
            else
            {
                tower->too_long = 1;
            }
        }
    }
}

// ETAP 4 - mapowanie przepisu i wypisywanie wynikow

void finish_recipe(int recipe_fd)
{
    char *data = mmap(NULL, FILE_BUF_LEN,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED, recipe_fd, 0);

    if (data == MAP_FAILED)
        fatal("mmap");

    const char old_word[] = "raw~berries";
    const char new_word[] = "raspberries";

    const size_t word_len = sizeof(old_word) - 1;

    size_t replacements = 0;

    // Szukamy wszystkich wystapien raw~berries

    for (size_t i = 0; i + word_len <= FILE_BUF_LEN; i++)
    {
        if (memcmp(data + i, old_word, word_len) == 0)
        {
            memcpy(data + i, new_word, word_len);

            replacements++;

            i += word_len - 1;
        }
    }

    // Zapisujemy zmiany w pliku

    if (msync(data, FILE_BUF_LEN, MS_SYNC) == -1)
        fatal("msync");

    char output[128];

    int len = snprintf(output, sizeof(output),
                       "Brudy ksiezniczki Joanny wyczyszczone: %zu\n",
                       replacements);

    write_all(STDOUT_FILENO, output, (size_t)len);

    // Wypisujemy wszystkie niepuste kroki

    int empty_steps = 0;

    for (int step = 0; step < 32; step++)
    {
        const char *line = data + step * LINE_LEN;

        size_t content_len = LINE_LEN - 1;

        // Usuwamy spacje tylko z konca linii

        while (content_len > 0 && line[content_len - 1] == ' ')
        {
            content_len--;
        }

        // Sprawdzamy, czy linia jest pusta

        if (content_len == 0)
        {
            empty_steps++;
            continue;
        }

        // Wypisujemy numer kroku w formacie NN:

        len = snprintf(output, sizeof(output), "%02d: ", step);

        write_all(STDOUT_FILENO, output, (size_t)len);

        // Wypisujemy tresc bez koncowych spacji

        write_all(STDOUT_FILENO, line, content_len);

        write_all(STDOUT_FILENO, "\n", 1);
    }

    // Sprawdzamy, czy wszystkie kroki sa zapisane

    if (empty_steps == 0)
    {
        const char msg[] = "Tort gotowy!\n";

        write_all(STDOUT_FILENO, msg, sizeof(msg) - 1);
    }
    else
    {
        len = snprintf(output, sizeof(output),
                       "Brakuje %d krokow, tort sie zapadnie!\n",
                       empty_steps);

        write_all(STDOUT_FILENO, output, (size_t)len);
    }

    // Usuwamy mapowanie pamieci

    if (munmap(data, FILE_BUF_LEN) == -1)
        fatal("munmap");
}

int main(int argc, char **argv)
{
    if (argc < 3 || argc > 2 + MAX_TOWERS)
    {
        usage(argv[0]);
    }

    const char *recipe = argv[1];

    // ETAP 1 - otwarcie lub utworzenie przepisu

    int recipe_fd = open(recipe, O_RDWR);

    if (recipe_fd == -1)
    {
        if (errno != ENOENT)
        {
            fatal("open recipe");
        }

        recipe_fd = open(recipe, O_RDWR | O_CREAT | O_EXCL, 0644);

        if (recipe_fd == -1)
        {
            fatal("create recipe");
        }

        char empty_line[LINE_LEN];

        memset(empty_line, ' ', LINE_LEN - 1);
        empty_line[LINE_LEN - 1] = '\n';

        for (int i = 0; i < 32; i++)
        {
            write_all(recipe_fd, empty_line, LINE_LEN);
        }
    }

    // Sprawdzamy rozmiar pliku

    if (lseek(recipe_fd, 0, SEEK_END) != FILE_BUF_LEN)
    {
        const char msg[] = "Ta ksiega to nie przepis!\n";

        write_all(STDERR_FILENO, msg, sizeof(msg) - 1);

        close(recipe_fd);

        return EXIT_FAILURE;
    }

    // ETAP 3 - obsluga wielu wiez

    int tower_count = argc - 2;
    int active = tower_count;

    struct pollfd pfds[MAX_TOWERS];

    Tower towers[MAX_TOWERS] = {0};

    // Sprawdzamy, czy wszystkie wieze sa FIFO

    for (int i = 0; i < tower_count; i++)
    {
        towers[i].name = argv[i + 2];

        struct stat st;

        if (stat(towers[i].name, &st) == -1 ||
            !S_ISFIFO(st.st_mode))
        {
            const char prefix[] = "Nie ma babci na wiezy ";

            write_all(STDERR_FILENO,
                      prefix,
                      sizeof(prefix) - 1);

            write_all(STDERR_FILENO,
                      towers[i].name,
                      strlen(towers[i].name));

            write_all(STDERR_FILENO, "\n", 1);

            close(recipe_fd);

            return EXIT_FAILURE;
        }
    }

    // Otwieramy wszystkie lacza FIFO

    for (int i = 0; i < tower_count; i++)
    {
        pfds[i].fd = open(towers[i].name, O_RDONLY);

        if (pfds[i].fd == -1)
        {
            fatal("open FIFO");
        }

        pfds[i].events = POLLIN;
        pfds[i].revents = 0;
    }

    // ETAP 2 i 3 - odbieranie wiadomosci z wielu laczy

    while (active > 0)
    {
        int ready = poll(pfds, (nfds_t)tower_count, 3000);

        if (ready == -1)
        {
            if (errno == EINTR)
                continue;

            fatal("poll");
        }

        // Przez 3 sekundy nie bylo zdarzen

        if (ready == 0)
        {
            const char msg[] = "Babcie dolewaja herbatki...\n";

            write_all(STDOUT_FILENO,
                      msg,
                      sizeof(msg) - 1);

            continue;
        }

        // Sprawdzamy wszystkie lacza

        for (int i = 0; i < tower_count; i++)
        {
            if (pfds[i].fd == -1 || pfds[i].revents == 0)
            {
                continue;
            }

            if (pfds[i].revents & POLLNVAL)
            {
                const char msg[] = "Nieprawidlowy deskryptor FIFO\n";

                write_all(STDERR_FILENO,
                          msg,
                          sizeof(msg) - 1);

                exit(EXIT_FAILURE);
            }

            if (pfds[i].revents & (POLLIN | POLLHUP | POLLERR))
            {
                char buffer[128];

                ssize_t bytes_read = read(pfds[i].fd,
                                          buffer,
                                          sizeof(buffer));

                if (bytes_read == -1)
                {
                    if (errno == EINTR)
                        continue;

                    fatal("read FIFO");
                }

                if (bytes_read > 0)
                {
                    // Kazda babcia ma wlasny bufor wiadomosci

                    process_data(recipe_fd,
                                 &towers[i],
                                 buffer,
                                 bytes_read);
                }
                else
                {
                    // EOF - babcia skonczyla dyktowac

                    if (close(pfds[i].fd) == -1)
                    {
                        fatal("close FIFO");
                    }

                    pfds[i].fd = -1;

                    active--;

                    const char prefix[] = "Babcia z wiezy ";
                    const char suffix[] = " skonczyla dyktowac\n";

                    write_all(STDOUT_FILENO,
                              prefix,
                              sizeof(prefix) - 1);

                    write_all(STDOUT_FILENO,
                              towers[i].name,
                              strlen(towers[i].name));

                    write_all(STDOUT_FILENO,
                              suffix,
                              sizeof(suffix) - 1);
                }
            }
        }
    }

    // ETAP 4 - mapowanie przepisu i wypisywanie wynikow

    finish_recipe(recipe_fd);

    if (close(recipe_fd) == -1)
    {
        fatal("close recipe");
    }

    return EXIT_SUCCESS;
}