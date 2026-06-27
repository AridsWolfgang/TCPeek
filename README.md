# TCPeek

A high-performance, cross-platform TCP port scanner with a polished terminal user interface.

## Features

- **Fast parallel scanning** — up to 300 concurrent worker threads
- **Real-time TUI** — live progress bar, scan speed, ETA, and port count
- **Scrollable results** — browse thousands of ports with keyboard controls
- **Colour-coded status** — green (open), red (closed), yellow (pending)
- **Service name lookup** — identifies 40+ well-known services (SSH, HTTP, MySQL, etc.)
- **Filter & export** — toggle to show only open ports, save results to file
- **Pause/Resume** — pause scanning without losing progress
- **Terminal resize** — automatically adapts to window size changes

## Controls

| Key | Action |
|-----|--------|
| `Q` / `ESC` | Quit scan |
| `Up` / `Down` | Scroll results |
| `PgUp` / `PgDn` | Page through results |
| `Home` / `End` | Jump to start/end |
| `O` | Toggle open-ports filter |
| `S` | Save results to file |
| `P` | Pause / resume scanning |

## Requirements

- Windows or Linux terminal
- A C11 compiler (MinGW GCC / GCC / Clang)
- Winsock2 (Windows) or pthreads (Linux)

## Building

### Windows

```pwsh
gcc -std=c11 -O2 src/main.c src/scanner.c src/ui.c src/utils.c src/platform_win32.c -o bin/tcpeek.exe -lws2_32
```

### Linux

```bash
gcc -std=c11 -O2 src/main.c src/scanner.c src/ui.c src/utils.c src/platform_linux.c -o bin/tcpeek -lpthread
```

### Makefile

```bash
make        # auto-detects OS
make run
make debug
make clean
```

### VS Code

Press `Ctrl+Shift+B` (uses `.vscode/tasks.json`).

## Usage

```
.\bin\tcpeek.exe     # Windows
./bin/tcpeek          # Linux
```

The program will prompt for:
1. **Target** — hostname or IP address (e.g., `scanme.nmap.org`)
2. **Port range** — e.g., `1-1000` (default: 1-1024)
3. **Thread count** — 1-300 (default: 50)

## Project Structure

```
TCPeek/
├── src/
│   ├── config.h           Shared types, constants, extern declarations
│   ├── main.c             Program entry point and orchestration
│   ├── scanner.c/h        Port scanning engine (Winsock / POSIX)
│   ├── ui.c/h             Terminal UI (platform-independent)
│   ├── utils.c/h          Utilities (formatting, service names)
│   ├── platform.h         Cross-platform abstraction interface
│   ├── platform_win32.c   Windows implementation
│   └── platform_linux.c   Linux implementation
├── bin/                   Compiled binaries
├── .vscode/               VS Code build task
├── .github/               Issue templates
├── Makefile               Build system
├── CHANGELOG.md
├── LICENSE                MIT
└── README.md
```

## License

MIT
