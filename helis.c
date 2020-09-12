 /* Includes */
 #include <asm-generic/ioctls.h>
 #include <ctype.h>
 #include <errno.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <sys/ioctl.h>
 #include <termios.h>
 #include <unistd.h>

 /* Defines */
 #define CTRL_KEY(k) ((k)&0x1f)
 #define HELIS_VERSION "0.0.0.0.1"

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

 /* Data */
 // Editor config
 struct editorConfig {
   int cx, cy; // Cursor coords
   int screenrows;
   int screencols;
   // Terminal attributes
   struct termios orig_termios;
 };

 struct editorConfig E;

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
   raw.c_cc[VTIME] = 8;

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
 // Drawing rows
 void editorDrawRows(struct abuf *ab) {
   int r;
   for (r = 0; r < E.screenrows; r++) {
     if (r == E.screenrows / 3) {
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
     // Clear the line when redrawing
     abAppend(ab, "\x1b[K", 3);
     if (r < E.screenrows - 1)
       abAppend(ab, "\r\n", 2);
   }
 }

 // Refreshing Screen
 void editorRefreshScreen() {
   struct abuf ab = ABUF_INIT;

   // Hiding the cursor
   abAppend(&ab, "\x1b[?25l", 6);
   // Move cursor to the top-left corner
   abAppend(&ab, "\x1b[1;1H", 6);

   editorDrawRows(&ab);

   // Move cursor to E.cx and E.cy
   char buf[32];
   snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
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
   switch (key) {
   case ARROW_UP:
     if (E.cy != 0) {
       E.cy--;
     }
     break;
   case ARROW_DOWN:
     if (E.cy != E.screenrows - 1) {
       E.cy++;
     }
     break;
   case ARROW_LEFT:
     if (E.cx != 0) {
       E.cx--;
     }
     break;
   case ARROW_RIGHT:
     if (E.cx != E.screencols - 1) {
       E.cx++;
       break;
     }
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
   if (getWindowSize(&E.screenrows, &E.screencols) == -1)
     die("getWindowSize");
 }

 // Main
 int main(int argc, char **argv) {
   enableRawMode();
   initEditor();

   while (1) {
     editorRefreshScreen();
     editorProcessKeypress();
   }

   return 0;
 }
