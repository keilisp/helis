/* Includes */
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <asm-generic/ioctls.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

/* Defines */
#define CTRL_KEY(k) ((k)&0x1f)
#define HELIS_VERSION "0.0.0.0.1"
#define HELIS_TAB_STOP 4

// Keys bindings
enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

// Row of text
typedef struct erow {
  int size;  // Chars size
  int rsize; // Render size
  char *chars;
  char *render;
} erow;

/* Data */
// Editor config
struct editorConfig {
  int cx, cy;     // Cursor coords
  int rx;         // Render coord
  int rowoff;     // Row offset
  int coloff;     // Column offset
  int screenrows; // Screen row count
  int screencols; // Screen columns count
  int numrows;    // File rows count
  erow *row;      // Row sctruct
  // Terminal attributes
  struct termios orig_termios;
};

struct editorConfig E;

/* Row */

// Find out rx
int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row->chars[j] == '\t') {
      rx += (HELIS_TAB_STOP - 1) - (rx % HELIS_TAB_STOP);
    }
    rx++;
  }
  return rx;
}

// Update Row
void editorUpdateRow(erow *row) {

  // Handling tabs
  int tabs = 0;
  int j;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t')
      tabs++;
  }

  free(row->render);
  row->render = malloc(row->size + tabs * (HELIS_TAB_STOP - 1) + 1);

  int idx = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while ((idx % HELIS_TAB_STOP) != 0)
        row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }

  row->render[idx] = '\0';
  row->rsize = idx;
}

// Append row
void editorAppendRow(char *s, size_t len) {
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  editorUpdateRow(&E.row[at]);

  E.numrows++;
}

/* Terminal */
// Fail handling
void die(const char *s) {
  // TODO: Refactor to editorRefreshScreen later
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[1;1H", 6);
  perror(s);
  exit(1);
}

// Disabling raw mode at exit to prevent issues
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

// Getting the atributes and setting flags to make terminal raw
void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
    die("tcgetattr");
  }

  atexit(disableRawMode);

  // Flags to make terminal 'true' raw
  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | IEXTEN | ISIG);

  // Min num of bytes needed before read() can return
  raw.c_cc[VMIN] = 0;
  // Max amount of time to wati before read() return
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}

// Reading the key from stdin
int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }

  if (c == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
          case '1':
            return HOME_KEY;
          case '3':
            return DEL_KEY;
          case '4':
            return END_KEY;
          case '5':
            return PAGE_UP;
          case '6':
            return PAGE_DOWN;
          case '7':
            return HOME_KEY;
          case '8':
            return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
        case 'A':
          return ARROW_UP;
        case 'B':
          return ARROW_DOWN;
        case 'C':
          return ARROW_RIGHT;
        case 'D':
          return ARROW_LEFT;
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
      }
    }
    return '\x1b';
  } else {
    switch (c) {
    case 'k':
      return ARROW_UP;
    case 'j':
      return ARROW_DOWN;
    case 'l':
      return ARROW_RIGHT;
    case 'h':
      return ARROW_LEFT;
    }
    return c;
  }
}

// Getting the cursor position
int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }
  buf[i] = '\0';
  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;
  return 0;
}

// Getting the window size
int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/* File I/o */

// Open file in the editor
void editorOpen(char *filename) {
  // Open file stream
  FILE *fp = fopen(filename, "r");
  if (!fp)
    die("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;

  // Copy line form file into erow struct
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    editorAppendRow(line, linelen);
  }
  free(line);
  // Closing file stream
  fclose(fp);
}

/* Append Buffer */
// Dynamic string
struct abuf {
  char *b;
  int len;
};

// Empty abuf
#define ABUF_INIT                                                              \
  { NULL, 0 }

// Append to dynamic string
void abAppend(struct abuf *ab, const char *s, int len) {
  // Allocate memory for new string
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL)
    return;
  // Append string into buffer
  memcpy(&new[ab->len], s, len);
  // Update buffer
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) { free(ab->b); }

/* Output */

// Scrolling
void editorScroll() {
  E.rx = 0;
  if (E.cy < E.numrows) {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }

  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencols) {
    E.coloff = E.rx - E.screencols + 1;
  }
}
// Drawing rows
void editorDrawRows(struct abuf *ab) {
  int r;
  for (r = 0; r < E.screenrows; r++) {
    // Rows in the file
    int filerow = r + E.rowoff;
    if (filerow >= E.numrows) {
      if (E.numrows == 0 && r == E.screenrows / 3) {
        // Printing editor version
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
                                  "Helis editor -- version %s", HELIS_VERSION);
        // If welcome string is longer then terminal columns number, truncate it
        if (welcomelen > E.screencols)
          welcomelen = E.screencols;
        // Centering the welcome message
        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(ab, ">", 1);
          padding--;
        }
        while (padding--)
          abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, ">", 1);
      }
    } else {
      int len = E.row[filerow].rsize - E.coloff;
      if (len < 0)
        len = 0;
      if (len > E.screencols)
        len = E.screencols;
      abAppend(ab, &E.row[filerow].render[E.coloff], len);
    }
    // Clear the line when redrawing
    abAppend(ab, "\x1b[K", 3);
    if (r < E.screenrows - 1)
      abAppend(ab, "\r\n", 2);
  }
}

// Refreshing Screen
void editorRefreshScreen() {
  editorScroll();

  struct abuf ab = ABUF_INIT;

  // Hiding the cursor
  abAppend(&ab, "\x1b[?25l", 6);
  // Move cursor to the top-left corner
  abAppend(&ab, "\x1b[1;1H", 6);

  editorDrawRows(&ab);

  // Move cursor to E.rx and E.cy
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
           (E.rx - E.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));

  // Showing cursor
  abAppend(&ab, "\x1b[?25h", 6);

  // Write the buffer's contents
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/* Input */

// Moving the cursor
void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  switch (key) {
  case ARROW_UP:
    if (E.cy != 0) {
      E.cy--;
    }
    break;
  case ARROW_DOWN:
    if (E.cy < E.numrows) {
      E.cy++;
    }
    break;
  case ARROW_LEFT:
    if (E.cx != 0) {
      E.cx--;
    } else if (E.cy > 0) {
      E.cy--;
      E.cx = E.row[E.cy].size;
    }
    break;
  case ARROW_RIGHT:
    if (row && E.cx < row->size) {
      E.cx++;
    } else if (row && E.cx == row->size) {
      E.cy++;
      E.cx = 0;
    }
    break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}

// Handling keypress
void editorProcessKeypress() {
  int c = editorReadKey();

  switch (c) {
  case CTRL_KEY('q'):
    // TODO: Refactor to editorRefreshScreen later
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[1;1H", 6);
    exit(0);
    break;
    // Handle Home and End
  case HOME_KEY:
    E.cx = 0;
    break;
  case END_KEY:
    E.cx = E.screencols - 1;
    break;

  // Handle PageUP and PageDown
  case PAGE_UP:
  case PAGE_DOWN: {
    int times = E.screenrows;
    while (times--)
      editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
  } break;

    // Handle hjkl and Arrows
  case ARROW_LEFT:
  case ARROW_DOWN:
  case ARROW_UP:
  case ARROW_RIGHT:
    editorMoveCursor(c);
    break;
  }
}

/* Init */
// Initialize the editor
void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.numrows = 0;
  E.coloff = 0;
  E.row = NULL;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("getWindowSize");
}

// Main
int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}
