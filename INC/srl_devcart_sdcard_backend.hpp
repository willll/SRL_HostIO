#pragma once
#include <cstdint>
#include <cstddef>
#include <stdio.h>
#include <utility>
#include "srl_devcart_sdcard_utilities.hpp"
#include "srl_devcart_sdcard_lowlevel.hpp"

namespace SRL
{
    namespace DevCart
    {
        namespace SD
        {
            /**
             * @brief Helpers for writable DevCart SD raw paths.
             *
             * Supported path format:
             * - Root/capability listing: sdraw:/
             * - Raw range target: sdraw:<start_sector>:<sector_count>
             */
            namespace Backend
            {
                using namespace SRL::DevCart::SD::Utilities;

                /** @brief The prefix string used for raw SD card paths. */
                constexpr const char kRawPathPrefix[] = "sdraw:";
                /** @brief The size of the raw path prefix string (excluding null terminator). */
                constexpr size_t kRawPathPrefixSize = sizeof(kRawPathPrefix) - 1;

                /** @brief Appends a formatted string to a buffer, updating the length and preventing overflow.
                 *  @tparam Args Types of the formatting arguments.
                 *  @param buf The target destination string buffer.
                 *  @param cap The total capacity of the destination buffer.
                 *  @param used Reference to the current number of bytes already written.
                 *  @param fmt The format string.
                 *  @param args The formatting arguments to interpolate.
                 */
                template <typename... Args>
                static inline void AppendFmt(char *buf, size_t cap, size_t &used, const char *fmt, Args &&...args)
                {
                    if (used < cap)
                    {
                        const int written = snprintf(buf + used, cap - used, fmt, std::forward<Args>(args)...);
                        if (written > 0)
                        {
                            const size_t advance = static_cast<size_t>(written);
                            used = (used + advance >= cap) ? cap : (used + advance);
                        }
                    }
                }

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
                 * @brief Checks if a path is a raw sector path starting with "sdraw:".
                 * @param path The path to check.
                 * @return True if the path starts with "sdraw:"; false otherwise.
                 */
                static inline bool IsRawPath(const char *path)
                {
                    return MatchLiteral(path, kRawPathPrefix);
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
                    return MatchLiteral(path, kRawPathPrefix) &&
                           path[kRawPathPrefixSize] == '/' &&
                           path[kRawPathPrefixSize + 1] == '\0';
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

                    // Skip the "sdraw:" prefix
                    const char *cursor = path + kRawPathPrefixSize;
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

