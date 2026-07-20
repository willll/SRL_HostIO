#pragma once
#include <cstdint>
#include <srl_devcart.hpp>
#include "sgclib/sdcard.h"

namespace SRL
{
    namespace DevCart
    {
        namespace SD
        {
            /** 
             * @brief SD card block size in bytes.
             * Note: Enforced at 512 bytes by the physical SD card hardware protocol 
             * and FAT filesystem specifications. Do not modify to optimize memory, 
             * as the hardware controller will reject non-512 byte block lengths.
             */
            constexpr uint32_t BLOCK_SIZE = 512;

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
             * @return True if SD is write-protected or missing, false if write-enabled.
             */
            static inline bool IsSdWriteProtectedOrMissing()
            {
                return (CS1::ReadRegister(CS1::Register::RegSdWriteProtect) & 0x01U) != 0;
            }

            /**
             * @brief Returns whether USB data path is expected to be enabled.
             * @return True if USB data path is enabled, false otherwise.
             */
            static inline bool IsUsbDataPathEnabled()
            {
                if (!CS1::IsUsbGamersCartridge())
                {
                    return true;
                }
                return !IsSdWriteProtectedOrMissing();
            }

            /**
             * @brief Checks if an SD card is physically inserted in the reader.
             * @return True if an SD card is inserted, false otherwise.
             */
            inline static bool IsSdCardInserted()
            {
                return (CS1::ReadRegister(CS1::Register::CpldIo) & (1U << static_cast<uint8_t>(SD_LSHFT::SD_EJECT_LSHFT))) == 0;
            }

            /**
             * @brief Checks if the SD card was reinserted since the last check/reset.
             * @return True if the SD card was reinserted, false otherwise.
             */
            inline static bool IsSdCardReinserted()
            {
                return (CS1::ReadRegister(CS1::Register::RegSdReinsert) & 0x01U) != 0;
            }

            /**
             * @brief Clears/resets the SD card reinsertion flag.
             */
            inline static void ClearSdCardReinsertFlag()
            {
                *(volatile uint8_t *)(static_cast<uint32_t>(CS1::Register::RegSdReinsert)) = 0x01U;
            }

            /**
             * @brief Sets the state of the SD card reader green LED.
             * @param[in] on True to turn on the green LED, false to turn it off.
             */
            inline static void SetLedGreen(bool on)
            {
                sdc_ledset(SDC_LED_GREEN, on ? SDC_LED_ON : SDC_LED_OFF);
            }

            /**
             * @brief Sets the state of the SD card reader red LED.
             * @param[in] on True to turn on the red LED, false to turn it off.
             */
            inline static void SetLedRed(bool on)
            {
                sdc_ledset(SDC_LED_RED, on ? SDC_LED_ON : SDC_LED_OFF);
            }

            /**
             * @brief Turns off both the green and red LEDs on the SD card reader.
             */
            inline static void TurnOffLeds()
            {
                SetLedGreen(false);
                SetLedRed(false);
            }




            /**
             * @brief Opens a "file" on the SD Card.
             * Since we aren't using a FAT filesystem, we write to raw sectors.
             * @param[in,out] handle Reference to the FileHandle to initialize.
             * @param[in] raw_sector_start The absolute sector on the SD card to start writing to.
             * @param[in] size Total file size in bytes.
             * @return True if the open operation succeeded, false otherwise.
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
                return true;
            }

            /**
             * @brief Writes a buffer of data to the SD Card.
             * Writes blocks of 512 bytes to raw sectors of the SD Card.
             * @param[in,out] handle Reference to the FileHandle to update.
             * @param[in] buffer Pointer to the source data buffer to write.
             * @param[in] length Number of bytes to write (must be a multiple of 512).
             * @return True if the write operation succeeded, false otherwise.
             */
            inline bool write(FileHandle &handle, const uint8_t *buffer, uint32_t length)
            {
                if (!handle.is_open)
                    return false;

                if (length == 0 || (length % 512) != 0)
                {
                    return false;
                }

                const uint32_t blocks_count = length / 512;
                const unsigned char res = sdc_write_multiple_blocks(
                    handle.current_sector,
                    const_cast<uint8_t *>(buffer),
                    blocks_count
                );

                if (res != 0)
                {
                    return false;
                }

                handle.current_sector += blocks_count;
                handle.bytes_written += length;
                return true;
            }

            /**
             * @brief Closes the file handle and finalizes SD card writes.
             * @param[in,out] handle Reference to the FileHandle to close.
             */
            inline void close(FileHandle &handle)
            {
                if (!handle.is_open)
                    return;

                handle.is_open = false;
                TurnOffLeds();
            }

        } // namespace SD
    } // namespace DevCart
} // namespace SRL
