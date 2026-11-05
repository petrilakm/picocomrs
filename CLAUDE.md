# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

picocom (v3.2a): a minimal dumb-terminal emulator for serial ports, written in C against POSIX termios. Despite the repo name, the code is C, not Rust. It targets Linux first, plus macOS and the BSDs; it does not build natively on Windows, so build and run it under WSL or a Unix host.

## Build

```sh
make                 # builds ./picocom (GNU make; use gmake on BSDs)
make clean && make   # rebuild after changing CPPFLAGS or Makefile options
cd <builddir> && make -f <srcdir>/Makefile VPATH=<srcdir>   # out-of-tree build; object subdirs are created by the .c.o rule
make doc             # regenerates picocom.1, picocom.1.html, picocom.1.pdf from picocom.1.md (needs pandoc, groff, and ~/.pandoc templates)
```

There are no automated tests and no lint target. To verify a change, build it with `-Wall` (the default) and run it against a real serial device or a pty pair (for example `socat -d -d pty,raw,echo=0 pty,raw,echo=0`). `./picocom --help` prints the compile-time features that were enabled.

### Compile-time options

Features are toggled by `-D` flags, either by commenting lines in or out in the `Makefile` or with `CPPFLAGS=... make`:

- `HIGH_BAUD`, `USE_FLOCK` (on by default), `UUCP_LOCK_DIR` (off; don't enable it together with flock)
- `LINENOISE` + `HISTFILE`: line editing and history for the filename/baud/hex prompts, using the vendored `linenoise-1.0/`. Without it, `picocom.c` falls back to simple `read_*` implementations (`#ifndef LINENOISE` block).
- `USE_CUSTOM_BAUD` / `NO_CUSTOM_BAUD`: force custom baudrates on or off (see below)
- `NO_HELP`: strips help strings
- `TTY_Q_SZ`: cap on the output queue size (0 = unlimited)
- `USE_TE_GPIO`: RS485 transmit-enable GPIO (`--te-gpio`, `--te-polarity`). Set by `TE_GPIO` in the Makefile, which auto-detects a Raspberry Pi from `/proc/device-tree/model` (override with `make TE_GPIO=1|0`). All of its code in `picocom.c` sits inside `#ifdef USE_TE_GPIO`, so builds without it are unchanged. Keep it that way.

`Android.mk` duplicates these settings for Android builds, so keep it in sync when you add source files or flags. When you add a new `.c` file, also add it to `OBJS`, give it a header-dependency rule, and add its `.o` to `clean` in the `Makefile`.

## Architecture

- **picocom.c**: the whole application. A global `opts` struct holds the command-line settings (`parse_args`). `main` handles locking (flock or UUCP), opens the port, configures it through the `term_*` API, and then runs `loop()`. `loop()` is a `select()`-based state machine with two states: `ST_TRANSPARENT` passes keystrokes through to the port, and `ST_COMMAND` is entered after the escape key (default `C-a`), where `do_command()` handles the key. Output to the port goes through the `tty_q` queue. Character mappings (`--imap/--omap/--emap`) live in `do_map`/`map_and_write`. File transfers (`run_cmd`) fork the external `--send-cmd`/`--receive-cmd` programs without a shell; arguments are tokenized with `split.c`, and this is a deliberate security property.
- **term.c / term.h**: a self-contained terminal-management library that wraps termios. For each managed fd it keeps three settings structures: `origtermios`, `currtermios` and `nexttermios`. The `term_set_*` functions modify only `nexttermios`, and `term_apply` writes it to the device. Original settings are restored through an atexit handler. The header comment in `term.h` documents the full API.
- **Custom baudrate layer**: `custbaud.h` decides per platform whether `USE_CUSTOM_BAUD` gets defined and selects `CUSTOM_BAUD_HEAD`:
  - Linux (x86/x86_64 with glibc `c_ispeed`/`c_ospeed`): `termios2.h`/`termios2.c`. `termios2.h` **macro-replaces** `tcsetattr`, `tcgetattr`, `cfsetispeed` and the others with `TCGETS2`/`TCSETS2` ioctl versions, using `termbits2.h`. The `NO_CUSTOM_BAUD` env var turns this off at runtime (`use_custom_baud()` in `custbaud.c`).
  - macOS/BSD: `custbaud_bsd.h`/`custbaud_bsd.c`.
  - Background notes are in `termios2.txt` and `bsd_notes.txt`.
- **te_gpio.c / te_gpio.h**: RS485 TE line control through the Linux GPIO chardev ioctls. It uses the v2 ABI when `<linux/gpio.h>` provides it, and the v1 ABI otherwise. In `picocom.c`, `te_tx_begin()` runs before each `write()` to the port. `te_tx_end()` runs at the top of `loop()` once `tty_q` is empty: it calls `tcdrain` and then releases TE. Any new code path that transmits to the port must assert TE in the same way.
- **fdio.c**: small I/O helpers that retry on EINTR (`writen_ni`, `fd_printf`).
- **split.c**: shell-like quoted-argument splitting.
- **bash_completion/picocom**: a custom Bash (>= 4.3) completion script. Its `DEBUG` variable defaults to `/dev/null`.
- `pcasc`, `pcxm`, `pcym`, `pczm`: wrapper scripts that launch picocom with ascii-xfr or x/y/zmodem send and receive commands.

## Documentation

The source of truth for the man page is `picocom.1.md`. `picocom.1`, `picocom.1.html` and `picocom.1.pdf` are generated from it but committed to the repo. When you change options or commands, update `picocom.1.md`, the `show_usage`/`show_keys` help text in `picocom.c`, and the bash completion script together. `lowerrts.md` documents the RTS/DTR behavior when the port is opened.

## Style

Use 4-space indentation and LF line endings (`.editorconfig`). The code uses K&R-ish C with a space before the call parens in many places (`term_set_raw (fd)`) and spaces inside `if ( ... )` conditions. Match the surrounding code.
