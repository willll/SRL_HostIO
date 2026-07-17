#pragma once
#include <srl_devcart.hpp>

#include "sgclib/sgclib.h"
#include "fatfs/ff.h"

#include <cstdint> // For uintptr_t, size_t, uint8_t, uint32_t
#include <string.h>
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
        } // namespace SD
    } // namespace DevCart
} // namespace SRL