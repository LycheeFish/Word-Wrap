#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/wait.h>

#ifndef DEBUG
#define DEBUG 0
#endif

#define BUFSIZE 32
#define LISTLEN 16

char **lines;
int line_count, line_array_size;

char *crnt_line;
int crnt_line_len; // does not include null terminator

void init_lines(void)
{
    lines = malloc(sizeof(char *) * LISTLEN);
    line_count = 0;
    line_array_size = LISTLEN;
}

// adds a word to crnt_line.
// wordlen does not include null terminator.
// if it's not the first word of a crnt_line, then add a space to crnt_line.
void add_word(char *word, int wordlen)
{
    if (DEBUG)
        printf("Adding word: %s (len %d)\n", word, wordlen);

    if (wordlen == 0)
        return;

    if (crnt_line == NULL)
    {
        crnt_line = malloc(wordlen + 1);
        crnt_line = memcpy(crnt_line, word, wordlen);
        crnt_line_len = wordlen;
    }
    else
    {
        crnt_line = realloc(crnt_line, crnt_line_len + wordlen + 1 + 1);
        crnt_line[crnt_line_len] = ' ';
        memcpy(&crnt_line[crnt_line_len + 1], word, wordlen);
        crnt_line_len += wordlen + 1;
    }
    crnt_line[crnt_line_len] = '\0';
}

// adds crnt_line to lines.
void add_crnt_line()
{
    if (crnt_line == NULL)
        return;

    if (DEBUG)
    {
        printf("Adding line: ");
        puts(crnt_line);
    }

    if (line_count == line_array_size)
    {
        line_array_size *= 2;
        lines = realloc(lines, line_array_size * sizeof(char *));
    }

    lines[line_count] = malloc(crnt_line_len + 1);
    memcpy(lines[line_count], crnt_line, crnt_line_len);
    lines[line_count][crnt_line_len] = '\0';
    line_count++;
}

void add_empty_line()
{
    if (DEBUG)
        printf("Adding empty line\n");

    if (line_count == line_array_size)
    {
        line_array_size *= 2;
        lines = realloc(lines, line_array_size * sizeof(char *));
    }

    lines[line_count] = malloc(1);
    lines[line_count][0] = '\0';
    line_count++;
}

int main(int argc, char **argv)
{
    if (DEBUG)
        printf("Started program!\n");

    int fd, bytes;
    char buf[BUFSIZE];
    char *crnt; // current word
    int len, crntlen;
    int pos, start;
    int writeInsteadOfPrinting = 0;

    init_lines();

    if (argc < 2)
    {
        printf("Not enough arguments\n");
        return 1;
    }

    int WIDTH = atoi(argv[1]);
    if (DEBUG)
        printf("WIDTH: %d\n", WIDTH);

    if (argc == 2)
    {
        fd = 0;
    }
    else if (argc >= 3)
    {
        fd = open(argv[2], O_RDONLY);
        if (fd == -1)
        {
            perror(argv[2]);
            return EXIT_FAILURE;
        }
        if (argc == 4)
        {
            // we know this was done from a execlp,
            // so we should write to a file instead of printing
            if (DEBUG)
                printf("This is a child process!\n");

            writeInsteadOfPrinting = 1;
        }

        struct stat statbuf;
        stat(argv[2], &statbuf);
        if (S_ISDIR(statbuf.st_mode))
        {
            if (DEBUG)
                printf("%s is a directory!\n", argv[2]);

            DIR *dp;
            struct dirent *de;
            dp = opendir(argv[2]);
            de = readdir(dp);
            while (de != NULL)
            {
                stat(de->d_name, &statbuf);
                if (de->d_name[0] == '.' || !strncmp(de->d_name, "wrap.", 5))
                {
                    de = readdir(dp);
                    continue;
                }

                if (S_ISDIR(statbuf.st_mode))
                {
                    if (DEBUG)
                    {
                        printf("Subdirectory: ");
                        puts(de->d_name);
                    }
                }
                else if (S_ISREG(statbuf.st_mode))
                {
                    if (DEBUG)
                    {
                        printf("File: ");
                        puts(de->d_name);
                    }
                }

                int pid = fork();
                if (pid == 0)
                {
                    char path[64] = ""; // assume 64...
                    strcat(path, argv[2]);
                    strcat(path, "/");
                    strcat(path, de->d_name);

                    /*
                    char directoryfd[64] = "";
                    sprintf(directoryfd, "%d", dirfd(dp));
                    */

                    if (DEBUG)
                        printf("About to exec path: %s\n", path);

                    execlp(argv[0], argv[0], argv[1], path, "dummy", NULL);
                }
                else
                {
                    int status;
                    wait(&status);
                }
                de = readdir(dp);
            }
            closedir(dp);
            return 0;
        }
    }

    crnt = NULL;
    crntlen = 0;
    int nlcount = 0; // if 2 are detected in a row, print empty line
    int spacecount = 0;
    int wordExceededWidth = 0;
    while ((bytes = read(fd, buf, BUFSIZE)) > 0)
    {
        // read buffer and break file into lines
        if (DEBUG)
        {
            printf("Read %d bytes\n", bytes);
            printf("Leftover word: %s\n", crnt);
        }

        start = 0;
        for (pos = 0; pos < bytes; pos++)
        {
            // printf("nlcount: %d\n", nlcount);
            if (DEBUG)
                printf("character at pos %d: %c\n", pos, buf[pos]);

            if ((buf[pos] == ' ' || buf[pos] == '\n') && spacecount == 0)
            {
                if (buf[pos] == ' ')
                {
                    spacecount++;
                    nlcount = 0;
                }
                else if (buf[pos] == '\n')
                {
                    nlcount++;
                    if (nlcount == 2)
                    {
                        // push whatever line you already have first
                        add_crnt_line();
                        free(crnt_line);
                        crnt_line = NULL;
                        crnt_line_len = 0;
                        add_empty_line();
                    }
                    spacecount = 0;
                }

                len = pos - start;
                crnt = realloc(crnt, len + crntlen + 1);
                memcpy(&crnt[crntlen], &buf[start], len);

                crntlen += len;
                crnt[crntlen] = '\0';

                if (WIDTH - crnt_line_len <= crntlen)
                {
                    // not enough space; make new line
                    add_crnt_line();
                    free(crnt_line);
                    crnt_line = NULL;
                    crnt_line_len = 0;
                }

                add_word(crnt, crntlen);

                if (crntlen > WIDTH)
                {
                    if (DEBUG)
                        printf("There is a word that is bigger than the width!\n");

                    wordExceededWidth = 1;
                }

                free(crnt);
                crnt = NULL;
                crntlen = 0;
                start = pos + 1;

                if (buf[pos] == '\n' && fd == 0)
                {
                    goto DONE;
                }
            }
            else if (buf[pos] == ' ' && spacecount > 0)
            {
                start = pos + 1;
                spacecount++;
            }
            else // regular character
            {
                nlcount = 0;
                spacecount = 0;
            }
        }

        // save any partial line at the end of the buffer
        if (start < pos)
        {
            if (DEBUG)
                printf("Segment %d:%d saved\n", start, pos);
            len = pos - start;
            crnt = realloc(crnt, len + crntlen + 1);
            memcpy(&crnt[crntlen], &buf[start], len);

            crntlen += len;
            crnt[crntlen] = '\0';
        }
    }

    if (bytes == -1)
    {
        perror("read");
        return EXIT_FAILURE;
    }

DONE:

    if (crnt_line)
    {
        add_crnt_line();
    }

    if (writeInsteadOfPrinting)
    {
        int lastslashindex;
        for (lastslashindex = strlen(argv[2]) - 1; lastslashindex >= 0; lastslashindex--)
        {
            if (argv[2][lastslashindex] == '/')
            {
                lastslashindex++;
                break;
            }
        }
        char writefile[64] = ""; // assume 64...
        strncat(writefile, argv[2], lastslashindex);
        strcat(writefile, "wrap.");
        strcat(writefile, &argv[2][lastslashindex]);
        if (DEBUG)
            printf("OPENING: %s\n", writefile);

        int writefd = open(writefile, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR | S_IWGRP | S_IWOTH);
        if (writefd == -1)
        {
            perror(argv[2]);
            return EXIT_FAILURE;
        }

        int writebytes;
        for (int i = 0; i < line_count; i++)
        {
            writebytes = write(writefd, lines[i], strlen(lines[i]));
            if (writebytes == -1)
            {
                perror("write");
                return EXIT_FAILURE;
            }
            write(writefd, "\n", 1);
        }
        close(writefd);
    }
    else
    {
        for (pos = 0; pos < line_count; pos++)
        {
            puts(lines[pos]);
        }
    }

    for (int i = 0; i < line_count; i++)
    {
        free(lines[i]);
    }
    free(lines);
    free(crnt_line);
    free(crnt);

    close(fd);

    if (wordExceededWidth)
    {
        printf("EXIT FAILURE: A word exceeded the width\n");
        return EXIT_FAILURE;
    }

    return 0;
}
