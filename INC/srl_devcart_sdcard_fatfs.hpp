/**
 * @file srl_devcart_sdcard_fatfs.hpp
 * @brief FatFS backend abstraction for the SRL_HostIO SD card handler layer.
 *
 * Provides the C++ types consumed by srl_devcart_sdcard_handlers.hpp:
 *  - @ref SRL::DevCart::SD::FileMode — open-mode enum (Read / Write).
 *  - @ref SRL::DevCart::SD::DirectoryEntry — portable directory entry.
 *  - @ref SRL::DevCart::SD::File — RAII file wrapper around FatFS FIL.
 *  - @ref SRL::DevCart::SD::Directory — RAII directory wrapper around FatFS DIR.
 *  - @ref SRL::DevCart::SD::FatFsBackend — filesystem operations backend.
 *  - @ref SRL::DevCart::SD::g_fatfsBackend — module-level backend singleton.
 *
 * Include this header before srl_devcart_sdcard_handlers.hpp (or rely on
 * srl_devcart_sdcard.hpp which includes both in the correct order).
 */
#pragma once
#include <cstdint>
#include <cstddef>
#include "fatfs/ff.h"

namespace SRL
{
    namespace DevCart
    {
        namespace SD
        {
            // ----------------------------------------------------------------
            // FileMode
            // ----------------------------------------------------------------

            /**
             * @brief Open mode for SD card FAT filesystem files.
             */
            enum class FileMode : uint8_t
            {
                Read,  /**< Open an existing file for reading (FA_READ). */
                Write, /**< Create or truncate a file for writing (FA_WRITE | FA_CREATE_ALWAYS). */
            };

            // ----------------------------------------------------------------
            // DirectoryEntry
            // ----------------------------------------------------------------

            /** @brief Maximum length of a FatFS short/long file name (LFN). */
            static constexpr size_t kMaxNameLength = FF_LFN_BUF + 1;

            /**
             * @brief Portable directory entry populated by Directory::Read().
             */
            struct DirectoryEntry
            {
                char     Name[kMaxNameLength]; /**< Null-terminated file or directory name. */
                uint32_t Size;                 /**< File size in bytes (0 for directories). */
                uint16_t Date;                 /**< FatFS packed date: bits[15:9]=year-1980, [8:5]=month, [4:0]=day. */
                uint16_t Time;                 /**< FatFS packed time: bits[15:11]=hour, [10:5]=min, [4:0]=sec/2. */
                bool     IsDirectory;          /**< True if the entry is a directory. */
            };

            // ----------------------------------------------------------------
            // File
            // ----------------------------------------------------------------

            /**
             * @brief RAII wrapper around a FatFS @c FIL object.
             *
             * Provides read, write, size query, and explicit close operations.
             * The file is not opened on construction; call Open() first.
             */
            class File
            {
            public:
                File() : m_isOpen(false) {}

                ~File()
                {
                    if (m_isOpen)
                    {
                        f_close(&m_fil);
                    }
                }

                /**
                 * @brief Opens a file on the SD card FAT filesystem.
                 * @param[in] path Null-terminated path to the file.
                 * @param[in] mode FileMode::Read or FileMode::Write.
                 * @return True on success; false if the file could not be opened.
                 */
                bool Open(const char *path, FileMode mode)
                {
                    const BYTE flags = (mode == FileMode::Write)
                        ? (FA_WRITE | FA_CREATE_ALWAYS)
                        : FA_READ;
                    m_isOpen = (f_open(&m_fil, path, flags) == FR_OK);
                    return m_isOpen;
                }

                /**
                 * @brief Reads bytes from the open file.
                 * @param[out] buf      Destination buffer.
                 * @param[in]  len      Maximum bytes to read.
                 * @param[out] bytesRead Actual bytes read; 0 signals EOF.
                 * @return True on success (including EOF); false on read error.
                 */
                bool Read(void *buf, size_t len, size_t &bytesRead)
                {
                    UINT br = 0;
                    const FRESULT rc = f_read(&m_fil, buf, static_cast<UINT>(len), &br);
                    bytesRead = static_cast<size_t>(br);
                    return rc == FR_OK;
                }

                /**
                 * @brief Writes bytes to the open file.
                 * @param[in]  buf          Source buffer.
                 * @param[in]  len          Number of bytes to write.
                 * @param[out] bytesWritten Actual bytes written.
                 * @return True on success; false on write error.
                 */
                bool Write(const void *buf, size_t len, size_t &bytesWritten)
                {
                    UINT bw = 0;
                    const FRESULT rc = f_write(&m_fil, buf, static_cast<UINT>(len), &bw);
                    bytesWritten = static_cast<size_t>(bw);
                    return rc == FR_OK;
                }

                /**
                 * @brief Returns the total size of the open file in bytes.
                 * @return File size, or 0 if the file is not open.
                 */
                size_t Size() const
                {
                    return m_isOpen ? static_cast<size_t>(f_size(&m_fil)) : 0u;
                }

                /**
                 * @brief Moves the file read/write pointer to an absolute offset from the start of the file.
                 * @param[in] offset Absolute byte offset.
                 * @return True on success; false on failure or if file is not open.
                 */
                bool Seek(size_t offset)
                {
                    if (!m_isOpen)
                    {
                        return false;
                    }
                    return f_lseek(&m_fil, static_cast<FSIZE_t>(offset)) == FR_OK;
                }

                /**
                 * @brief Closes the file and releases the underlying FatFS handle.
                 */
                void Close()
                {
                    if (m_isOpen)
                    {
                        f_close(&m_fil);
                        m_isOpen = false;
                    }
                }

            private:
                FIL  m_fil;
                bool m_isOpen;
            };

            // ----------------------------------------------------------------
            // Directory
            // ----------------------------------------------------------------

            /**
             * @brief RAII wrapper around a FatFS @c DIR object.
             *
             * Iterates directory entries via repeated Read() calls until Read()
             * returns false (end-of-directory or error).
             */
            class Directory
            {
            public:
                Directory() : m_isOpen(false) {}

                ~Directory()
                {
                    if (m_isOpen)
                    {
                        f_closedir(&m_dir);
                    }
                }

                /**
                 * @brief Opens a directory for iteration.
                 * @param[in] path Null-terminated directory path.
                 * @return True on success; false if the directory cannot be opened.
                 */
                bool Open(const char *path)
                {
                    m_isOpen = (f_opendir(&m_dir, path) == FR_OK);
                    return m_isOpen;
                }

                /**
                 * @brief Reads the next entry from the open directory.
                 * @param[out] entry Receives the name, size, date/time, and type flag.
                 * @return True if an entry was read; false at end-of-directory or on error.
                 */
                bool Read(DirectoryEntry &entry)
                {
                    FILINFO info{};
                    const FRESULT rc = f_readdir(&m_dir, &info);
                    if (rc != FR_OK || info.fname[0] == '\0')
                    {
                        return false;
                    }

                    size_t i = 0;
                    while (i < kMaxNameLength - 1 && info.fname[i] != '\0')
                    {
                        entry.Name[i] = info.fname[i];
                        ++i;
                    }
                    entry.Name[i]    = '\0';
                    entry.Size        = static_cast<uint32_t>(info.fsize);
                    entry.Date        = info.fdate;
                    entry.Time        = info.ftime;
                    entry.IsDirectory = (info.fattrib & AM_DIR) != 0;
                    return true;
                }

                /**
                 * @brief Closes the directory handle.
                 */
                void Close()
                {
                    if (m_isOpen)
                    {
                        f_closedir(&m_dir);
                        m_isOpen = false;
                    }
                }

            private:
                DIR  m_dir;
                bool m_isOpen;
            };

            // ----------------------------------------------------------------
            // FatFsBackend
            // ----------------------------------------------------------------

            /**
             * @brief Module-level FatFS filesystem backend used by the handler layer.
             *
             * Wraps the FatFS volume object and exposes high-level operations
             * (mount, stat, remove, mkdir, rename, cwd) that the
             * srl_devcart_sdcard_handlers.hpp handler functions call through
             * the @ref g_fatfsBackend singleton.
             */
            class FatFsBackend
            {
            public:
                FatFsBackend() : m_mounted(false) {}

                /**
                 * @brief Ensures the FAT volume is mounted.
                 *
                 * Mounts the default volume ("") with immediate mount if not
                 * already mounted.
                 *
                 * @return True if the volume is mounted; false on failure.
                 */
                bool EnsureReady()
                {
                    if (m_mounted)
                    {
                        return true;
                    }
                    m_mounted = (f_mount(&m_fatfs, "", 1) == FR_OK);
                    return m_mounted;
                }

                /**
                 * @brief Retrieves the current working directory.
                 * @param[out] buf  Buffer to receive the null-terminated path.
                 * @param[in]  size Buffer capacity in bytes.
                 * @return True on success; false on failure.
                 */
                bool GetCurrentDirectory(char *buf, size_t size)
                {
                    return f_getcwd(buf, static_cast<UINT>(size)) == FR_OK;
                }

                /**
                 * @brief Changes the current working directory.
                 * @param[in] path Null-terminated destination path.
                 * @return True on success; false on failure.
                 */
                bool ChangeDirectory(const char *path)
                {
                    return f_chdir(path) == FR_OK;
                }

                /**
                 * @brief Retrieves metadata for a file or directory.
                 * @param[in]  path  Null-terminated path.
                 * @param[out] entry Receives name, size, date/time, and type flag.
                 * @return True if the path exists; false otherwise.
                 */
                bool Stat(const char *path, DirectoryEntry &entry)
                {
                    FILINFO info{};
                    if (f_stat(path, &info) != FR_OK)
                    {
                        return false;
                    }

                    const char *src = info.fname;
                    size_t i = 0;
                    while (i < kMaxNameLength - 1 && src[i] != '\0')
                    {
                        entry.Name[i] = src[i];
                        ++i;
                    }
                    entry.Name[i]    = '\0';
                    entry.Size        = static_cast<uint32_t>(info.fsize);
                    entry.Date        = info.fdate;
                    entry.Time        = info.ftime;
                    entry.IsDirectory = (info.fattrib & AM_DIR) != 0;
                    return true;
                }

                /**
                 * @brief Removes a file or empty directory.
                 * @param[in] path Null-terminated path to remove.
                 * @return True on success; false on failure.
                 */
                bool Remove(const char *path)
                {
                    return f_unlink(path) == FR_OK;
                }

                /**
                 * @brief Creates a directory (and intermediate directories are
                 *        not created — FatFS does not support recursive mkdir).
                 * @param[in] path Null-terminated path of the new directory.
                 * @return True on success; false on failure.
                 */
                bool MakeDirectory(const char *path)
                {
                    return f_mkdir(path) == FR_OK;
                }

                /**
                 * @brief Renames or moves a file or directory.
                 * @param[in] oldPath Null-terminated current path.
                 * @param[in] newPath Null-terminated destination path.
                 * @return True on success; false on failure.
                 */
                bool Rename(const char *oldPath, const char *newPath)
                {
                    return f_rename(oldPath, newPath) == FR_OK;
                }

            private:
                FATFS m_fatfs;    /**< FatFS volume workspace. */
                bool  m_mounted;  /**< True after EnsureReady() succeeds. */
            };

            // ----------------------------------------------------------------
            // Module-level singleton
            // ----------------------------------------------------------------

            /**
             * @brief Module-level FatFS backend instance used by the handler layer.
             *
             * Defined as @c inline so the linker produces exactly one definition
             * across all translation units that include this header.
             */
            inline FatFsBackend g_fatfsBackend;

        } // namespace SD
    } // namespace DevCart
} // namespace SRL
