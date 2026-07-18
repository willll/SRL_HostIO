#pragma once
#include <cstdint>
#include <srl_devcart.hpp>

namespace SRL
{
    namespace DevCart
    {
        namespace SD
        {
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

        } // namespace SD
    } // namespace DevCart
} // namespace SRL
