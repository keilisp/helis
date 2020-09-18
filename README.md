# Helis

# Description

Simple terminal code editor with basic vim-like bindings and syntax highlighting

# Preview

![helis](https://raw.githubusercontent.com/mediocreeee/helis/master/helis_preview.png)

# Installation

Clone the repo into your folder

```sh
cd *your-folder*
git clone https://github.com/mediocreeee/helis.git
```

Compile the program with make

```sh
make
```

Run the _helis_ binary

```sh
./helis
```

or with file you want to edit

```sh
./helis textfile.c
```

# Usage

## Movement

### Normal Mode

- h - move cursor left
- j - move cursor down
- k - move cursor up
- l - move cursor right

---

- 0 - move cursor to the beginning of a line
- \$ - move cursor to the end of a line

---

- PageUp - scroll one page up
- PageDown - scroll one page down

---

- gg - move cursor to the beginning of a file
- G - move cursor to the end of a file

---

- / - search for a text
- ArrowUp/ArrowRight - go to the next occurrence
- ArrowDown/ArrowLeft - go to the previous occurrence

---

- x - delete a character under the cursor

---

- : - enable Cmd mode

---

- i - enable Insert mode
- I - move cursor to the beginning of a line and enable Insert mode
- a - move cursor right and enable Insert mode
- A - move cursor to the end of a line and enable Insert mode
- o - create newline down and enable Insert mode
- O - create newline up and enable Insert mode

### Cmd mode

---

- q/quit - exit from _helis_
- w/write - write changes to the disk
