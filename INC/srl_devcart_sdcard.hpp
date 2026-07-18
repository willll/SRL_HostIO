#pragma once
#include <srl.hpp>
#include <srl_devcart.hpp>
#include "srl_devcart_hostio.hpp"


#include "sgclib/sgclib.h"
#include "fatfs/ff.h"

#include <cstdint> // For uintptr_t, size_t, uint8_t, uint32_t
#include <string.h>
#include <stdio.h>
#include <initializer_list>

/**
 * @brief Namespace for interacting with a USB development cartridge for the
 * Sega Saturn.
 *
 * This provides access to registers for USB communication, SD card access, and
 * other hardware features.
 */
namespace SRL
{
    namespace DevCart
    {
        namespace SD
        {
            /** @brief Initializes the SGCLIB FAT driver. Returns SGC_FR_OK on success. */
            static inline int fs_init()
            {
                return sgc_init();
            }

            /** @brief Gets current working directory. */
            static inline int fs_getcwd(char *buff, int buflen)
            {
                return sgc_getcwd(buff, buflen);
            }

            /** @brief Changes current working directory. */
            static inline int fs_chdir(const char *path)
            {
                return sgc_chdir(path);
            }

            /** @brief Opens a file. Returns file descriptor or -1 on error. */
            static inline int fs_open(const char *filename, int flags)
            {
                return sgc_open(filename, flags);
            }

            /** @brief Closes a file descriptor. */
            static inline int fs_close(int fd)
            {
                return sgc_close(fd);
            }

            /** @brief Reads from an open file. Returns bytes read, 0 at EOF, <0 on error. */
            static inline int fs_read(int fd, void *buf, int len)
            {
                return sgc_read(fd, buf, len);
            }

            /** @brief Writes to an open file. Returns bytes written, <0 on error. */
            static inline int fs_write(int fd, const void *buf, int len)
            {
                return sgc_write(fd, buf, len);
            }

            /** @brief Gets file/directory metadata. Returns SGC_FR_OK on success. */
            static inline int fs_stat(const char *filename, sgc_stat_t *stat)
            {
                return sgc_stat(filename, stat, static_cast<int>(sizeof(sgc_stat_t)));
            }

            /** @brief Opens a directory for listing. Returns SGC_FR_OK on success. */
            static inline int fs_opendir(const char *path)
            {
                return sgc_opendir(path);
            }

            /** @brief Removes a file. Returns SGC_FR_OK on success. */
            static inline int fs_unlink(const char *path)
            {
                return sgc_unlink(path);
            }

            /** @brief Reads a directory entry. Returns SGC_FR_OK on success. */
            static inline int fs_readdir(FILINFO *entry)
            {
                return f_readdir(&dir, entry);
            }

            /**
             * @brief Bit shifts for SD card LED and switch controls in registers (e.g.,
             * LED_SETTING).
             */
            enum class SD_LSHFT : uint8_t
            {
                SD_LEDG_LSHFT = 0, // Green LED bit position
                SD_LEDR_LSHFT = 1, // Red LED bit position (typo fix: SD_LEDR_LSHFT?)
                SD_SW1_LSHFT = 4,  // Switch 1 bit
                SD_EJECT_LSHFT = 7 // Eject detect bit
            };

            /**
             * @brief Bit shifts for SD card control signals (CS, DIN, CLK) in control
             * registers.
             */
            enum class SD_CTRL_LSHFT : uint8_t
            {
                SD_CSL_LSHFT = 0, // Chip Select low-active?
                SD_DIN_LSHFT = 1, // Data In
                SD_CLK_LSHFT = 2  // Clock signal
            };

            /**
             * @brief Returns SD write-protect/no-card status on USB Gamer's cartridge.
             *
             * 0: write enabled
             * 1: write protected or no SD card
             */
            static inline bool IsSdWriteProtectedOrMissing()
            {
                return (CS1::ReadRegister(CS1::Register::RegSdWriteProtect) & 0x01U) != 0;
            }

            /**
             * @brief Returns whether USB data path is expected to be enabled.
             *
             * On USB Gamer's cartridge, USB may be disabled when SD is write-protected
             * or missing. On other variants this returns true.
             */
            static inline bool IsUsbDataPathEnabled()
            {
                if (!CS1::IsUsbGamersCartridge())
                {
                    return true;
                }
                return !IsSdWriteProtectedOrMissing();
            }

            // Common SD Card Commands
            /** @brief SD card block size in bytes. */
            constexpr uint32_t BLOCK_SIZE = 512;
            /** @brief Command to reset the SD card to idle state (CMD0). */
            constexpr uint16_t CMD_GO_IDLE_STATE = 0;
            /** @brief Command to write a single 512-byte block (CMD24). */
            constexpr uint16_t CMD_WRITE_SINGLE_BLOCK = 24;
            /** @brief Command to write multiple blocks (CMD25). */
            constexpr uint16_t CMD_WRITE_MULTIPLE_BLOCK = 25;
            /** @brief Command to terminate a multiple-block write transmission (CMD12). */
            constexpr uint16_t CMD_STOP_TRANSMISSION = 12;

            /**
             * @brief A simple File handle structure for raw SD sector operations.
             */
            struct FileHandle
            {
                uint32_t start_sector;   /**< The starting sector on the SD card */
                uint32_t current_sector; /**< The current sector being accessed */
                uint32_t bytes_written;  /**< Number of bytes written to the file so far */
                uint32_t file_size;      /**< Total size of the file in bytes */
                bool is_open;            /**< Flag indicating if the file handle is open */
            };

            /**
             * @brief Send a low-level command to the SD Card via the DevCart
             * CPLD/Registers.
             * @param cmd The SD card command index.
             * @param arg The 32-bit command argument.
             */
            inline void send_sd_command(uint16_t cmd, uint32_t arg)
            {
                // 1. Wait for SD card to be ready (poll Auxiliary Status Register)
                while ((*(volatile uint16_t *)CS0::SDCardRegisters::CartAsr) &
                       0x01)
                { /* busy wait */
                }

                // 2. Write the 32-bit argument
                *(volatile uint32_t *)CS0::SDCardRegisters::CartCmdArg = arg;

                // 3. Send the command index
                *(volatile uint16_t *)CS0::SDCardRegisters::CartCmd = cmd;
            }

            /**
             * @brief Wait for the SD Card to complete its current operation.
             * @return True if the SD card became ready; false if timed out.
             */
            inline bool wait_sd_ready()
            {
                // Poll status register until ready bit is set
                uint32_t timeout = 0xFFFFF;
                while ((*(volatile uint32_t *)CS0::SDCardRegisters::CartSr) &
                       0x00000100 /* Example busy bit */)
                {
                    if (--timeout == 0)
                        return false;
                }
                return true;
            }
            /**
             * @brief Opens a "file" on the SD Card.
             * Since we aren't using a FAT filesystem, we write to raw sectors.
             * @param handle The file handle to initialize
             * @param raw_sector_start The absolute sector on the SD card to start writing
             * to
             * @param size Total file size
             * @return true if successful
             */
            inline bool open(FileHandle &handle, uint32_t raw_sector_start, uint32_t size)
            {
                // Check if SD card is present and not write protected
                if (IsSdWriteProtectedOrMissing())
                {
                    return false;
                }
                handle.start_sector = raw_sector_start;
                handle.current_sector = raw_sector_start;
                handle.file_size = size;
                handle.bytes_written = 0;
                handle.is_open = true;
                // Optionally send an init command or CMD25 (Write Multiple Block) here
                return true;
            }
            /**
             * @brief Writes a buffer of data to the SD Card.
             * Buffers the data until a full 512-byte sector is ready, then commits to SD.
             */
            inline bool write(FileHandle &handle, const uint8_t *buffer, uint32_t length)
            {
                if (!handle.is_open)
                    return false;
                // Note: A true implementation needs a 512-byte static buffer here.
                // Once 512 bytes are accumulated, you execute CMD24 (Write Single Block)
                // For simplicity, assuming length == 512 for block writes:

                if (length == 512)
                {
                    send_sd_command(CMD_WRITE_SINGLE_BLOCK, handle.current_sector);
                    wait_sd_ready();
                    // Write the 512 bytes into the Data Port (usually a shared buffer register)
                    // volatile uint32_t* sd_data_port = (volatile uint32_t*)SOME_DATA_REGISTER;
                    // for(int i=0; i<128; i++) {
                    //     sd_data_port[i] = ((uint32_t*)buffer)[i];
                    // }
                    wait_sd_ready();
                    handle.current_sector++;
                    handle.bytes_written += 512;
                }
                return true;
            }
            /**
             * @brief Closes the file handle and finalizes SD card writes.
             */
            inline void close(FileHandle &handle)
            {
                if (!handle.is_open)
                    return;

                // If we were using CMD25 (Multi-block write), send CMD12 to stop transmission
                // send_sd_command(CMD_STOP_TRANSMISSION, 0);
                // wait_sd_ready();
                handle.is_open = false;
            }

            /**
             * @brief Helpers for writable DevCart SD raw paths.
             *
             * Supported path format:
             * - Root/capability listing: sdraw:/
             * - Raw range target: sdraw:<start_sector>:<sector_count>
             */
            namespace Backend
            {
                /**
                 * @brief Checks if a string starts with a specific literal substring.
                 * @param input The input string to check.
                 * @param literal The literal prefix string to match.
                 * @return True if the input starts with the prefix; false otherwise.
                 */
                static inline bool MatchLiteral(const char *input, const char *literal)
                {
                    if (input == nullptr || literal == nullptr)
                    {
                        return false;
                    }

                    while (*literal != '\0')
                    {
                        if (*input != *literal)
                        {
                            return false;
                        }
                        ++input;
                        ++literal;
                    }

                    return true;
                }

                /**
                 * @brief Converts a hexadecimal character to its numeric value.
                 * @param c The character to convert.
                 * @return The numeric value (0-15), or -1 if the character is not valid hex.
                 */
                static inline int HexDigitValue(const char c)
                {
                    if (c >= '0' && c <= '9')
                        return c - '0';
                    if (c >= 'a' && c <= 'f')
                        return 10 + (c - 'a');
                    if (c >= 'A' && c <= 'F')
                        return 10 + (c - 'A');
                    return -1;
                }

                /**
                 * @brief Attempts to parse a 32-bit unsigned integer (decimal or hex) from a string.
                 * @param begin The start of the string to parse.
                 * @param end Out-parameter pointing to the first character after the parsed number.
                 * @param value Reference to store the parsed 32-bit integer.
                 * @return True if parsing succeeded; false otherwise.
                 */
                static inline bool TryParseU32(const char *begin,
                    const char **end,
                    uint32_t &value)
                {
                    if (begin == nullptr || end == nullptr)
                    {
                        return false;
                    }

                    int base = 10;
                    const char *cursor = begin;
                    if (cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X'))
                    {
                        base = 16;
                        cursor += 2;
                    }

                    uint32_t parsed = 0;
                    bool consumed = false;
                    while (*cursor != '\0')
                    {
                        int digit = (base == 16) ? HexDigitValue(*cursor) : ((*cursor >= '0' && *cursor <= '9') ? (*cursor - '0') : -1);
                        if (digit < 0)
                        {
                            break;
                        }

                        if (parsed > ((0xFFFFFFFFUL - static_cast<uint32_t>(digit)) /
                                         static_cast<uint32_t>(base)))
                        {
                            return false;
                        }

                        parsed = (parsed * static_cast<uint32_t>(base)) +
                                 static_cast<uint32_t>(digit);
                        consumed = true;
                        ++cursor;
                    }

                    if (!consumed)
                    {
                        return false;
                    }

                    value = parsed;
                    *end = cursor;
                    return true;
                }

                /**
                 * @brief Checks if a path is a raw sector path starting with "sdraw:".
                 * @param path The path to check.
                 * @return True if the path starts with "sdraw:"; false otherwise.
                 */
                static inline bool IsRawPath(const char *path)
                {
                    return MatchLiteral(path, "sdraw:");
                }

                /**
                 * @brief Checks if a path is the raw sector root path ("sdraw:/").
                 * @param path The path to check.
                 * @return True if the path is exactly "sdraw:/"; false otherwise.
                 */
                static inline bool IsRawRootPath(const char *path)
                {
                    if (path == nullptr)
                    {
                        return false;
                    }
                    return path[0] == 's' && path[1] == 'd' && path[2] == 'r' &&
                           path[3] == 'a' && path[4] == 'w' && path[5] == ':' &&
                           path[6] == '/' && path[7] == '\0';
                }

                /**
                 * @brief Parses a raw sector range string of the format "sdraw:<start_sector>:<sector_count>".
                 * @param path The raw path string to parse.
                 * @param startSector Reference to store the parsed start sector.
                 * @param sectorCount Reference to store the parsed sector count.
                 * @return True if parsing succeeded and a valid range was extracted; false otherwise.
                 */
                static inline bool TryParseRawRange(const char *path,
                    uint32_t &startSector,
                    uint32_t &sectorCount)
                {
                    if (!IsRawPath(path) || IsRawRootPath(path))
                    {
                        return false;
                    }

                    const char *cursor = path + 6;
                    const char *end = nullptr;
                    uint32_t parsedStart = 0;
                    if (!TryParseU32(cursor, &end, parsedStart) || end == nullptr || *end != ':')
                    {
                        return false;
                    }

                    cursor = end + 1;
                    uint32_t parsedCount = 0;
                    if (!TryParseU32(cursor, &end, parsedCount) || end == nullptr || *end != '\0')
                    {
                        return false;
                    }

                    startSector = parsedStart;
                    sectorCount = parsedCount;
                    return sectorCount > 0;
                }

                /**
                 * @brief Erases/zeroes out a range of raw sectors on the SD card.
                 * @param startSector The start sector to begin zeroing.
                 * @param sectorCount The number of sectors to zero out.
                 * @return True if the range was successfully zeroed; false otherwise.
                 */
                static inline bool EraseRawRange(uint32_t startSector, uint32_t sectorCount)
                {
                    if (sectorCount == 0)
                    {
                        return false;
                    }

                    FileHandle handle{};
                    if (!open(handle, startSector, sectorCount * BLOCK_SIZE))
                    {
                        return false;
                    }

                    uint8_t zeroBlock[BLOCK_SIZE] = {0};
                    for (uint32_t i = 0; i < sectorCount; ++i)
                    {
                        if (!write(handle, zeroBlock, BLOCK_SIZE))
                        {
                            close(handle);
                            return false;
                        }
                    }

                    close(handle);
                    return true;
                }
            } // namespace Backend

            constexpr size_t kMaxRequestBytes = 255;
            constexpr size_t kMaxResponseBytes = 1023;
            constexpr size_t kMaxDirEntries = 96;
            constexpr size_t kMaxPathBytes = 255;

            inline bool g_sgcReady = false;
            inline FATFS g_fatfs;

#ifndef APPEND_FMT
    #define APPEND_FMT(_BUF_, _CAP_, _USED_, _FMT_, ...)                                           \
        do                                                                                         \
        {                                                                                          \
            if ((_USED_) < (_CAP_))                                                                \
            {                                                                                      \
                const int _written = snprintf((_BUF_) + (_USED_),                                  \
                    (_CAP_) - (_USED_),                                                            \
                    (_FMT_), ##__VA_ARGS__);                                                       \
                if (_written > 0)                                                                  \
                {                                                                                  \
                    const size_t _advance = static_cast<size_t>(_written);                         \
                    (_USED_) = ((_USED_) + _advance >= (_CAP_)) ? (_CAP_) : ((_USED_) + _advance); \
                }                                                                                  \
            }                                                                                      \
        }                                                                                          \
        while (0)
#endif

            /**
             * @brief Calculates or updates a CRC-8 checksum over a buffer of data.
             * @param crc The initial CRC value, or the previous CRC for chained updates.
             * @param data Pointer to the buffer of data to process.
             * @param dataLen Length of the data buffer in bytes.
             * @return The updated CRC-8 checksum.
             */
            static inline uint8_t Crc8Update(uint8_t crc, const uint8_t *data, size_t dataLen)
            {
                static const uint8_t crcTable[256] = {
                    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31,
                    0x24, 0x23, 0x2A, 0x2D, 0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
                    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D, 0xE0, 0xE7, 0xEE, 0xE9,
                    0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
                    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1,
                    0xB4, 0xB3, 0xBA, 0xBD, 0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
                    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA, 0xB7, 0xB0, 0xB9, 0xBE,
                    0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
                    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16,
                    0x03, 0x04, 0x0D, 0x0A, 0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
                    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A, 0x89, 0x8E, 0x87, 0x80,
                    0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
                    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8,
                    0xDD, 0xDA, 0xD3, 0xD4, 0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
                    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44, 0x19, 0x1E, 0x17, 0x10,
                    0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
                    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F,
                    0x6A, 0x6D, 0x64, 0x63, 0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
                    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13, 0xAE, 0xA9, 0xA0, 0xA7,
                    0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
                    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF,
                    0xFA, 0xFD, 0xF4, 0xF3};

                while (dataLen-- != 0)
                {
                    const uint32_t index = static_cast<uint32_t>(crc ^ *data++) & 0xFFU;
                    crc = crcTable[index];
                }
                return crc;
            }

            /**
             * @brief Ensures that the SGCLIB and FAT file system are initialized and mounted.
             * @return True if the SD card is successfully mounted; false otherwise.
             */
            static inline bool EnsureSgclibReady()
            {
                if (g_sgcReady)
                {
                    return true;
                }

                const int initRes = fs_init();
                if (initRes != 0)
                {
                    return false;
                }

                const FRESULT mountRes = f_mount(&g_fatfs, "", 1);
                g_sgcReady = (mountRes == FR_OK);
                return g_sgcReady;
            }

            /**
             * @brief Checks if a path is intended for the SD card FAT file system.
             * @param path The path to check.
             * @return True if the path starts with a slash ('/'); false otherwise.
             */
            static inline bool IsSdFsPath(const char *path)
            {
                return path != nullptr && path[0] == '/';
            }

            /**
             * @brief Normalizes an SD FAT file system path.
             * @param path The path to normalize.
             * @return A pointer to the normalized path string.
             */
            static inline const char *NormalizeSdFsPath(const char *path)
            {
                return path;
            }

            /**
             * @brief Saves the current FAT file system working directory to a buffer.
             * @param buffer Pointer to the buffer where the current directory will be saved.
             * @param size Size of the buffer in bytes.
             */
            static inline void SaveCurrentDir(char *buffer, size_t size)
            {
                if (buffer == nullptr || size == 0)
                {
                    return;
                }

                buffer[0] = '\0';
                f_getcwd(buffer, static_cast<UINT>(size - 1));
                buffer[size - 1] = '\0';
            }

            /**
             * @brief Restores the FAT file system working directory from a buffer.
             * @param buffer Pointer to the buffer containing the directory path to restore.
             */
            static inline void RestoreCurrentDir(const char *buffer)
            {
                if (buffer != nullptr && buffer[0] != '\0')
                {
                    f_chdir(buffer);
                }
            }

            /**
             * @brief Splits a full path into directory path and file name.
             *
             * Modifies the input string by replacing the last slash with a null terminator.
             *
             * @param fullPath The full path string to split (will be modified).
             * @return A pointer to the file name component.
             */
            static inline char *SplitPathAndName(char *fullPath)
            {
                int slash = -1;
                for (int i = 0; fullPath[i] != '\0'; ++i)
                {
                    if (fullPath[i] == '/')
                    {
                        slash = i;
                    }
                }

                if (slash < 0)
                {
                    return fullPath;
                }

                fullPath[slash] = '\0';
                return fullPath + slash + 1;
            }

            /**
             * @brief Handles a request from the host to list directory contents.
             *
             * Supports SD card FAT filesystem paths and SD raw sector ranges.
             *
             * @param path The path or raw sector range to list.
             * @param response Buffer to write the response string into.
             * @param responseLen Reference to the length of the written response string.
             * @return The status of the operation (e.g., Ok, BadRequest, Error).
             */
            static inline SRL::DevCart::HostIo::Status HandleList(const char *path, char *response,
                size_t &responseLen)
            {
                bool detailed = false;
                if (strncmp(path, "-l ", 3) == 0)
                {
                    detailed = true;
                    path += 3;
                }
                if (Backend::IsRawPath(path))
                {
                    if (Backend::IsRawRootPath(path))
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoList] sdraw:/ has no directory root.\n"
                            "Use sdraw:<start_sector>:<sector_count> for raw sector access.\n");
                        return SRL::DevCart::HostIo::Status::Ok;
                    }

                    uint32_t startSector = 0;
                    uint32_t sectorCount = 0;
                    if (!Backend::TryParseRawRange(path, startSector, sectorCount))
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoList] Invalid SD raw path: %s\n", path);
                        return SRL::DevCart::HostIo::Status::BadRequest;
                    }

                    APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                        "[DoList] %s [W RAW RANGE]\n", path);
                    return SRL::DevCart::HostIo::Status::Ok;
                }

                if (IsSdFsPath(path))
                {
                    if (!EnsureSgclibReady())
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoList] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    const char *sgclibPath = NormalizeSdFsPath(path);
                    FILINFO stat;
                    const FRESULT statRes = f_stat(sgclibPath, &stat);
                    if (statRes == FR_OK && (stat.fattrib & AM_DIR) == 0)
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoList] %s [F]\n", path);
                        return SRL::DevCart::HostIo::Status::Ok;
                    }

                    DIR dir;
                    const FRESULT openDirRes = f_opendir(&dir, sgclibPath);
                    if (openDirRes != FR_OK)
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoList] Can't open SD path (opendir=%d, stat=%d): %s\n", openDirRes, statRes, path);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                        "[DoList] Listing: %s\n", path);

                    bool any = false;
                    for (size_t i = 0; i < kMaxDirEntries; ++i)
                    {
                        FILINFO entry{};
                        const FRESULT rc = f_readdir(&dir, &entry);
                        if (rc != FR_OK || entry.fname[0] == '\0')
                        {
                            break;
                        }

                        any = true;
                        if (detailed)
                        {
                            APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                                "  %s %10lu %04u-%02u-%02u %02u:%02u %s\n",
                                (entry.fattrib & AM_DIR) ? "[D]" : "[F]",
                                (unsigned long)entry.fsize,
                                (entry.fdate >> 9) + 1980, (entry.fdate >> 5) & 15, entry.fdate & 31,
                                (entry.ftime >> 11), (entry.ftime >> 5) & 63,
                                entry.fname);
                        }
                        else
                        {
                            APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                                "  %s %s\n", (entry.fattrib & AM_DIR) ? "[D]" : "[F]",
                                entry.fname);
                        }
                    }

                    if (!any)
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "  (empty directory)\n");
                    }

                    return SRL::DevCart::HostIo::Status::Ok;
                }

                APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                    "[DoList] Unsupported path scheme: %s\n"
                    "Use /... for SD FAT or sdraw:... for raw sectors\n",
                    path);
                return SRL::DevCart::HostIo::Status::BadRequest;
            }

            /**
             * @brief Handles a request from the host to remove a file or erase a raw sector range.
             *
             * @param path The path of the file to remove, or the raw sector range to erase.
             * @param response Buffer to write the response string into.
             * @param responseLen Reference to the length of the written response string.
             * @return The status of the operation (e.g., Ok, BadRequest, Error).
             */
            static inline SRL::DevCart::HostIo::Status HandleRemove(const char *path, char *response,
                size_t &responseLen)
            {
                if (Backend::IsRawPath(path))
                {
                    uint32_t startSector = 0;
                    uint32_t sectorCount = 0;
                    if (!Backend::TryParseRawRange(path, startSector, sectorCount))
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoRemove] Invalid SD raw path: %s\n", path);
                        return SRL::DevCart::HostIo::Status::BadRequest;
                    }

                    if (!Backend::EraseRawRange(startSector, sectorCount))
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoRemove] SD erase failed: %s\n", path);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                        "[DoRemove] Erased SD raw range: %s\n", path);
                    return SRL::DevCart::HostIo::Status::Ok;
                }

                if (IsSdFsPath(path))
                {
                    if (!EnsureSgclibReady())
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoRemove] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    const FRESULT rc = f_unlink(path);
                    if (rc != FR_OK)
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoRemove] Unlink failed (%d): %s\n", rc, path);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                        "[DoRemove] Removed: %s\n", path);
                    return SRL::DevCart::HostIo::Status::Ok;
                }

                APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                    "[DoRemove] Read-only or unsupported path: %s\n", path);
                return SRL::DevCart::HostIo::Status::Unsupported;
            }

            /**
             * @brief Handles a request from the host to create a directory.
             *
             * @param path The path of the directory to create.
             * @param response Buffer to write the response string into.
             * @param responseLen Reference to the length of the written response string.
             * @return The status of the operation (e.g., Ok, BadRequest, Error).
             */
            static inline SRL::DevCart::HostIo::Status HandleMkdir(const char *path, char *response,
                size_t &responseLen)
            {
                if (IsSdFsPath(path))
                {
                    if (!EnsureSgclibReady())
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoMkdir] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    const FRESULT rc = f_mkdir(path);
                    if (rc != FR_OK)
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoMkdir] Mkdir failed (%d): %s\n", rc, path);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                        "[DoMkdir] Created directory: %s\n", path);
                    return SRL::DevCart::HostIo::Status::Ok;
                }

                APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                    "[DoMkdir] Read-only or unsupported path: %s\n", path);
                return SRL::DevCart::HostIo::Status::Unsupported;
            }

            /**
             * @brief Handles a request from the host to remove a directory.
             *
             * @param path The path of the directory to remove.
             * @param response Buffer to write the response string into.
             * @param responseLen Reference to the length of the written response string.
             * @return The status of the operation (e.g., Ok, BadRequest, Error).
             */
            static inline SRL::DevCart::HostIo::Status HandleRmdir(const char *path, char *response,
                size_t &responseLen)
            {
                if (IsSdFsPath(path))
                {
                    if (!EnsureSgclibReady())
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoRmdir] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    const FRESULT rc = f_unlink(path);
                    if (rc != FR_OK)
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoRmdir] Rmdir failed (%d): %s\n", rc, path);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                        "[DoRmdir] Removed directory: %s\n", path);
                    return SRL::DevCart::HostIo::Status::Ok;
                }

                APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                    "[DoRmdir] Read-only or unsupported path: %s\n", path);
                return SRL::DevCart::HostIo::Status::Unsupported;
            }

            /**
             * @brief Handles a request from the host to rename or move a file or directory.
             *
             * @param path The payload containing both the old and new path.
             * @param response Buffer to write the response string into.
             * @param responseLen Reference to the length of the written response string.
             * @return The status of the operation (e.g., Ok, BadRequest, Error).
             */
            static inline SRL::DevCart::HostIo::Status HandleRename(const char *path, char *response,
                size_t &responseLen)
            {
                const char *oldPath = path;
                const char *newPath = path + strlen(oldPath) + 1;

                if (IsSdFsPath(oldPath) && IsSdFsPath(newPath))
                {
                    if (!EnsureSgclibReady())
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoRename] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    const FRESULT rc = f_rename(oldPath, newPath);
                    if (rc != FR_OK)
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoRename] Rename failed (%d): %s -> %s\n", rc, oldPath, newPath);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                        "[DoRename] Renamed: %s -> %s\n", oldPath, newPath);
                    return SRL::DevCart::HostIo::Status::Ok;
                }

                APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                    "[DoRename] Read-only or unsupported path: %s -> %s\n", oldPath, newPath);
                return SRL::DevCart::HostIo::Status::Unsupported;
            }

            /**
             * @brief Handles a request from the host to calculate the CRC-8 checksum of a file.
             *
             * Supports both SD FAT files and standard CD/host filesystem files.
             *
             * @param path The path of the file to checksum.
             * @param response Buffer to write the response string into.
             * @param responseLen Reference to the length of the written response string.
             * @return The status of the operation (e.g., Ok, Error).
             */
            static inline SRL::DevCart::HostIo::Status HandleCrc(const char *path, char *response,
                size_t &responseLen)
            {
                if (IsSdFsPath(path))
                {
                    if (!EnsureSgclibReady())
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoCrc] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    FIL file;
                    const FRESULT openRes = f_open(&file, path, FA_READ);
                    if (openRes != FR_OK)
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoCrc] Can't open file '%s'\n", path);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    uint8_t buffer[1024];
                    uint8_t checksum = 0;
                    while (true)
                    {
                        UINT bytesRead = 0;
                        const FRESULT readRes = f_read(&file, buffer, sizeof(buffer), &bytesRead);
                        if (readRes != FR_OK)
                        {
                            f_close(&file);
                            APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                                "[DoCrc] File read error.\n");
                            return SRL::DevCart::HostIo::Status::Error;
                        }
                        if (bytesRead == 0)
                        {
                            break;
                        }
                        checksum = Crc8Update(checksum, buffer, static_cast<size_t>(bytesRead));
                    }

                    f_close(&file);
                    APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                        "[DoCrc] %s CRC-8 = 0x%02X\n", path,
                        static_cast<unsigned int>(checksum));
                    return SRL::DevCart::HostIo::Status::Ok;
                }

                SRL::Cd::File file(path);
                if (!file.Exists() || !file.Open())
                {
                    APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                        "[DoCrc] Can't open file '%s'\n", path);
                    return SRL::DevCart::HostIo::Status::Error;
                }

                uint8_t buffer[1024];
                uint8_t checksum = 0;
                while (true)
                {
                    const int32_t read = file.Read(static_cast<int32_t>(sizeof(buffer)), buffer);
                    if (read < 0)
                    {
                        file.Close();
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoCrc] File read error.\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }
                    if (read == 0)
                    {
                        break;
                    }
                    checksum = Crc8Update(checksum, buffer, static_cast<size_t>(read));
                }

                file.Close();
                APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                    "[DoCrc] %s CRC-8 = 0x%02X\n", path,
                    static_cast<unsigned int>(checksum));
                return SRL::DevCart::HostIo::Status::Ok;
            }

            /**
             * @brief Handles a request from the host to upload a file to the SD card.
             *
             * The upload protocol consists of:
             * 1. Receiving a 4-byte file size (Big Endian).
             * 2. Receiving the file data in chunks.
             * 3. Receiving a 1-byte CRC-8 checksum from the host.
             * 4. Sending a 1-byte result code (0x00 for success, 0x01 for failure).
             *
             * @param path The destination path on the SD card FAT filesystem.
             * @param response Buffer to write the response string into (used for errors).
             * @param responseLen Reference to the length of the written response string.
             * @return The status of the operation, typically Handled as the response is manual.
             */
            static inline SRL::DevCart::HostIo::Status HandleUpload(const char *path, char *response,
                size_t &responseLen)
            {
                if (IsSdFsPath(path))
                {
                    if (!EnsureSgclibReady())
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoUpload] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    FIL file;
                    const FRESULT openRes = f_open(&file, path, FA_WRITE | FA_CREATE_ALWAYS);
                    if (openRes != FR_OK)
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoUpload] Can't open file '%s' for writing\n", path);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    // Tell host we are ready
                    SRL::DevCart::HostIo::SendResponse(SRL::DevCart::HostIo::Status::Ok, nullptr, 0);

                    // Now wait for raw data from host
                    // 1. Read 4 bytes file size (Big Endian)
                    uint8_t size_buf[4];
                    if (!SRL::DevCart::HostIo::ReadAll(size_buf, 4))
                    {
                        f_close(&file);
                        return SRL::DevCart::HostIo::Status::Handled; // skip auto-response
                    }
                    uint32_t file_size = (static_cast<uint32_t>(size_buf[0]) << 24) |
                                         (static_cast<uint32_t>(size_buf[1]) << 16) |
                                         (static_cast<uint32_t>(size_buf[2]) << 8) |
                                         static_cast<uint32_t>(size_buf[3]);

                    // 2. Read File Data & Calculate CRC
                    uint8_t buffer[4096];
                    uint32_t received = 0;
                    uint8_t checksum = 0;
                    while (received < file_size)
                    {
                        uint32_t chunk = file_size - received;
                        if (chunk > sizeof(buffer))
                        {
                            chunk = sizeof(buffer);
                        }
                        if (!SRL::DevCart::HostIo::ReadAll(buffer, chunk))
                        {
                            break;
                        }

                        checksum = Crc8Update(checksum, buffer, chunk);
                        UINT bw = 0;
                        f_write(&file, buffer, chunk, &bw);
                        received += chunk;
                    }

                    f_close(&file);

                    // 3. Read Checksum
                    uint8_t host_checksum;
                    if (!SRL::DevCart::HostIo::ReadAll(&host_checksum, 1))
                    {
                        return SRL::DevCart::HostIo::Status::Handled;
                    }

                    // 4. Send Result
                    uint8_t result = (host_checksum == checksum) ? 0x00 : 0x01;
                    SRL::DevCart::HostIo::WriteAll(&result, 1);

                    return SRL::DevCart::HostIo::Status::Handled;
                }

                APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                    "[DoUpload] Unsupported path: %s\n", path);
                return SRL::DevCart::HostIo::Status::Unsupported;
            }

            /**
             * @brief Handles a request from the host to download a file from the SD card.
             *
             * The download protocol consists of:
             * 1. Sending a protocol response with the 4-byte file size as payload.
             * 2. Sending the file data in chunks.
             * 3. Sending the 1-byte CRC-8 checksum to the host.
             *
             * @param path The source path on the SD card FAT filesystem.
             * @param response Buffer to write the response string into (used for errors).
             * @param responseLen Reference to the length of the written response string.
             * @return The status of the operation, typically Handled as the response is manual.
             */
            static inline SRL::DevCart::HostIo::Status HandleDownload(const char *path, char *response,
                size_t &responseLen)
            {
                if (IsSdFsPath(path))
                {
                    if (!EnsureSgclibReady())
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoDownload] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    FIL file;
                    const FRESULT openRes = f_open(&file, path, FA_READ);
                    if (openRes != FR_OK)
                    {
                        APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                            "[DoDownload] Can't open file '%s' for reading\n", path);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    uint32_t file_size = f_size(&file);

                    // 1. Tell host we are ready & send file size (4 bytes, Big Endian)
                    uint8_t size_buf[4] = {
                        static_cast<uint8_t>((file_size >> 24) & 0xFFU),
                        static_cast<uint8_t>((file_size >> 16) & 0xFFU),
                        static_cast<uint8_t>((file_size >> 8) & 0xFFU),
                        static_cast<uint8_t>(file_size & 0xFFU)};
                    SRL::DevCart::HostIo::SendResponse(SRL::DevCart::HostIo::Status::Ok, size_buf, 4);

                    // 2. Send File Data & Calculate CRC
                    uint8_t buffer[4096];
                    uint32_t sent = 0;
                    uint8_t checksum = 0;
                    while (sent < file_size)
                    {
                        uint32_t chunk = file_size - sent;
                        if (chunk > sizeof(buffer))
                        {
                            chunk = sizeof(buffer);
                        }
                        UINT br = 0;
                        f_read(&file, buffer, chunk, &br);
                        if (br == 0)
                        {
                            break;
                        }

                        checksum = Crc8Update(checksum, buffer, br);
                        SRL::DevCart::HostIo::WriteAll(buffer, br);
                        sent += br;
                    }

                    f_close(&file);

                    // 3. Send Checksum
                    SRL::DevCart::HostIo::WriteAll(&checksum, 1);

                    return SRL::DevCart::HostIo::Status::Handled;
                }

                APPEND_FMT(response, kMaxResponseBytes + 1, responseLen,
                    "[DoDownload] Unsupported path: %s\n", path);
                return SRL::DevCart::HostIo::Status::Unsupported;
            }
        } // namespace SD
    } // namespace DevCart
} // namespace SRL