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

## Host-Side File Management Examples (using `ftx`)

When the `SRL_HostIO` sample application is running on the Sega Saturn, the [ftx](https://github.com/willll/ftx) CLI tool can be used on the host PC to manage files and directories:

### Directory Listing (`List`)
List files and directories on the SD card:
```bash
./ftx --ls /
```
Get a detailed directory listing containing file sizes, attributes, and modification timestamps:
```bash
./ftx -l --ls /SD_TEST
```

### File Upload (`Upload`)
Copy a local file from the host PC onto the target Saturn's SD card FAT filesystem:
```bash
./ftx --cp data.bin /test_folder/data.bin
```

### Directory Management (`Mkdir` / `Rmdir`)
Create a new directory on the SD card:
```bash
./ftx --mkdir /new_folder
```
Delete an empty directory from the SD card:
```bash
./ftx --rmdir /new_folder
```

### File Deletion (`Remove`)
Delete a file on the SD card:
```bash
./ftx --rm /test_folder/data.bin
```

### File Checksum (`Crc`)
Calculate and verify the CRC-8 checksum of a file on the target device (supports files on the SD card or CD-ROM):
```bash
./ftx --crc /test_folder/data.bin
```

---

## Project Structure

```
SRL_HostIO/
├── INC/
│   ├── srl_devcart_hostio.hpp        # Core HostIo protocol decoder and frame sender
│   ├── srl_devcart_sdcard.hpp        # SD Card C++ FileSystem, File, and Directory wrappers
│   ├── srl_devcart_sdcard_lowlevel.hpp # Low-level registers, status functions, & sector routines
│   ├── srl_devcart_sdcard_backend.hpp  # Generic path parsing utilities & raw sector operations
│   ├── srl_devcart_sdcard_handlers.hpp # HostIO protocol command request handler implementations
│   ├── srl_devcart_sdcard_crc.hpp      # Generic CRC-8 checksum calculation routine
│   ├── fatfs/                        # FatFs FAT file system module headers
│   └── sgclib/                       # SGCLIB low-level SD driver interface headers
├── Samples/
│   ├── Debug - File transfer/         # Host-interactive file upload/download and diagnostics
│   ├── SDCard - Access/               # Loading and displaying a 24-bit uncompressed TGA texture
│   ├── SDCard - Directory Browser/    # Listing directories & file metadata in the FatFs partition
│   └── SDCard - Save State Logger/    # Persisting game progress and verifying file states
├── LICENSE                           # MIT License
└── module.mk                         # Optional local build integration configuration
```

---

## Samples Overview

`SRL_HostIO` includes several sample applications that showcase how to use the SD Card APIs:

### 1. Debug - File transfer
* **Description**: Sets up the framed binary protocol over USB FIFO. Demonstrates integration with host utilities like `ftx` to manage folders, upload/download files, compute checksums, or run raw sector diagnostics.
* **Usage**: Main application loop dispatching host commands.

### 2. SDCard - Access
* **Description**: Shows how to load assets directly from the SD card's FAT partition. Specifically, it loads an uncompressed 24-bit TGA image `/TEST.TGA`, decodes it into VDP1 texture memory, and displays it with a dynamic distorted sprite animation.
* **Usage**: Demonstrates `File` opening, chunked reading, and color space conversion to Saturn HighColor (RGB555) format.

### 3. SDCard - Directory Browser
* **Description**: Shows how to navigate and scan directories. Lists root directory contents and displays entries on screen, showing directory types, files, sizes, and last modification dates.
* **Usage**: Demonstrates the C++ wrappers `Directory`, `DirectoryEntry`, and `FileSystem::GetCurrentDirectory()`.

### 4. SDCard - Save State Logger
* **Description**: Demonstrates persistent save game logging. Reads an existing save state (with high scores, play coordinates), updates the information, writes it back to a file, and prints the updated metadata using `FileSystem::Stat()`.
* **Usage**: Demonstrates `File` write mode, data serialization/deserialization, and `FileSystem::Stat()`.

---

## Including SRL_HostIO in an SRL Project

If your project uses the SRL build system, add `SRL_HostIO` to `MODULES_EXTRA` in your project Makefile:

```makefile
MODULES_EXTRA = SRL_HostIO
```

The SRL shared makefile automatically adds `modules_extra/SRL_HostIO/INC` to the include path and copies any `cd/data/` files into the project image during build.

### Git integration

If `SRL_HostIO` is not already present in your repository, add it as a Git submodule under `modules_extra`:

```bash
git submodule add https://github.com/willll/SRL_HostIO.git modules_extra/SRL_HostIO
git submodule update --init --recursive
```

If you already have the module configuration in `.gitmodules`, update it with:

```bash
git submodule update --init --recursive modules_extra/SRL_HostIO
```

Then include the HostIO header in your code:

```cpp
#include <srl_devcart_hostio.hpp>
```

If you want to use the sample application instead of just the header, see `Samples/Debug - File transfer/makefile` for a real SRL project example.

---

## Core API Reference

### `INC/srl_devcart_hostio.hpp`
Located in the `SRL::DevCart::HostIo` namespace:
- **`TryReadRequest`**: Reads and decodes a request frame from the USB FIFO. Extracts the command and buffers the payload.
- **`SendResponse`**: Packages status and payload data into a standardized response frame and writes it back to the host via DevCart.
- **`WriteAll` / `ReadAll`**: Helper methods for writing and reading raw byte streams to and from the CPLD registers.

### `INC/srl_devcart_sdcard.hpp`
Provides C++ wrapper classes in the `SRL::DevCart::SD` namespace:
- **`FileSystem`**: Class managing the FAT partition volume (mounting, STAT metadata query, folder creation/deletion, renaming, and removal). Exposes a global singleton `g_fatfsBackend`.
- **`File`**: Represents a file descriptor, supporting `Open`, `Read`, `Write`, `Seek`, and `Close` using safe memory bounds.
- **`Directory`**: Handles scanning directory listings.
- **`DirectoryEntry`**: Struct storing metadata (dates, times, sizes, attributes) for a directory node.

### `INC/srl_devcart_sdcard_backend.hpp`
Located in the `SRL::DevCart::SD::Backend` namespace:
- **`TryParseRawRange`**: Parses raw sector ranges formatted as `sdraw:<sector>:<count>`.
- **`EraseRawRange`**: Zeroes out raw block sector ranges on the SD card.

### `INC/srl_devcart_sdcard_crc.hpp`
Provides the CRC checksum engine:
- **`Crc8Update`**: Accumulates and updates a CRC-8 checksum calculation over a block of bytes.

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
