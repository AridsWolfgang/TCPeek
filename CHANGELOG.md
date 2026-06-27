# Changelog

## [2.0.0] — 2026-06-27

### Added
- Renamed project to TCPeek
- Professional module-based project structure (src/config.h, scanner, ui, utils)
- Unicode box-drawing UI with double-line borders and section dividers
- Service name lookup for 40+ well-known ports
- Smoothed scan speed display with ETA calculation
- Pause/resume functionality (P key)
- Thousands-separated number formatting (1,234 / 5,000)
- Terminal resize detection and automatic redraw
- Makefile with release and debug targets
- Comprehensive README, MIT license, changelog, and GitHub issue templates

### Changed
- Complete UI redesign with cohesive colour scheme
- Replaced single-file program with 4-module architecture
- Build output now goes to bin/ directory
- Saved scan results include formatted table with service names

## [1.0.0] — 2026-06-XX

### Added
- Single-file multithreaded TCP port scanner (original main.c)
- Real-time TUI with progress bar, statistics, and scrollable results
- Interactive controls: scrolling, open-port filter, save to file
- Configurable target, port range, and thread count
