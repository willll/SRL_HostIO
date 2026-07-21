#pragma once
#include <srl.hpp>
#include <srl_devcart.hpp>
#include "srl_devcart_hostio.hpp"

/**
 * @file srl_devcart_sdcard.hpp
 * @brief High-level SD card FAT filesystem interface and HostIO command handlers.
 *
 * Main aggregator header for SD card operations. Includes sub-modules:
 *  - srl_devcart_sdcard_crc.hpp: CRC-8 calculation.
 *  - srl_devcart_sdcard_utilities.hpp: String and hex parsing helpers.
 *  - srl_devcart_sdcard_lowlevel.hpp: Raw SD hardware & sector functions.
 *  - srl_devcart_sdcard_backend.hpp: Raw path parsing and AppendFmt templates.
 *  - srl_devcart_sdcard_fatfs.hpp: FatFS C++ wrappers (File, Directory, FatFsBackend).
 *  - srl_devcart_sdcard_handlers.hpp: HostIO command handlers (HandleList, HandleUpload, etc.).
 */

#include "sgclib/sgclib.h"
#include "fatfs/ff.h"

#include <cstdint>
#include <string.h>
#include <stdio.h>
#include <initializer_list>

#include "srl_devcart_sdcard_crc.hpp"
#include "srl_devcart_sdcard_utilities.hpp"
#include "srl_devcart_sdcard_lowlevel.hpp"
#include "srl_devcart_sdcard_backend.hpp"
#include "srl_devcart_sdcard_fatfs.hpp"

namespace SRL
{
    namespace DevCart
    {
        namespace SD
        {
            /**
             * @brief Initializes the SGCLIB FAT filesystem driver.
             * @return SGC_FR_OK (0) on success, or a non-zero SGCLIB error code on failure.
             */
            static inline int fs_init()
            {
                return sgc_init();
            }

            /**
             * @brief Retrieves the current working directory path.
             * @param[out] buff Buffer to receive the null-terminated path string.
             * @param[in] buflen Size of @p buff in bytes, including null terminator.
             * @return SGC_FR_OK (0) on success, or a non-zero SGCLIB error code on failure.
             */
            static inline int fs_getcwd(char *buff, int buflen)
            {
                return sgc_getcwd(buff, buflen);
            }

            /**
             * @brief Changes the current working directory.
             * @param[in] path Null-terminated path of directory to change into.
             * @return SGC_FR_OK (0) on success, or a non-zero SGCLIB error code on failure.
             */
            static inline int fs_chdir(const char *path)
            {
                return sgc_chdir(path);
            }

            /**
             * @brief Opens a file on the SD card FAT filesystem.
             * @param[in] filename Null-terminated path to file to open.
             * @param[in] flags Open mode flags.
             * @return A non-negative file descriptor on success, or -1 on error.
             */
            static inline int fs_open(const char *filename, int flags)
            {
                return sgc_open(filename, flags);
            }

            /**
             * @brief Closes an open file descriptor.
             * @param[in] fd The file descriptor to close.
             * @return SGC_FR_OK (0) on success, or a non-zero SGCLIB error code on failure.
             */
            static inline int fs_close(int fd)
            {
                return sgc_close(fd);
            }

            /**
             * @brief Reads data from an open file.
             * @param[in] fd File descriptor to read from.
             * @param[out] buf Destination buffer.
             * @param[in] len Maximum number of bytes to read.
             * @return Bytes read on success, 0 at EOF, negative value on error.
             */
            static inline int fs_read(int fd, void *buf, int len)
            {
                return sgc_read(fd, buf, len);
            }

            /**
             * @brief Writes data to an open file.
             * @param[in] fd File descriptor to write to.
             * @param[in] buf Source buffer.
             * @param[in] len Number of bytes to write.
             * @return Bytes written on success, negative value on error.
             */
            static inline int fs_write(int fd, const void *buf, int len)
            {
                return sgc_write(fd, buf, len);
            }

            /**
             * @brief Retrieves metadata for a file or directory.
             * @param[in] filename Null-terminated path to file/directory.
             * @param[out] stat Pointer to sgc_stat_t structure to populate.
             * @return SGC_FR_OK (0) on success, non-zero error code on failure.
             */
            static inline int fs_stat(const char *filename, sgc_stat_t *stat)
            {
                return sgc_stat(filename, stat, static_cast<int>(sizeof(sgc_stat_t)));
            }

            /**
             * @brief Opens a directory for iteration.
             * @param[in] path Null-terminated path to directory to open.
             * @return SGC_FR_OK (0) on success, non-zero error code on failure.
             */
            static inline int fs_opendir(const char *path)
            {
                return sgc_opendir(path);
            }

            /**
             * @brief Removes a file from the SD card FAT filesystem.
             * @param[in] path Null-terminated path to file to remove.
             * @return SGC_FR_OK (0) on success, non-zero error code on failure.
             */
            static inline int fs_unlink(const char *path)
            {
                return sgc_unlink(path);
            }

            /**
             * @brief Reads the next entry from a previously opened directory.
             * @param[out] entry Pointer to FILINFO structure to populate.
             * @return FR_OK (0) on success, non-zero FatFS FRESULT error code on failure.
             */
            static inline int fs_readdir(FILINFO *entry)
            {
                return f_readdir(&dir, entry);
            }

            /** @brief Legacy global FatFS filesystem object for backwards compatibility. */
            inline FATFS g_fatfs;

        } // namespace SD
    } // namespace DevCart
} // namespace SRL

#include "srl_devcart_sdcard_handlers.hpp"