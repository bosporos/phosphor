//
// project phosphor-mmc
// author Maximilien M. Cura
//

#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define __mmc_max(x, y) (((x) > (y)) ? (x) : (y))
#define __mmc_min(x, y) (((x) < (y)) ? (x) : (y))

#define __MMC_CHUNKSIZE 0x4000

#define __MMC_SETTING_ANNOUNCE_DIRECTORIES 1
#define __MMC_SETTING_COLORIZE_OUTPUT 2

int __mmc_settings = 0;

size_t __mmc_nlines (const char * path)
{
    int fd = open (path, O_RDONLY);
    if (!fd)
        return 0 * fprintf (stderr, "warning: failed to open file: %s\n", path);

    char chunk[__MMC_CHUNKSIZE];
    char *read_buffer = (char *)chunk, *cp;
    ssize_t bytes_read;

    size_t lines = 0, i;
    while ((bytes_read = read (fd, read_buffer, __MMC_CHUNKSIZE)) > 0) {
        cp = &chunk[0];
        for (i = 0; i < bytes_read; i++)
            if (0x0a == *cp++)
                lines++;
    }

    close (fd);

    return lines;
}

static float __mmc_left[][3] = {
    { 0x3a, 0xb7, 0x95 },
    { 0x22, 0x74, 0xa5 },
    { 0xff, 0x43, 0x65 },
};
static float __mmc_right[][3] = {
    { 0x22, 0x74, 0xa5 },
    { 0xff, 0x43, 0x65 },
    { 0xff, 0xb2, 0x0f },
};

void __mmc_colorize (char ** string, size_t lines)
{
    float *left, *right, lerp, color[3];
    if (lines < 392) {
        lerp  = (float)lines / 392.0f;
        left  = &__mmc_left[0][0];
        right = &__mmc_right[0][0];
    } else if (lines < 1024) {
        lerp  = ((float)lines - 392.0f) / 632.0f;
        left  = &__mmc_left[1][0];
        right = &__mmc_right[1][0];
    } else {
        left  = &__mmc_right[2][0];
        right = &__mmc_right[2][0];
        lerp  = 1.0f;
    }
    color[0]         = left[0] + lerp * (right[0] - left[0]);
    color[1]         = left[1] + lerp * (right[1] - left[1]);
    color[2]         = left[2] + lerp * (right[2] - left[2]);
    int rgb_color[3] = { color[0], color[1], color[2] };
    asprintf (string, "\x1b[38;2;%i;%i;%im", rgb_color[0], rgb_color[1], rgb_color[2]);
}

int __mmc_scan_trees (char * const * paths)
{
    FTS * ftsp;
    FTSENT *entity, *chp;
    int fts_options = FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR;
    int rval        = 0;

    if ((ftsp = fts_open (paths, fts_options, NULL)) == NULL) {
        fprintf (stderr, "error: could not open file hierarchy; fts_open call failed with exit code %i", errno);
        return -1;
    }
    /* Initialize ftsp with as many argv[] parts as possible. */
    chp = fts_children (ftsp, 0);
    if (chp == NULL) {
        return 0;
    }

    enum {
        IGN_GIT,
        IGN_BUILD,
        IGN_NONE
    } ignore_mode
        = IGN_NONE;
    char first_leave = 0;

#define IGNORE_ENTER(dirname, dirlen, ign_symb)                                          \
    if (!strncmp (entity->fts_name, dirname, __mmc_max (dirlen, entity->fts_namelen))) { \
        ignore_mode = ign_symb;                                                          \
        first_leave = 1;                                                                 \
    }
#define IGNORE_LEAVE(dirname, dirlen, ign_symb)                                                                     \
    if (ignore_mode == ign_symb && !strncmp (entity->fts_name, dirname, __mmc_max (dirlen, entity->fts_namelen))) { \
        if (!first_leave) {                                                                                         \
            ignore_mode = ign_symb;                                                                                 \
        }                                                                                                           \
    }

    size_t lines_total = 0;

    char * color = NULL;
    while ((entity = fts_read (ftsp)) != NULL) {
        switch (entity->fts_info) {
            case FTS_D:
                if (ignore_mode == IGN_NONE && (__mmc_settings & __MMC_SETTING_ANNOUNCE_DIRECTORIES)) {
                    printf ("======> Scanning directory %s...\n", entity->fts_path);
                }
                first_leave = 1;
                IGNORE_ENTER (".git", 4, IGN_GIT)
                IGNORE_ENTER ("build", 5, IGN_BUILD)
            case FTS_DP:
                if (first_leave)
                    first_leave = 0;
                else if (ignore_mode == IGN_NONE && (__mmc_settings & __MMC_SETTING_ANNOUNCE_DIRECTORIES)) {
                    printf ("------> Leaving directory %s...\n", entity->fts_path);
                }
                IGNORE_LEAVE (".git", 4, IGN_GIT)
                IGNORE_LEAVE ("build", 5, IGN_BUILD)
                break;
            case FTS_F:
                if (ignore_mode == IGN_NONE) {
                    unsigned long lines_file = __mmc_nlines (entity->fts_path);
                    if (__mmc_settings & __MMC_SETTING_COLORIZE_OUTPUT) {
                        __mmc_colorize (&color, lines_file);
                        printf (" -- %5zi %s%s\x1b[0m\n", lines_file, color, entity->fts_path);
                        free (color);
                    } else {
                        printf (" -- %5zi %s\n", lines_file, entity->fts_path);
                    }
                    lines_total += lines_file;
                }
                break;
        }
    }
    fts_close (ftsp);

    printf ("\n");
    if (__mmc_settings & __MMC_SETTING_COLORIZE_OUTPUT)
        printf ("\x1b[33m ==[==]==>\x1b[0m Total: %zi\n", lines_total);
    else
        printf (" ==[==]==> Total: %zi\n", lines_total);
    printf ("\n");

    return 0;
}

int __mmc_try_process_arg (const char * arg)
{
    if (!strcmp (arg, "-d")) {
        __mmc_settings |= __MMC_SETTING_ANNOUNCE_DIRECTORIES;
        return 1;
    }
    if (!strcmp (arg, "-c")) {
        __mmc_settings |= __MMC_SETTING_COLORIZE_OUTPUT;
        return 1;
    }
    return 0;
}

int main (int argc, char * const * argv)
{
    int argc_offset = 1;
    if (argc > 1) {
        if (__mmc_try_process_arg (argv[argc_offset])) {
            ++argc_offset;
            if (argc > 2)
                if (__mmc_try_process_arg (argv[argc_offset]))
                    ++argc_offset;
        }
    }
    if ((argc - argc_offset) <= 0) {
        printf ("Usage: %s [-d] [-c] directories...\n", argv[0]);
        printf ("\n");
        printf ("Options:\n");
        printf ("\t-d\tannounce directories\n");
        printf ("\t-c\tuse 24-bit color\n");
    } else {
        __mmc_scan_trees (argv + argc_offset);
    }
    return 0;
}
