/*

MIT License

Copyright (c) 2020 Mika Tuupola

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

-cut-

SPDX-License-Identifier: MIT

*/

#include <stdio.h>
#include <gd.h>
#include <getopt.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>

#include <fontx2.h>

void usage(void)
{
    fprintf(stderr, "\n");
    fprintf(stderr, "Dump a FONTX2 file as a PNG image\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage\n");
    fprintf(stderr, "    $ fontx2png <options> [filename]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options\n");
    fprintf(stderr, "    -i <file>, --input <file>      FONTX2 file to dump.\n");
    fprintf(stderr, "    -o <file>, --output <file>     PNG filename to output.\n");
    fprintf(stderr, "    -w <width>, --width <width>    Image width in pixels.\n");
    fprintf(stderr, "    -h <height>, --height <height> Image height in pixels.\n");
    fprintf(stderr, "    -v, --verbose\n\n");
    fprintf(stderr, "    --help\n");
    fprintf(stderr, "Examples\n");
    fprintf(stderr, "    $ fontx2png 8x8.fnt\n");
    fprintf(stderr, "    $ fontx2png --verbose 8x8.fnt\n");
    fprintf(stderr, "    $ fontx2png --input 8x8.fnt\n");

    exit(1);
}

int main(int argc, char **argv)
{
    char *input = NULL;
    static gdImagePtr img;
    FILE *png;
    int32_t color;

    static int32_t verbose;
    int32_t width, height;
    int32_t spacing = 0;
    wchar_t buffer[255];
    uint16_t max_glyph;
    uint16_t out_glyph = 0;
    uint8_t status;

    fontx2_meta_t meta;
    fontx2_glyph_t glyph;
    size_t fsize;
    FILE *f;
    uint8_t *font;

    setlocale(LC_ALL, "");

    while (1) {
        static struct option options[] = {
            {"verbose", no_argument, &verbose, 1},
            {"width", required_argument, 0, 'w'},
            {"height", required_argument, 0, 'h'},
            {"spacing", required_argument, 0, 's'},
            {"input", required_argument, 0, 'i'},
            {0, 0, 0, 0}
        };

        int32_t index = 0;
        char c;

        c = getopt_long(argc, argv, "w:h:i:", options, &index);

        /* Detect the end of the options. */
        if (c == -1) {
            break;
        }

        switch (c) {
        case 0:
            /* Flags are being set, */
            break;
        case 's':
            spacing = atoi(optarg);
            break;
        case 'w':
            width = atoi(optarg);
            break;
        case 'h':
            height = atoi(optarg);
            break;
        case 'i':
            input = optarg;
            break;
        default:
            usage();
            exit(0);
            break;
        }
    }

    if (optind < argc) {
        while (optind < argc) {
            if (input) {
                usage();
            }
            input = argv[optind++];
        }
    }

    if (verbose) {
        puts("verbose flag is set");
    };

    if (NULL == input) {
        usage();
        exit(1);
    }

    f = fopen(input, "rb");
    if (NULL == f) {
        printf("Unable to open file \"%s\".\n", input);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    font = malloc(fsize + 1);

    fread(font, 1, fsize, f);
    fclose(f);
    status = fontx2_meta(&meta, font);

    max_glyph = 0xffff;
    if (FONTX2_TYPE_SBCS == meta.type) {
        max_glyph = 0xff;
    }

    int32_t x0 = 6;
    int32_t y0 = 6;

    /* Default width is 320. */
    if (0 == width) {
        width = 320;
    }

    printf("%s (%dx%d) 0 - %d\n", meta.name, meta.width, meta.height, max_glyph);

    /* Calculate height automatically. */
    if (0 == height) {
        for (uint16_t c = 1; c < max_glyph; c++) {
            uint8_t status = fontx2_glyph(&glyph, c, font);
            if (FONTX2_OK == status) {
                x0 += glyph.width + spacing;
                if (x0 > width - glyph.width - spacing) {
                    x0 = 6;
                    y0 += glyph.height + spacing;
                }
            } else {
                //printf("Could not get glyph %d", c);
            }
        }

        height = y0 + glyph.height + 6 + spacing;
    }
    printf("%dx%d %s\n", width, height, input);

    printf("%s (%dx%d) 0 - %d\n", meta.name, meta.width, meta.height, max_glyph);

    img = gdImageCreateTrueColor(width, height);
    color = gdTrueColorAlpha(255, 255, 255, 0);

    x0 = 6;
    y0 = 6;

    for (uint16_t c = 1; c < max_glyph; c++) {
        uint8_t status = fontx2_glyph(&glyph, c, font);
        if (FONTX2_OK == status) {
            out_glyph++;
            wprintf(L"%lc ", c);
            printf("%d (0x%x)\n", c, c);
            if (verbose) {
                printf("%p %p %p\n", font, glyph.buffer, (void *)(glyph.buffer - font));
            }
            for (uint8_t y = 0; y < glyph.height; y++) {
                for (uint8_t x = 0; x < glyph.width; x++) {
                    uint8_t set = *(glyph.buffer + x / 8) & (0x80 >> (x % 8));
                    if (set) {
                        gdImageSetPixel(img, x0 + x, y0 + y, color);
                    }
                }
                glyph.buffer += glyph.pitch;
            }

            x0 += glyph.width + spacing;
            if (x0 > width - glyph.width - spacing) {
                x0 = 6;
                y0 += glyph.height + spacing;
            }
        }
    }

    printf("\nTotal %d glyphs\n", out_glyph);

    char *output = NULL;
    char *ext = input;

    ext = strrchr(input, '.');
    strcpy(ext, ".png");

    output = strrchr(input, '/');
    if (output) {
        ++output;
    } else {
        output = input;
    }

    // printf("%s\n", output);
    // exit(0);

    png = fopen(output, "wb");
    if (NULL == png) {
        printf("Cannot open file \"%s\"", output);
    }
    gdImagePng(img, png);
    fclose(png);

    exit(0);
}
