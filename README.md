# SRL_HostIO

A C++ communication and file transfer library for **Sega Saturn** homebrew development. `SRL_HostIO` implements a minimal framed command/response protocol over the USB FIFO of a Sega Saturn USB Development Cartridge (such as the USB Gamer's Cartridge). 

It allows host-side debugging utilities (like [ftx](https://github.com/willll/ftx)) to interact with the Saturn's SD card, query file statuses, compute checksums, and perform directory/file management in real-time.

---

## Features

- **Framed Binary Protocol**: Lightweight command/response structures prefixed with a magic identifier (`SRL1`) to prevent synchronization issues.
- **FAT Filesystem Operations**: Interface for mounting and manipulating an SD card's FAT partition:
  - Directory listing (`List`) with optional detailed metadata (`-l` flag: size, timestamp, attributes).
  - Directory creation and deletion (`Mkdir`, `Rmdir`).
  - File deletion (`Remove`).
  - Safe file uploads (`Upload`) with chunked transmission and error recovery.
- **Raw Sector Management**: Directly read and erase/zero raw sector ranges using the `sdraw:<start_sector>:<sector_count>` path scheme.
- **Data Integrity Verification**: Built-in CRC-8 checksum calculation supporting files on both the SD card and the CD-ROM/host filesystems.
- **On-Screen Diagnostics**: A scrolling debug console print utility to output commands, paths, and status codes on the TV/monitor screen.

---

## Protocol Specification

All frames transmitted over the DevCart USB FIFO use big-endian encoding for multibyte integers.

### Request Frame (Host $\rightarrow$ Saturn)
| Field | Offset (Bytes) | Size (Bytes) | Description |
| :--- | :--- | :--- | :--- |
| **Magic Header** | 0 | 4 | ASCII representation `"SRL1"` |
| **Command ID** | 4 | 1 | Type of command (see list below) |
| **Payload Length** | 5 | 2 | Big-endian length of the payload ($N$) |
| **Payload** | 7 | $N$ | Command-specific string or binary arguments (e.g., path) |

#### Commands
- `List = 1`: List files in a directory or inspect a raw sector range.
- `Remove = 2`: Remove a file or erase a raw sector range.
- `Crc = 3`: Calculate the CRC-8 checksum of a file.
- `Upload = 4`: Upload a file to the SD card.
- `Mkdir = 5`: Create a directory.
- `Rmdir = 6`: Remove a directory.

---

### Response Frame (Saturn $\rightarrow$ Host)
| Field | Offset (Bytes) | Size (Bytes) | Description |
| :--- | :--- | :--- | :--- |
| **Magic Header** | 0 | 4 | ASCII representation `"SRL1"` |
| **Status Code** | 4 | 1 | Status of the requested operation (see list below) |
| **Payload Length** | 5 | 2 | Big-endian length of response payload ($N$) |
| **Payload** | 7 | $N$ | Text output or data returned by the command |

#### Status Codes
- `Ok = 0`: Operation completed successfully.
- `Error = 1`: General error or operation failure.
- `Unsupported = 2`: Command or target path scheme is unsupported.
- `BadRequest = 3`: Invalid request parameters or payload formatting.
- `Handled = 4`: Request was processed, response sent manually (e.g., in chunked uploads).

---

## Project Structure

```
SRL_HostIO/
├── INC/
│   └── hostio.hpp           # Core HostIo protocol decoder and frame sender
├── Samples/
│   └── Debug - File transfer/
│       ├── makefile         # Compilation setup for Saturn Ring Library SDK
│       ├── run_on_saturn.bat# Convenience runner script for Windows/Linux emulator/cartridge
│       └── src/
│           ├── main.cxx     # Sample application main loop and request dispatcher
│           ├── srl_devcart_sdcart.hpp # SD raw sector routines & SGCLIB wrapper
│           ├── fatfs/       # FatFs FAT file system module headers
│           └── sgclib/      # SGCLIB low-level SD driver interface headers
├── LICENSE                  # MIT License
└── module.mk                # Optional local build integration configuration
```

---

## Core API Reference

### `INC/hostio.hpp`
Located in the `SRL::DevCart::HostIo` namespace:
- **`TryReadRequest`**: Reads and decodes a request frame from the USB FIFO. Extracts the command and buffers the payload.
- **`SendResponse`**: Packages status and payload data into a standardized response frame and writes it back to the host via DevCart.
- **`WriteAll` / `ReadAll`**: Helper methods for writing and reading raw byte streams to and from the CPLD registers.

### `Samples/Debug - File transfer/src/srl_devcart_sdcart.hpp`
Located in the `SRL::DevCart::SD` namespace:
- **`fs_load_stub`**: Copies the compiled SGCLIB driver assembly stub into Saturn High Work RAM.
- **`fs_init`**: Initializes the low-level SD interface.
- **`Backend::TryParseRawRange`**: Parses raw sector paths (e.g. `sdraw:0x2000:10`).
- **`Backend::EraseRawRange`**: Fills a specified number of raw blocks starting at a specific sector with zeroes.

---

## Build & Run Instructions

To build the sample file transfer application:

1. Ensure the **[Saturn Ring Library (SRL)](https://github.com/ReyeMe/SaturnRingLib)** SDK is installed.
2. Open `Samples/Debug - File transfer/makefile` and adjust the path to the SDK root:
   ```makefile
   SRL_INSTALL_ROOT ?= /path/to/your/srl/installation
   ```
3. Run `make` in the sample directory to generate the Saturn bootable image (`.iso` / `.bin`).
4. To test on hardware using a USB Dev Cartridge:
   ```bash
   ./run_on_saturn.bat
   # (Runs ../../tools/scripts/run.sh with the USBGamers parameter)
   ```

## License

This project is licensed under the [MIT License](LICENSE).
