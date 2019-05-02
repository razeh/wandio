/*
 *
 * Copyright (c) 2007-2019 The University of Waikato, Hamilton, New Zealand.
 * All rights reserved.
 *
 * This file is part of libwandio.
 *
 * This code has been developed by the University of Waikato WAND
 * research group. For further information please see http://www.wand.net.nz/
 *
 * libwandio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * libwandio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include "wandio.h"

struct user_t {
        io_t *parent;
};

#define DATA(io) ((struct user_t *)((io)->data))

static int64_t user_read(io_t *io, void *buffer, int64_t len)
{
        int return_value = wandio_read(DATA(io)->parent, buffer, len);
        char *p = (char*)buffer;
        /* tweak the data a bit so we can be sure that user_read was invoked. */
        for(int i = 0; i < len; i++) {
                p[i]--;
        }
        return return_value;
}

static int user_pre_init(__attribute__((unused))io_t *parent, const char *filename)
{
        const char *USER_SUFFIX = ".user";
        /* Check for a .user extension. */
        size_t len = strlen(filename);
        if (len >= strlen(USER_SUFFIX)) {
                const char *ptr = filename + len - strlen(USER_SUFFIX);
                if (strcmp(ptr, USER_SUFFIX) == 0) {
                        return 1;
                }
        }
        return 0;
}

static void user_close(io_t *io)
{
        wandio_destroy(DATA(io)->parent);
        free(io->data);
        free(io);
}

static io_t* user_open(io_t *parent, const char *filename);

static io_source_t user_test_source = {
    "user",                     // name
    NULL,
    user_read,                  // read
    NULL,                       // peek
    NULL,                       // tell
    NULL,                       // seek
    user_close,                 // close
    user_pre_init,              // pre_init
    user_open,                  // open
};

#define protocol_prefix "user://"

static io_t* user_open(io_t *parent, __attribute__((unused))const char *filename)
{
	// If parent is null, this is the protocol test case.
        assert(parent != NULL);
        io_t *io = malloc(sizeof(io_t));
        io->data = malloc(sizeof(struct user_t));
        io->source= &user_test_source;
        DATA(io)->parent = parent;
        return io;
}

static io_t* protocol_open(__attribute__((unused))io_t *parent, const char *filename)
{
	const char *p = strstr(filename, protocol_prefix);
	if (p && *p) {
		return stdio_open(filename + strlen(protocol_prefix));
	}
	return NULL;
}

static int protocol_preinit(__attribute__((unused))io_t *parent, const char *filename)
{
	const char *p = strstr(filename, protocol_prefix);
	if (p && *p) {
		return 1;
	}
	return 0;
	
}

static io_source_t protocol_test_source = {
    "protocol_test",            // name
    protocol_prefix,            // protocol
    NULL,                       // read
    NULL,                       // peek
    NULL,                       // tell
    NULL,                       // seek
    NULL,                       // close
    protocol_preinit,           // pre_init
    protocol_open,              // open
};

static void printhelp() {
        printf("wandiocat: concatenate files into a single compressed file\n");
        printf("\n");
        printf("Available options:\n\n");
        printf(" -z <level>\n");
        printf("    Sets a compression level for the output file, must be \n");
        printf("    between 0 (uncompressed) and 9 (max compression)\n");
        printf("    Default is 0.\n");
        printf(" -Z <method>\n");
        printf("    Set the compression method. Must be one of 'gzip', \n");
        printf("    'bzip2', 'lzo', 'lzma', 'zstd' or 'lz4'. If not specified, "
               "no\n");
        printf("    compression is performed.\n");
        printf(" -o <file>\n");
        printf("    The name of the output file. If not specified, output\n");
        printf("    is written to standard output.\n");
        printf(" -u\n");
        printf("    Enable user extension test.\n");
}

int main(int argc, char *argv[]) {
        int compress_level = 0;
        int compress_type = WANDIO_COMPRESS_NONE;
        char *output = "-";
        int c;
        char *buffer = NULL;
        int register_user_io_source = 0;

        while ((c = getopt(argc, argv, "Z:z:o:hu")) != -1) {
                switch (c) {
                case 'Z': {
                        struct wandio_compression_type *compression_type =
                            wandio_lookup_compression_type(optarg);
                        if (compression_type == 0) {
                                fprintf(
                                    stderr,
                                    "Unable to lookup compression type: '%s'\n",
                                    optarg);
                                return -1;
                        }
                        compress_type = compression_type->compress_type;

                } break;
                case 'z':
                        compress_level = atoi(optarg);
                        break;
                case 'o':
                        output = optarg;
                        break;
                case 'h':
                        printhelp();
                        return 0;
                case 'u':
                        register_user_io_source = 1;
                        break;
                case '?':
                        if (optopt == 'Z' || optopt == 'z' || optopt == 'o')
                                fprintf(stderr,
                                        "Option -%c requires an argument.\n",
                                        optopt);
                        else if (isprint(optopt))
                                fprintf(stderr, "Unknown option `-%c'.\n",
                                        optopt);
                        else
                                fprintf(stderr,
                                        "Unknown option character `\\x%x'.\n",
                                        optopt);
                        return 1;
                default:
                        abort();
                }
        }

        if (register_user_io_source) {
                if(wandio_register_io_source(&user_test_source)) {
                        fprintf(stderr,
                                "Unable to create user io source\n");
                        abort();
                }
		if(wandio_register_io_source(&protocol_test_source)) {
                        fprintf(stderr,
                                "Unable to create user protocol source\n");
                        abort();
		}
        }

        iow_t *iow = wandio_wcreate(output, compress_type, compress_level, 0);
        /* stdout */
        int i;

#if _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600
        if (posix_memalign((void **)&buffer, 4096, WANDIO_BUFFER_SIZE) != 0) {
                fprintf(stderr,
                        "Unable to allocate aligned buffer for wandiocat: %s\n",
                        strerror(errno));
                abort();
        }
#else
        buffer = malloc(WANDIO_BUFFER_SIZE);
#endif

        for (i = optind; i < argc; ++i) {
                io_t *ior = wandio_create(argv[i]);
                if (!ior) {
                        fprintf(stderr, "Failed to open %s\n", argv[i]);
                        continue;
                }

                int64_t len;
                do {
                        len = wandio_read(ior, buffer, WANDIO_BUFFER_SIZE);
                        if (len > 0)
                                wandio_wwrite(iow, buffer, len);
                } while (len > 0);

                wandio_destroy(ior);
        }
        free(buffer);
        wandio_wdestroy(iow);
        return 0;
}
