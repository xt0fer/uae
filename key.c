/*
 * key.c                
 *
 * Anthony's Editor January 93
 *
 * Public Domain 1991, 1993 by Anthony Howe.  No warranty.
 */

#include <ctype.h>
#include <string.h>
#include "header.h"
#include "key.h"

/* Variable length structure. */
typedef struct input_t {
        struct input_t *next;
        char *ptr;
        char buf[1];

} input_t;

static input_t *istack = NULL;
static char blank[] = " \t\r\n";

static int k_default _((FILE *, char *, keymap_t *));
static int k_define _((FILE *, char *, keymap_t *));
static int k_erase  _((FILE *, char *, keymap_t *));
static int k_help _((FILE *, char *, keymap_t *));
static int k_itself _((FILE *, char *, keymap_t *));
static int k_kill _((FILE *, char *, keymap_t *));
static int k_token _((FILE *, char *, keymap_t *));
static keymap_t *growkey _((keymap_t *, size_t));
static int ipush _((char *));
static int ipop _((void));
static void iflush _((void));

keyinit_t keywords[] = {
        { K_INSERT_ENTER, ".insert_enter", k_default },
        { K_INSERT_EXIT, ".insert_exit", k_default },
        { K_DELETE_LEFT, ".delete_left", k_default },
        { K_DELETE_RIGHT, ".delete_right", k_default },
        { K_BLOCK, ".block", k_default },
        { K_CUT, ".cut", k_default },
        { K_PASTE, ".paste", k_default },
        { K_UNDO, ".undo", k_default },
        { K_CURSOR_UP, ".cursor_up", k_default },
        { K_CURSOR_DOWN, ".cursor_down", k_default },
        { K_CURSOR_LEFT, ".cursor_left", k_default },
        { K_CURSOR_RIGHT, ".cursor_right", k_default },
        { K_PAGE_UP, ".page_up", k_default },
        { K_PAGE_DOWN, ".page_down", k_default },
        { K_WORD_LEFT, ".word_left", k_default },
        { K_WORD_RIGHT, ".word_right", k_default },
        { K_LINE_LEFT, ".line_left", k_default },
        { K_LINE_RIGHT, ".line_right", k_default },
        { K_FILE_TOP, ".file_top", k_default },
        { K_FILE_BOTTOM, ".file_bottom", k_default },
        { K_HELP, ".help", k_default },
        { K_HELP_OFF, ".help_off", k_token },
        { K_HELP_TEXT, ".help_text", k_help },
        { K_MACRO, ".macro", k_default },
        { K_MACRO_DEFINE, ".macro_define", k_define },
        { K_QUIT, ".quit", k_default },
        { K_QUIT_ASK, ".quit_ask", k_default },
        { K_FILE_READ, ".file_read", k_default },
        { K_FILE_WRITE, ".file_write", k_default },
        { K_STTY_ERASE, ".stty_erase", k_erase },
        { K_STTY_KILL, ".stty_kill", k_kill },
        { K_ITSELF, ".itself", k_itself },
        { K_REDRAW, ".redraw", k_default },
        { K_SHOW_VERSION, ".show_version", k_default },
        { K_LITERAL, ".literal", k_default },
        { K_ERROR, NULL, NULL }

};

/*
 * Encode the given string into the supplied buffer that will be
 * large enough to contain the string and the terminating '\0'.
 * Return length of string string encoded in buffer; or 0 for
 * an error.
 */
size_t
encodekey(str, buf)
char *str, *buf;
{
        long number;
        char *ptr, *ctrl;
        static char escmap[] = "bfnrst";
        static char escvalue[] = "\b\f\n\r \t";
        static char control[] = "@abcdefghijklmnopqrstuvwxyz[\\]^_";

        for (ptr = buf; *str != '\0'; ++ptr) {
                switch (*str) {
                case '^':
                        ++str;
                        if (*str == '?') {
                                /* ^? equals ASCII DEL character. */
                                *ptr = 0x7f;
                        } else {
                                /* Non-ASCII dependant control key mapping. */
                                if (isupper(*str))
                                        *str = tolower(*str);
                                if ((ctrl = strchr(control, *str)) == NULL)
                                        return (0);
                                *ptr = (char) (ctrl - control);
                        }
                        ++str;
                        break;
                case '\\':
                        /* Escapes. */
                        ++str;
                        if (isdigit(*str)) {
                                /* Numeric escapes allow for
                                 *  octal       \0nnn
                                 *  hex         \0xnn
                                 *  decimal     \nnn
                                 */
                                number = strtol(str, &str, 0);
                                if (255 < number)
                                        return (0);
                                *ptr = (char) number;
                                break;
                        }
                        if ((ctrl = strchr(escmap, *str)) != NULL) {
                                *ptr = escvalue[ctrl - escmap];
                                ++str;
                                break;
                        }
                        /* Literal escapes. */
                default:
                        /* Character. */
                        *ptr = *str++;
                }
        }
        *ptr = '\0';
        return ((size_t) (ptr - buf));

}

/*
 * Read a configuration file from either the current directory or
 * the user's home directory.  Return an error status.  Pass-back
 * either a pointer to a key mapping table, or NULL if an error
 * occured.
 */
int
initkey(fn, keys)
char *fn;
keymap_t **keys;
{
        FILE *fp;
        int error;
        keyinit_t *kp;
        keymap_t *array;
        char *buf, *token;
        size_t len, count;

        *keys = NULL;
        if ((fp = openrc(fn)) == NULL)
                return (INITKEY_OPEN);

        /* Allocate an array big enough to hold at least one of everything. */
        if ((array = growkey(NULL, len = K_MAX_CODES)) == NULL) {
                error = INITKEY_ALLOC;
                goto error1;
        }

        count = 0;
        while ((error = getblock(fp, "\n", &buf)) != GETBLOCK_EOF) {
                if (error == GETBLOCK_ALLOC) {
                        error = INITKEY_ALLOC;
                        goto error1;
                }

                if (buf[0] != '.'
                || (token = strtok(buf, blank)) == NULL
                || (kp = findikey(keywords, strlwr(token))) == NULL) {
                        free(buf);
                        continue;
                }

                array[count].code = kp->code;
                if (kp->fn != NULL) {
                        if (!(*kp->fn)(fp, buf, &array[count])) {
                                error = INITKEY_ERROR;
                                goto error1;
                        }
                }
                ++count;

                if (len <= count) {
                        keymap_t *new;
                        len += K_MAX_CODES;
                        if ((new = growkey(array, len)) == NULL) {
                                error = INITKEY_ALLOC;
                                goto error1;
                        }
                        array = new;
                }
        }
        error = INITKEY_OK;
        *keys = array;
error1:
        (void) fclose(fp);
        array[count].code = K_ERROR;
        if (error != INITKEY_OK)
                finikey(array);
        return (error);

}

void
finikey(keys)
keymap_t *keys;
{
        keymap_t *kp;
        if (keys != NULL) {
                for (kp = keys; kp->code != K_ERROR; ++kp) {
                        if (kp->lhs != NULL)
                                free(kp->lhs);
                }
                free(keys);
        }

}

/*
 * .help_text
 *   line(s) of text
 * .end
 *
 * Fetch a block of text.
 */
static int
k_help(fp, buf, kp)
FILE *fp;
char *buf;
keymap_t *kp;
{
        free(buf);
        return (getblock(fp, ".end\n", &kp->lhs) == GETBLOCK_OK);

}

/*
 * .function string
 *
 * Encode a key sequence for the given function.
 */
static int
k_default(fp, buf, kp)
FILE *fp;
char *buf;
keymap_t *kp;
{
        char *tok;
        size_t lhs;

        if ((tok = strtok(NULL, blank)) == NULL
        || (lhs = encodekey(tok, buf)) == 0
        || (kp->lhs = (char *) realloc(buf, lhs)) == NULL) {
                free(buf);
                return (FALSE);
        }
        return (TRUE);

}

/*
 * .macro_define
 * .macro_define lhs rhs
 *
 * The first case is used as a place holder to reserve macro
 * space.  The second case actual defines a macro.
 */
static int
k_define(fp, buf, kp)
FILE *fp;
char *buf;
keymap_t *kp;
{
        char *tok;
        size_t lhs, rhs;

        if ((tok = strtok(NULL, blank)) == NULL) {
                return (k_token(fp, buf, kp));
        } else if ((lhs = encodekey(tok, buf)) != 0
        && (tok = strtok(NULL, blank)) != NULL
        && (rhs = encodekey(tok, buf+lhs+1)) != 0
        && (kp->lhs = realloc(buf, lhs+rhs+2)) != NULL) {
                kp->rhs = kp->lhs + lhs + 1;
                return (TRUE);
        }
        free(buf);
        return (FALSE);

}

/*
 * .token
 */
static int
k_token(fp, buf, kp)
FILE *fp;
char *buf;
keymap_t *kp;
{
        free(buf);
        kp->lhs = kp->rhs = NULL;
        return (TRUE);

}

/*
 * .itself character
 */
static int
k_itself(fp, buf, kp)
FILE *fp;
char *buf;
keymap_t *kp;
{
        buf[1] = '\0';
        kp->code = *(unsigned char *) buf;
        return ((kp->lhs = (char *) realloc(buf, 2)) != NULL);

}

/*
 * .stty_erase
 */
static int
k_erase(fp, buf, kp)
FILE *fp;
char *buf;
keymap_t *kp;
{
        buf[0] = erasechar();
        buf[1] = '\0';
        return ((kp->lhs = (char *) realloc(buf, 2)) != NULL);

}

/*
 * .stty_kill
 */
static int
k_kill(fp, buf, kp)
FILE *fp;
char *buf;
keymap_t *kp;
{
        buf[0] = killchar();
        buf[1] = '\0';
        return ((kp->lhs = (char *) realloc(buf, 2)) != NULL);

}

/*
 * Find token and return corresponding table entry; else NULL.
 */
keymap_t *
findkey(kp, token)
keymap_t *kp;
char *token;
{
        for (; kp->code != K_ERROR; ++kp)
                if (kp->lhs != NULL && strcmp(token, kp->lhs) == 0)
                        return (kp);
        return (NULL);

}

keyinit_t *
findikey(kp, token)
keyinit_t *kp;
char *token;
{
        for (; kp->code != K_ERROR; ++kp)
                if (kp->lhs != NULL && strcmp(token, kp->lhs) == 0)
                        return (kp);
        return (NULL);

}

/*
 *
 */
static keymap_t *
growkey(array, len)
keymap_t *array;
size_t len;
{
        keymap_t *new;
        if (len == 0)
                return (NULL);
        len *= sizeof (keymap_t);
        if (array == NULL)
                return ((keymap_t *) malloc(len));
        return ((keymap_t *) realloc(array, len));

}

int
getkey(keys)
keymap_t *keys;
{
        keymap_t *k;
        int submatch;
        static char buffer[K_BUFFER_LENGTH];
        static char *record = buffer;

        /* If recorded bytes remain, return next recorded byte. */
        if (*record != '\0')
                return (*(unsigned char *)record++);
        /* Reset record buffer. */
        record = buffer;
        do {
                if (K_BUFFER_LENGTH < record - buffer) {
                        record = buffer;
                        buffer[0] = '\0';
                        return (K_ERROR);
                }
                /* Read and record one byte. */
                *record++ = getliteral();
                *record = '\0';

                /* If recorded bytes match any multi-byte sequence... */
                for (k = keys, submatch = 0; k->code != K_ERROR; ++k) {
                        char *p, *q;
                        if (k->lhs == NULL
                        || k->code == K_DISABLED || k->code == K_HELP_TEXT)
                                continue;
                        for (p = buffer, q = k->lhs; *p == *q; ++p, ++q) {
                                if (*q == '\0') {
                                        if (k->code == K_LITERAL)
                                                return (getliteral());
                                        if (k->code != K_MACRO_DEFINE) {
                                                /* Return extended key code. */
                                                return (k->code);
                                        }
                                        if (k->rhs != NULL)
                                                (void) ipush(k->rhs);
                                        break;
                                }
                        }
                        if (*p == '\0' && *q != '\0') {
                                /* Recorded bytes match anchored substring. */
                                submatch = 1;
                        }
                }
                /* If recorded bytes matched an anchored substring, loop. */
        } while (submatch);
        /* Return first recorded byte. */
        record = buffer;
        return (*(unsigned char *)record++);

}

int
getliteral()
{
        int ch;

        ch = ipop();
        if (ch == EOF)
                return ((unsigned) getch());
        return (ch);

}

/*
 * Return true if a new input string was pushed onto the stack,
 * else false if there was no more memory for the stack.
 */
static int
ipush(buf)
char *buf;
{
        input_t *new;

        new = (input_t *) malloc(sizeof (input_t) + strlen (buf));
        if (new == NULL)
                return (FALSE);
        (void) strcpy(new->buf, buf);
        new->ptr = new->buf;
        new->next = istack;
        istack = new;
        return (TRUE);

}

/*
 * Pop and return a character off the input stack.  Return EOF if
 * the stack is empty.  If the end of an input string is reached,
 * then free the node.  This will allow clean tail recursion that
 * won't blow the stack.  
 */
static int
ipop()
{
        int ch;
        input_t *node;

        if (istack == NULL)
                return (EOF);
        ch = (unsigned) *istack->ptr++;
        if (*istack->ptr == '\0') {
                node = istack;
                istack = istack->next;
                free(node);
        }
        return (ch);

}

/*
 * Flush the entire input stack.
 */
static void
iflush()
{
        input_t *node;

        while (input != NULL) {
                node = istack;
                istack = istack->next;
                free(node);
        }

}

int
ismacro()
{
        return (istack != NULL);

}

/*
 * Field input.
 */
typedef struct key_entry_t {
        int code;
        int (*func) _((void));

} key_entry_t;

static int fld_done _((void));
static int fld_erase _((void));
static int fld_kill _((void));
static int fld_left _((void));
static int fld_insert _((void));

#define ERASE_KEY       0
#define KILL_KEY        1

static key_entry_t ktable[] = {
        { K_STTY_ERASE, fld_erase },
        { K_STTY_KILL, fld_kill },
        { '\r', fld_done },
        { '\n', fld_done },
        { '\b', fld_erase },
        { -1, fld_insert }

};

static int fld_row;
static int fld_col;
static int fld_key;
static int fld_echo;
static int fld_index;
static int fld_length;
static char *fld_buffer;

#ifndef getmaxyx
#define getmaxyx(w,r,c)         (r=LINES,c=COLS)
#endif

int
getinput(buf, len, echoing)
char *buf;
int len, echoing;
{
        int first;
        key_entry_t *k;
        fld_buffer = buf;
        fld_index = (int) strlen(fld_buffer);
        fld_length = len < 0 ? COLS : len;
        if (--fld_length < 1)
                return (FALSE);
        ktable[ERASE_KEY].code = erasechar();
        ktable[KILL_KEY].code = killchar();    
        fld_echo = echoing;
        getyx(stdscr, fld_row, fld_col);
        addstr(fld_buffer);
        move(fld_row, fld_col);
        for (first = TRUE;; first = FALSE) {
                refresh();
                fld_key = getliteral();
                for (k = ktable; k->code != -1 && k->code != fld_key; ++k)
                        ;
                if (first && k->func == fld_insert)
                        fld_kill();
                if (k->func != NULL && !(*k->func)()) {
                        fld_buffer[fld_index] = '\0';
                        break;
                }
        }
        return (TRUE);

}

static int
fld_done()
{
        return (FALSE);

}

static int
fld_left()
{
        int row, col, max_row, max_col;
        getyx(stdscr, row, col);
        getmaxyx(stdscr, max_row, max_col);
        if (0 < fld_index) {
                --fld_index;
                /* Assume that if 0 < fld_index then fld_row <= row
                 * and fld_col < col.  So when fld_index == 0, then
                 * fld_row == row and fld_col == col.
                 */
                if (0 < col) {
                        --col;
                } else if (0 < row) {
                        /* Handle reverse line wrap. */
                        --row;
                        col = max_col-1;
                }
                move(row, col);
        }
        return (TRUE);

}

static int
fld_erase()
{
        int row, col;
        if (0 < fld_index) {
                fld_left();
                getyx(stdscr, row, col);
                addch(' ');
                move(row, col);
                fld_buffer[fld_index] = '\0';
        }
        return (TRUE);

}

static int
fld_kill()
{
        move(fld_row, fld_col);
        while (0 < fld_index--)
                addch(' ');
        move(fld_row, fld_col);
        fld_buffer[0] = '\0';
        fld_index = 0;
        return (TRUE);

}

static int
fld_insert()
{
        if (fld_index < fld_length) {
                if (!ISFUNCKEY(fld_key)) {
                        fld_buffer[fld_index++] = fld_key;
                        if (fld_echo)
                                addch(fld_key);
                }
        }
        return (fld_index < fld_length);

}

