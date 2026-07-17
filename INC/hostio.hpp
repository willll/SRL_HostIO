// Based on SatCom Library by cafe-alpha, Original:
// http://ppcenter.free.fr/satcom/

#pragma once
#include <cstddef> // For size_t
#include <cstdint> // For uintptr_t, size_t, uint8_t, uint32_t
#include <initializer_list>
#include <srl_devcart.hpp>

/**
 * @brief Namespace for interacting with a USB development cartridge for the
 * Sega Saturn.
 *
 * This provides access to registers for USB communication, SD card access, and
 * other hardware features.
 */
namespace SRL
{
/** @brief Namespace for Sega Saturn USB development cartridge hardware access.
     */
    namespace DevCart
    {
/**
         * @brief Minimal framed protocol for host commands over DevCart USB FIFO.
         *
         * This protocol is used by host tools (such as ftx) to send filesystem-like
         * requests (ls/rm/crc) through FTDI, where Saturn-side code can parse and
         * handle them.
         *
         * Request frame:
         *   - 4 bytes magic: "SRL1"
         *   - 1 byte command
         *   - 2 bytes payload length (big-endian)
         *   - N bytes payload
         *
         * Response frame:
         *   - 4 bytes magic: "SRL1"
         *   - 1 byte status
         *   - 2 bytes payload length (big-endian)
         *   - N bytes payload
         */
        namespace HostIo
        {
/** @brief Command type identifiers for host-to-Saturn requests.
             */
            enum class Command : uint8_t
            {
  /** @brief List files command. */
                List = 1,
  /** @brief Remove file command. */
                Remove = 2,
  /** @brief Compute CRC checksum command. */
                Crc = 3,
  /** @brief Upload file command. */
                Upload = 4,
  /** @brief Create directory command. */
                Mkdir = 5,
  /** @brief Remove directory command. */
                Rmdir = 6
            };

/** @brief Status response code identifiers for Saturn-to-host responses.
             */
            enum class Status : uint8_t
            {
  /** @brief Operation succeeded. */
                Ok = 0,
  /** @brief General error occurred. */
                Error = 1,
  /** @brief Command/operation is unsupported. */
                Unsupported = 2,
  /** @brief Bad request parameters or format. */
                BadRequest = 3,
  /** @brief Request successfully handled. */
                Handled = 4
            };

/** @brief Magic character 0 ('S').
             */
            constexpr static uint8_t MAGIC_0 = 'S';
/** @brief Magic character 1 ('R').
             */
            constexpr static uint8_t MAGIC_1 = 'R';
/** @brief Magic character 2 ('L').
             */
            constexpr static uint8_t MAGIC_2 = 'L';
/** @brief Magic character 3 ('1').
             */
            constexpr static uint8_t MAGIC_3 = '1';

/** @brief Total size in bytes of the protocol frame header.
             */
            constexpr static size_t HEADER_SIZE = 7;

/** @brief Write all bytes of a buffer to the DevCart USB FIFO.
             *  @param data Pointer to the source buffer.
             *  @param size Number of bytes to write.
             *  @return True if all bytes were successfully written.
             */
            static inline bool WriteAll(const uint8_t *data, size_t size)
            {
                return CS0::Write(data, size) == size;
            }

/** @brief Read all bytes into a buffer from the DevCart USB FIFO.
             *  @param data Pointer to the destination buffer.
             *  @param size Number of bytes to read.
             *  @return True when all bytes are successfully read.
             */
            static inline bool ReadAll(uint8_t *data, size_t size)
            {
                for (size_t i = 0; i < size; ++i)
                {
                    data[i] = CS0::Read();
                }
                return true;
            }

/** @brief Decodes a 16-bit big-endian value from two bytes.
             *  @param hi High byte.
             *  @param lo Low byte.
             *  @return The decoded 16-bit unsigned integer value.
             */
            static inline uint16_t DecodeU16BE(const uint8_t hi, const uint8_t lo)
            {
                return static_cast<uint16_t>((static_cast<uint16_t>(hi) << 8) |
                                             static_cast<uint16_t>(lo));
            }

/** @brief Attempt to read and parse a request frame from Host.
             *  @param command Output parameter returning the parsed command.
             *  @param payloadBuffer Buffer to receive the request payload.
             *  @param payloadCapacity Maximum bytes the payloadBuffer can hold.
             *  @param payloadSize Output parameter returning the actual read payload size.
             *  @return True if request header was read successfully and matches protocol
             * rules.
             */
            static inline bool TryReadRequest(Command &command, uint8_t *payloadBuffer,
                size_t payloadCapacity, size_t &payloadSize)
            {
                payloadSize = 0;
                uint8_t header[HEADER_SIZE];
                if (!ReadAll(header, HEADER_SIZE))
                {
                    return false;
                }

                if (header[0] != MAGIC_0 || header[1] != MAGIC_1 || header[2] != MAGIC_2 ||
                    header[3] != MAGIC_3)
                {
                    return false;
                }

                command = static_cast<Command>(header[4]);
                const uint16_t payloadLen = DecodeU16BE(header[5], header[6]);

                if (payloadLen > payloadCapacity)
                {
                    uint8_t sink = 0;
                    for (uint16_t i = 0; i < payloadLen; ++i)
                    {
                        sink = CS0::Read();
                    }
                    (void)sink;
                    return false;
                }

                if (payloadLen > 0)
                {
                    ReadAll(payloadBuffer, payloadLen);
                    payloadSize = payloadLen;
                }

                return true;
            }

/** @brief Send a protocol response frame to the Host.
             *  @param status The response status code.
             *  @param payload Pointer to response payload buffer (can be nullptr if
             * payloadSize is 0).
             *  @param payloadSize Number of bytes in response payload.
             *  @return True if response was sent successfully.
             */
            static inline bool SendResponse(Status status, const uint8_t *payload,
                size_t payloadSize)
            {
                if (payloadSize > 0xFFFFU)
                {
                    return false;
                }

                uint8_t header[HEADER_SIZE] = {
                    MAGIC_0,
                    MAGIC_1,
                    MAGIC_2,
                    MAGIC_3,
                    static_cast<uint8_t>(status),
                    static_cast<uint8_t>((payloadSize >> 8) & 0xFFU),
                    static_cast<uint8_t>(payloadSize & 0xFFU)};

                if (!WriteAll(header, HEADER_SIZE))
                {
                    return false;
                }

                if (payloadSize == 0)
                {
                    return true;
                }

                return WriteAll(payload, payloadSize);
            }
        } // namespace HostIo

    } // namespace DevCart
} // namespace SRL