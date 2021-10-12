/*
 *  bdf2fontx - convert X11 BDF font file into DOS/V $fontx2 format
 *
 *  Copyright (C) 1999-2001 by Dai ISHIJIMA
 *  All rights reserved.
 *
 *	0.0: Sep.  2, 1999 by Dai ISHIJIMA
 *	0.1: Dec.  1, 2001
 */

#define HAVE_STRNCASECMP
#ifdef __TURBOC__
#define MSDOS
#endif
#ifdef _MSC_VER
#define MSDOS
#endif
#ifdef LSI_C
#define MSDOS
#define HAVE_FSETBIN
#undef  HAVE_STRNCASECMP
#endif
#include <wchar.h>
#include <locale.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifndef NO_STDLIB
#include <stdlib.h>
#endif

#define EOS '\0'

#ifdef MSDOS
#include <fcntl.h>
#include <io.h>
#endif
#ifndef HAVE_FSETBIN
#define fsetbin(fp) setmode(fileno(fp), O_BINARY)
#endif
#ifndef HAVE_STRNCASECMP
#define strncasecmp strncmpi
#endif

#ifdef MSDOS
#define TEMPDEFAULT "."
#else
#define TEMPDEFAULT "/tmp"
#endif

#define SBCS 0
#define DBCS 1
#define ID_LEN 6
#define NAM_LEN 8

#define NPROP 18
#define DESCENT 2

#define CHARS_SBCS 256


/* $fontx2 file header information */
typedef struct {
    char id[ID_LEN];
    char name[NAM_LEN];
    unsigned char width;
    unsigned char height;
    unsigned char type;
} fontx_h;


/* Table of stored characters */
typedef struct {
    unsigned short start;
    unsigned short end;
} fontx_tbl;


int match(char *s, char *t)
{
    return(strncasecmp(s, t, strlen(t)));
}


#define XLFDCONV \
    "FONT -%[^-]-%*[^-]-%*[^-]-%*[^-]-%*[^-]--%d-%*d-%*d-%*d-%*[^-]-%d-%[^-]-%*s"
#define XLFDCONV2 \
    "FONT -%[^-]-%*[^-]-%*[^-]-%*[^-]-%*[^-]-%*[^-]-%d-%*d-%*d-%*d-%*[^-]-%d-%[^-]-%*s"
/*      jis   fixed  medium r      normal  14 130 75  75  C      70 jisx0201 */

void bdfheader(char *name, int *width, int *height, int *type, int *sjis)
{
    char s[BUFSIZ];
    char coding[BUFSIZ];

    *sjis = 0;
    *type = -1;
    *width = *height = -1;

    strcpy(name, "unknown");
    while (fgets(s, BUFSIZ, stdin) != NULL) {
        if (match(s, "ENDPROPERTIES") == 0) {
            break;
        }
        if (match(s, "FONT ") == 0) {
            sscanf(s, XLFDCONV, name, height, width, coding);
            if (-1 == *height) {
                sscanf(s, XLFDCONV2, name, height, width, coding);
            }

            *width /= 10;
            if (match(coding, "jisx0201") == 0) {
                *type = 0;
            } else if (match(coding, "jisx0208") == 0) {
                *type = 1;
                *sjis = 1;
            } else if (match(coding, "ISO10646") == 0) {
                *type = 1;
            } else {
                *type = 0;
            }
        }

        if (match(s, "FONTBOUNDINGBOX") == 0) {
            sscanf(s, "FONTBOUNDINGBOX %d %d %*d %*d", width, height);
        }
    }
}


char *temp()
{
    static char temp[BUFSIZ];
    char *s;

    if ((s = getenv("TMP")) != NULL) {
        strcpy(temp, s);
        return(temp);
    } else if ((s = getenv("TEMP")) != NULL) {
        strcpy(temp, s);
        return(temp);
    } else {
        strcpy(temp, TEMPDEFAULT);
    }

    return(temp);
}


/* Byte decomposition and composition */
#define lobyte(x) (((unsigned short)(x)) & 0xff)
#define hibyte(x) ((((unsigned short)(x)) >> 8) & 0xff)
#define hilo(h,l) ((((unsigned char)(h)) << 8) | ((unsigned char)(l)))


/* Overview of Shift JIS encoding
 *
 * ・ The upper byte [division] (0x21..0x7e) is assigned to (0x81..0x9f), (0xe0..0xef)
 * ・ Lower byte [point] (0x21..0x7e)
 * If it is an odd number, assign it to (0x40..0x7e), (0x80..0x9e)
 * If it is an even number, assign it to (0x9f..0xcf)
 */

#define JIS_START 0x21
#define JIS_END 0x7e
#define DELETE 0x7f
#define SJIS_HS1 0x81
#define SJIS_HE1 0x9f
#define SJIS_HS2 0xe0
#define SJIS_HE2 0xef
#define SJIS_LS1 0x40
#define SJIS_LS2 0x9f

/* Convert JIS to Shift JIS */
int jtos(unsigned short ch)
{
    int x, y;
    int hi, lo;

    hi = hibyte(ch);
    lo = lobyte(ch);

    /* High byte processing */
    x = (hi - JIS_START) / 2 + SJIS_HS1;
    if (x > SJIS_HE1) {
        x += (SJIS_HS2 - SJIS_HE1 - 1);
    }

    /* Low byte processing */
    if (hi % 2 == 1) {  /* Odd area */
        y = lo - JIS_START + SJIS_LS1;
        if (y >= DELETE) {
            ++y;
        }
    } else {  /* Even zone */
        y = lo - JIS_START + SJIS_LS2;
    }

    return(hilo(x, y));
}


/* Read BDF file and write to intermediate file */
int collect(FILE *co, FILE *gl, int width, int height, int type, int *ntab, int *sjis)
{
    char s[BUFSIZ];
    int n;
    int y;
    int p, b;
    int x;
    int chars;
    int start, lastcode;
    int code;
    int convwidth;

    setlocale(LC_ALL, "");

    *ntab = 0;
    chars = 0;
    start = 0;
    lastcode = -1 /* For when there is code 0 */;
    /* Round up the number of horizontal dots to a multiple of 8. */
    convwidth = ((width + 7) / 8) * 8;
    while(fgets(s, BUFSIZ, stdin) != NULL) {
        if (match(s, "ENCODING") == 0) {
            sscanf(s, "ENCODING %d", &n);
            code = *sjis ? jtos(n) : n;
            if (lastcode + 1 != code) {
                if (start != 0) {
                    fprintf(stderr, "Table %d %04x -> %04x\n", *ntab, start, lastcode);

                    fprintf(co, "%04x %04x\n", start, lastcode);
                    ++*ntab;
                }
                start = code;
                /* ANK font is embedded. TODO: If out of order */
                if (type == 0) {
                    for (++lastcode; lastcode < code; ++lastcode) {
                        for (y = 0; y < height; y++) {
                            for (x = convwidth; x > 0; x -= 8) {
                                putc(0, gl);
                            }
                        }
                    }
                }
            }
            lastcode = code;
            while (match(s, "BITMAP") != 0) {
                if (fgets(s, BUFSIZ, stdin) == NULL) {
                    break;
                }
            }

            for (y = 0; y < height; y++) {
                if (fgets(s, BUFSIZ, stdin) == NULL) {
                    break;
                }
                /* Some characters may be smaller than reported header height */
                if (match(s, "ENDCHAR") == 0) {
                    break;
                }
                while(strlen(s) * 4 < convwidth) {
                    s[strlen(s)-1] = '\0';
                    strcat(s, "00\n");
                }
                sscanf(s, "%x", &p);
                for (x = convwidth; x > 0; x -= 8) {
                    b = (p >> (x - 8)) & 0xff;
                    if (code != 0) {
                        putc(b, gl);
                    }
                }
            }

            /* Fill remaining lines if any with empty pixels */
            while(y < height) {
                for (x = convwidth; x > 0; x -= 8) {
                   putc(0x00, gl); 
                }
                y++;
            }

            /* Ignore extra character lines if any */
            while(match(s, "ENDCHAR") != 0 && fgets(s, BUFSIZ, stdin) != NULL);

            if (match(s, "ENDCHAR") != 0) {
                fprintf(stderr, "no ENDCHAR at %d (0x%x)\n", n, n);
            } else {
                fwprintf(stderr, L"%lc ", code);
                fprintf(stderr, "%d (0x%x) (dim: %d, %d)\n", code, code, convwidth, y);
                ++chars;
            }
        }
    }
    fprintf(co, "%x %x\n", start, lastcode);
    ++*ntab;
    /* ANK font fills up to 0xff */
    if (type == 0) {
        for (++lastcode; lastcode < 256; ++lastcode) {
            for (y = 0; y < height; y++) {
                for (x = convwidth; x > 0; x -= 8) {
                    putc(0, gl);
                }
            }
        }
    }
    return(chars);
}


#define FONTXNAMLEN 8

void fontxheader(char *name, int width, int height, int type)
{
    int i;

    fputs("FONTX2", stdout);
    for (i = 0; i < FONTXNAMLEN; i++) {
        if (name[i] == EOS) {
            break;
        }
        putc(toupper(name[i]), stdout);
    }

    for (; i < FONTXNAMLEN; i++) {
        putc(' ', stdout);
    }

    putc(width, stdout);
    putc(height, stdout);
    putc(type, stdout);
}


void codetable(FILE *co)
{
    int start, end;

    while (fscanf(co, "%x %x", &start, &end) == 2) {
        putc(lobyte(start), stdout);
        putc(hibyte(start), stdout);
        putc(lobyte(end), stdout);
        putc(hibyte(end), stdout);
    }
}

int main()
{
    int width, height, type;
    char name[BUFSIZ];
    char fcodetab[BUFSIZ];	/* Code table intermediate file */
    char fglyph[BUFSIZ];	/* Glyph intermediate file */
    FILE *co, *gl;
    int n;
    int ntab;
    int ch;
    int sjis;

    bdfheader(name, &width, &height, &type, &sjis);
    fprintf(stderr, "%s: %d x %d, type: %d, sjis %d\n", name, width, height, type, sjis);
    strcpy(fcodetab, temp());
    strcat(fcodetab, "/cXXXXXX.tbl");
    strcpy(fglyph, temp());
    strcat(fglyph, "/fXXXXXX.tbl");

    if ((co = fopen(fcodetab, "wb")) == NULL) {
        fprintf(stderr, "can't open %s\n", fcodetab);
        exit(1);
    }

    if ((gl = fopen(fglyph, "wb")) == NULL) {
        fprintf(stderr, "can't open %s\n", fglyph);
        exit(1);
    }

    n = collect(co, gl, width, height, type, &ntab, &sjis);
    if (fclose(co) != 0) {
        fprintf(stderr, "can't close %s\n", fcodetab);
    }

    if (fclose(gl) != 0) {
        fprintf(stderr, "can't close %s\n", fglyph);
    }

    fprintf(stderr, "%d chars, %d tables\n", n, ntab);
    fontxheader(name, width, height, type);
    if (type != 0) {
        if ((co = fopen(fcodetab, "rb")) == NULL) {
            fprintf(stderr, "can't open %s\n", fcodetab);
            exit(1);
        }
        putc(ntab, stdout);
        codetable(co);
    }

    if ((gl = fopen(fglyph, "rb")) == NULL) {
        fprintf(stderr, "can't open %s\n", fglyph);
        exit(1);
    }

    while ((ch = getc(gl)) != EOF) {
        putc(ch, stdout);
    }

    fclose(gl);
    exit(0);
}

/* Local Variables: */
/* compile-command:"cc -Wall -o bdf2fontx bdf2fontx.c" */
/* End: */
