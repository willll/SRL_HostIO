#pragma once
#include <cstdint>

/**
 * @file srl_devcart_sdcard_utilities.hpp
 * @brief Common helper utilities for parsing and string processing in the DevCart SD module.
 */

namespace SRL
{
    namespace DevCart
    {
        namespace SD
        {
            /**
             * @brief Namespace containing common parsing and utility functions.
             */
            namespace Utilities
            {
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
            } // namespace Utilities
        } // namespace SD
    } // namespace DevCart
} // namespace SRL
