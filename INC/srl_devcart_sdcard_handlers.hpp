#pragma once
#include <cstdint>
#include <cstddef>
#include <string.h>
#include <stdio.h>
#include <srl.hpp>
#include <srl_endian.hpp>
#include "srl_devcart_hostio.hpp"
#include "srl_devcart_sdcard_crc.hpp"
#include "srl_devcart_sdcard_backend.hpp"
#include "srl_devcart_sdcard_fatfs.hpp"

namespace SRL
{
    namespace DevCart
    {
        namespace SD
        {
            constexpr size_t kMaxRequestBytes = 255;
            constexpr size_t kMaxResponseBytes = 1023;
            constexpr size_t kMaxDirEntries = 96;
            constexpr size_t kMaxPathBytes = 255;
            constexpr size_t kFileBufferSize = 1024;

            inline bool g_sgcReady = false;

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

                g_sgcReady = g_fatfsBackend.EnsureReady();
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
             * @brief Saves the current FAT file system working directory to a buffer.
             * @param buffer Pointer to the buffer where the current directory will be saved.
             * @param size Size of the buffer in bytes.
             * @return True if the current directory was successfully retrieved; false otherwise.
             */
            static inline bool SaveCurrentDir(char *buffer, size_t size)
            {
                if (buffer == nullptr || size == 0)
                {
                    return false;
                }

                buffer[0] = '\0';
                const bool success = g_fatfsBackend.GetCurrentDirectory(buffer, size);
                buffer[size - 1] = '\0';
                return success;
            }

            /**
             * @brief Restores the FAT file system working directory from a buffer.
             * @param buffer Pointer to the buffer containing the directory path to restore.
             * @return True if the directory was successfully restored or no restore was needed; false otherwise.
             */
            static inline bool RestoreCurrentDir(const char *buffer)
            {
                if (buffer != nullptr && buffer[0] != '\0')
                {
                    return g_fatfsBackend.ChangeDirectory(buffer);
                }
                return true;
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
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoList] sdraw:/ has no directory root.\n"
                            "Use sdraw:<start_sector>:<sector_count> for raw sector access.\n");
                        return SRL::DevCart::HostIo::Status::Ok;
                    }

                    uint32_t startSector = 0;
                    uint32_t sectorCount = 0;
                    if (!Backend::TryParseRawRange(path, startSector, sectorCount))
                    {
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoList] Invalid SD raw path: %s\n", path);
                        return SRL::DevCart::HostIo::Status::BadRequest;
                    }

                    Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                        "[DoList] %s [W RAW RANGE]\n", path);
                    return SRL::DevCart::HostIo::Status::Ok;
                }

                if (IsSdFsPath(path))
                {
                    if (!EnsureSgclibReady())
                    {
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoList] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    DirectoryEntry stat{};
                    const bool statOk = g_fatfsBackend.Stat(path, stat);
                    if (statOk && !stat.IsDirectory)
                    {
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoList] %s [F]\n", path);
                        return SRL::DevCart::HostIo::Status::Ok;
                    }

                    Directory dir;
                    const bool openDirRes = dir.Open(path);
                    if (!openDirRes)
                    {
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoList] Can't open SD path: %s\n", path);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                        "[DoList] Listing: %s\n", path);

                    bool any = false;
                    for (size_t i = 0; i < kMaxDirEntries; ++i)
                    {
                        DirectoryEntry entry{};
                        const bool rc = dir.Read(entry);
                        if (!rc)
                        {
                            break;
                        }

                        any = true;
                        if (detailed)
                        {
                            Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                                "  %s %10lu %04u-%02u-%02u %02u:%02u %s\n",
                                entry.IsDirectory ? "[D]" : "[F]",
                                (unsigned long)entry.Size,
                                (entry.Date >> 9) + 1980, (entry.Date >> 5) & 15, entry.Date & 31,
                                (entry.Time >> 11), (entry.Time >> 5) & 63,
                                entry.Name);
                        }
                        else
                        {
                            Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                                "  %s %s\n", entry.IsDirectory ? "[D]" : "[F]",
                                entry.Name);
                        }
                    }

                    if (!any)
                    {
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "  (empty directory)\n");
                    }

                    return SRL::DevCart::HostIo::Status::Ok;
                }

                Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
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
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoRemove] Invalid SD raw path: %s\n", path);
                        return SRL::DevCart::HostIo::Status::BadRequest;
                    }

                    if (!Backend::EraseRawRange(startSector, sectorCount))
                    {
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoRemove] SD erase failed: %s\n", path);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                        "[DoRemove] Erased SD raw range: %s\n", path);
                    return SRL::DevCart::HostIo::Status::Ok;
                }

                if (IsSdFsPath(path))
                {
                    if (!EnsureSgclibReady())
                    {
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoRemove] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    const bool rc = g_fatfsBackend.Remove(path);
                    if (!rc)
                    {
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoRemove] Unlink failed: %s\n", path);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                        "[DoRemove] Removed: %s\n", path);
                    return SRL::DevCart::HostIo::Status::Ok;
                }

                Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
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
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoMkdir] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    const bool rc = g_fatfsBackend.MakeDirectory(path);
                    if (!rc)
                    {
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoMkdir] Mkdir failed: %s\n", path);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                        "[DoMkdir] Created directory: %s\n", path);
                    return SRL::DevCart::HostIo::Status::Ok;
                }

                Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
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
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoRmdir] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    const bool rc = g_fatfsBackend.Remove(path);
                    if (!rc)
                    {
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoRmdir] Rmdir failed: %s\n", path);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                        "[DoRmdir] Removed directory: %s\n", path);
                    return SRL::DevCart::HostIo::Status::Ok;
                }

                Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
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
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoRename] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    const bool rc = g_fatfsBackend.Rename(oldPath, newPath);
                    if (!rc)
                    {
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoRename] Rename failed: %s -> %s\n", oldPath, newPath);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                        "[DoRename] Renamed: %s -> %s\n", oldPath, newPath);
                    return SRL::DevCart::HostIo::Status::Ok;
                }

                Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
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
                uint8_t buffer[kFileBufferSize];
                uint8_t checksum = 0;
                if (IsSdFsPath(path))
                {
                    if (!EnsureSgclibReady())
                    {
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoCrc] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    File file;
                    if (!file.Open(path, FileMode::Read))
                    {
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoCrc] Can't open file '%s'\n", path);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    while (true)
                    {
                        size_t bytesRead = 0;
                        if (!file.Read(buffer, sizeof(buffer), bytesRead))
                        {
                            file.Close();
                            Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                                "[DoCrc] File read error.\n");
                            return SRL::DevCart::HostIo::Status::Error;
                        }
                        if (bytesRead == 0)
                        {
                            break;
                        }
                        checksum = Crc8Update(checksum, buffer, bytesRead);
                    }

                    file.Close();
                    Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                        "[DoCrc] %s CRC-8 = 0x%02X\n", path,
                        static_cast<unsigned int>(checksum));
                    return SRL::DevCart::HostIo::Status::Ok;
                }

                SRL::Cd::File file(path);
                if (!file.Exists() || !file.Open())
                {
                    Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                        "[DoCrc] Can't open file '%s'\n", path);
                    return SRL::DevCart::HostIo::Status::Error;
                }

                while (true)
                {
                    const int32_t read = file.Read(static_cast<int32_t>(sizeof(buffer)), buffer);
                    if (read < 0)
                    {
                        file.Close();
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
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
                Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
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
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoUpload] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    File file;
                    if (!file.Open(path, FileMode::Write))
                    {
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
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
                        file.Close();
                        return SRL::DevCart::HostIo::Status::Handled; // skip auto-response
                    }
                    uint8_t size_le[4] = {size_buf[3], size_buf[2], size_buf[1], size_buf[0]};
                    uint32_t file_size = SRL::Endian::DeserializeUint32(size_le);

                    // 2. Read File Data & Calculate CRC
                    uint8_t buffer[kFileBufferSize];
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
                        size_t bw = 0;
                        file.Write(buffer, chunk, bw);
                        received += chunk;
                    }

                    file.Close();

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

                Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
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
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoDownload] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    File file;
                    if (!file.Open(path, FileMode::Read))
                    {
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoDownload] Can't open file '%s' for reading\n", path);
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    uint32_t file_size = static_cast<uint32_t>(file.Size());

                    // 1. Tell host we are ready & send file size (4 bytes, Big Endian)
                    uint8_t size_buf[4] = {
                        static_cast<uint8_t>((file_size >> 24) & 0xFFU),
                        static_cast<uint8_t>((file_size >> 16) & 0xFFU),
                        static_cast<uint8_t>((file_size >> 8) & 0xFFU),
                        static_cast<uint8_t>(file_size & 0xFFU)};
                    SRL::DevCart::HostIo::SendResponse(SRL::DevCart::HostIo::Status::Ok, size_buf, 4);

                    // 2. Send File Data & Calculate CRC
                    uint8_t buffer[kFileBufferSize];
                    uint32_t sent = 0;
                    uint8_t checksum = 0;
                    while (sent < file_size)
                    {
                        uint32_t chunk = file_size - sent;
                        if (chunk > sizeof(buffer))
                        {
                            chunk = sizeof(buffer);
                        }
                        size_t br = 0;
                        if (!file.Read(buffer, chunk, br))
                        {
                            break;
                        }
                        if (br == 0)
                        {
                            break;
                        }

                        checksum = Crc8Update(checksum, buffer, br);
                        SRL::DevCart::HostIo::WriteAll(buffer, br);
                        sent += br;
                    }

                    file.Close();

                    // 3. Send Checksum
                    SRL::DevCart::HostIo::WriteAll(&checksum, 1);

                    return SRL::DevCart::HostIo::Status::Handled;
                }

                Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                    "[DoDownload] Unsupported path: %s\n", path);
                return SRL::DevCart::HostIo::Status::Unsupported;
            }

            /**
             * @brief Handles a request from the host to synchronize a directory.
             *
             * @param path The path of the directory to synchronize.
             * @param response Buffer to write the response string into.
             * @param responseLen Reference to the length of the written response string.
             * @return The status of the operation (e.g., Ok, Error).
             */
            static inline SRL::DevCart::HostIo::Status HandleSync(const char *path, char *response,
                size_t &responseLen)
            {
                if (IsSdFsPath(path))
                {
                    if (!EnsureSgclibReady())
                    {
                        Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                            "[DoSync] SGCLIB init failed\n");
                        return SRL::DevCart::HostIo::Status::Error;
                    }

                    // Ensure target folder exists or is ready for sync
                    g_fatfsBackend.MakeDirectory(path);

                    Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                        "[DoSync] Synchronizing directory: %s\n", path);
                    return SRL::DevCart::HostIo::Status::Ok;
                }

                Backend::AppendFmt(response, kMaxResponseBytes + 1, responseLen,
                    "[DoSync] Read-only or unsupported path: %s\n", path);
                return SRL::DevCart::HostIo::Status::Unsupported;
            }
        } // namespace SD
    } // namespace DevCart
} // namespace SRL
