# PortScannerUI

A high-performance, multithreaded TCP port scanner with a polished terminal user interface. Built with the Win32 Console API and Winsock2.

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

- Windows Vista or later
- A C11 compiler (MinGW GCC recommended)
- Winsock2 (included with Windows)

## Building

### MinGW GCC

```bash
# Release build
gcc -std=c11 -O2 src/main.c src/scanner.c src/ui.c src/utils.c -o bin/portscanner.exe -lws2_32

# Or use the Makefile
make
make run
make debug
make clean
```

### MSVC (Visual Studio)

```bash
cl /std:c11 /O2 src/main.c src/scanner.c src/ui.c src/utils.c /Fe:bin/portscanner.exe /link ws2_32.lib
```

## Usage

```
.\bin\portscanner.exe
```

The program will prompt for:
1. **Target** — hostname or IP address (e.g., `scanme.nmap.org`)
2. **Port range** — e.g., `1-1000` (default: 1-1024)
3. **Thread count** — 1-300 (default: 50)

## Project Structure

```
PortScannerUI/
├── src/
│   ├── config.h       Shared types, constants, and extern declarations
│   ├── main.c         Program entry point and orchestration
│   ├── scanner.c/h    Port scanning engine (Winsock, threading)
│   ├── ui.c/h         Terminal UI (console API, drawing, input)
│   └── utils.c/h      Utilities (formatting, service names)
├── bin/               Compiled binaries
├── .github/           Issue templates
├── Makefile           Build system
├── CHANGELOG.md
├── LICENSE            MIT
└── README.md
```

## License

MIT
