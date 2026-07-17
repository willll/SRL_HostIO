#pragma once

#include <srl.hpp>
#include <srl_log.hpp>
#include <srl_devcart.hpp>

/**
 * @brief Mock implementation of CS0 to simulate USB FIFO operations without hardware.
 *
 * This mock namespace intercepts calls from hostio.hpp to the hardware-bound CS0 space
 * and redirects them to RAM buffers. This makes unit testing on standard emulator platforms
 * stable and predictable.
 */
namespace MockCS0
{
    /** @brief Pointer to the buffer that simulates incoming host data. */
    static const uint8_t *readBuffer = nullptr;
    /** @brief Size of the read buffer in bytes. */
    static size_t readBufferSize = 0;
    /** @brief Current offset index when reading from the buffer. */
    static size_t readBufferOffset = 0;

    /** @brief Pointer to the destination buffer that captures data sent by the Saturn. */
    static uint8_t *writtenData = nullptr;
    /** @brief Number of bytes written by Saturn. */
    static size_t writtenDataSize = 0;
    /** @brief Capacity of the output destination buffer. */
    static size_t writtenDataCapacity = 0;

    /**
     * @brief Resets the mock CS0 state.
     */
    inline void Reset()
    {
        readBuffer = nullptr;
        readBufferSize = 0;
        readBufferOffset = 0;
        writtenData = nullptr;
        writtenDataSize = 0;
        writtenDataCapacity = 0;
    }

    /**
     * @brief Configures mock data for the HostIo reader to consume.
     * @param data Pointer to mock host request frame.
     * @param size Size of mock frame in bytes.
     */
    inline void SetReadData(const uint8_t *data, size_t size)
    {
        readBuffer = data;
        readBufferSize = size;
        readBufferOffset = 0;
    }

    /**
     * @brief Sets up a destination buffer to capture responses sent by Saturn.
     * @param buffer Pointer to the destination buffer.
     * @param capacity Capacity of the buffer in bytes.
     */
    inline void SetWriteBuffer(uint8_t *buffer, size_t capacity)
    {
        writtenData = buffer;
        writtenDataCapacity = capacity;
        writtenDataSize = 0;
    }

    /**
     * @brief Mocks CS0::Read register access.
     * @return Next byte from read buffer, or 0 if end of buffer.
     */
    inline uint8_t Read()
    {
        if (readBufferOffset < readBufferSize && readBuffer != nullptr)
        {
            return readBuffer[readBufferOffset++];
        }
        return 0;
    }

    inline size_t Write(const uint8_t *data, size_t size)
    {
        size_t written = 0;
        for (size_t i = 0; i < size; ++i)
        {
            if (writtenData != nullptr)
            {
                if (writtenDataSize < writtenDataCapacity)
                {
                    writtenData[writtenDataSize++] = data[i];
                    ++written;
                }
            }
            else
            {
                ++writtenDataSize;
                ++written;
            }
        }
        return written;
    }
} // namespace MockCS0

// Redirect CS0 references in srl_devcart_hostio.hpp to MockCS0
#define CS0 MockCS0
#include <srl_devcart_hostio.hpp>
#undef CS0

#include "minunit.h"

using namespace SRL;
using namespace SRL::Logger;

extern "C" {
extern const uint8_t buffer_size;
extern char buffer[];
extern uint32_t suite_error_counter;

/**
 * @brief Setup function executed before each HostIO unit test.
 */
void hostio_test_setup(void)
{
    MockCS0::Reset();
}

/**
 * @brief Teardown function executed after each HostIO unit test.
 */
void hostio_test_teardown(void)
{
    ASCII::Clear();
    ASCII::SetPalette(0);
}

/**
 * @brief Outputs the test suite header once when errors are first encountered.
 */
void hostio_test_output_header(void)
{
    if (!suite_error_counter++)
    {
        if (Log::GetLogLevel() == Logger::LogLevels::TESTING)
        {
            LogDebug("****UT_HOSTIO****");
        }
        else
        {
            LogInfo("****UT_HOSTIO_ERROR(S)****");
        }
    }
}

MU_TEST(test_decode_u16_be)
{
    using namespace SRL::DevCart::HostIo;
    mu_assert_int_eq(0x1234, DecodeU16BE(0x12, 0x34));
    mu_assert_int_eq(0x0000, DecodeU16BE(0x00, 0x00));
    mu_assert_int_eq(0xFFFF, DecodeU16BE(0xFF, 0xFF));
}

MU_TEST(test_write_all)
{
    using namespace SRL::DevCart::HostIo;
    uint8_t outBuf[10] = {0};
    MockCS0::SetWriteBuffer(outBuf, sizeof(outBuf));

    const uint8_t data[] = {1, 2, 3, 4, 5};
    mu_assert(WriteAll(data, 5), "WriteAll failed");
    mu_assert_int_eq(5, MockCS0::writtenDataSize);
    for (int i = 0; i < 5; ++i)
    {
        mu_assert_int_eq(data[i], outBuf[i]);
    }
}

MU_TEST(test_read_all)
{
    using namespace SRL::DevCart::HostIo;
    const uint8_t data[] = {9, 8, 7, 6};
    MockCS0::SetReadData(data, sizeof(data));

    uint8_t inBuf[4] = {0};
    mu_assert(ReadAll(inBuf, 4), "ReadAll failed");
    for (int i = 0; i < 4; ++i)
    {
        mu_assert_int_eq(data[i], inBuf[i]);
    }
}

MU_TEST(test_try_read_request_success_no_payload)
{
    using namespace SRL::DevCart::HostIo;
    const uint8_t req[] = {'S', 'R', 'L', '1', static_cast<uint8_t>(Command::List), 0, 0};
    MockCS0::SetReadData(req, sizeof(req));

    Command cmd;
    uint8_t payload[10];
    size_t payloadSize = 999;
    mu_assert(TryReadRequest(cmd, payload, sizeof(payload), payloadSize), "TryReadRequest failed");
    mu_assert(cmd == Command::List, "Incorrect command");
    mu_assert_int_eq(0, payloadSize);
}

MU_TEST(test_try_read_request_success_with_payload)
{
    using namespace SRL::DevCart::HostIo;
    const uint8_t req[] = {'S', 'R', 'L', '1', static_cast<uint8_t>(Command::Remove), 0, 4, 'a', 'b', 'c', 'd'};
    MockCS0::SetReadData(req, sizeof(req));

    Command cmd;
    uint8_t payload[10];
    size_t payloadSize = 0;
    mu_assert(TryReadRequest(cmd, payload, sizeof(payload), payloadSize), "TryReadRequest failed");
    mu_assert(cmd == Command::Remove, "Incorrect command");
    mu_assert_int_eq(4, payloadSize);
    mu_assert_int_eq('a', payload[0]);
    mu_assert_int_eq('b', payload[1]);
    mu_assert_int_eq('c', payload[2]);
    mu_assert_int_eq('d', payload[3]);
}

MU_TEST(test_try_read_request_wrong_magic)
{
    using namespace SRL::DevCart::HostIo;
    const uint8_t req[] = {'S', 'R', 'L', '2', static_cast<uint8_t>(Command::List), 0, 0};
    MockCS0::SetReadData(req, sizeof(req));

    Command cmd;
    uint8_t payload[10];
    size_t payloadSize = 0;
    mu_assert(!TryReadRequest(cmd, payload, sizeof(payload), payloadSize), "TryReadRequest should fail for wrong magic");
}

MU_TEST(test_try_read_request_payload_overflow)
{
    using namespace SRL::DevCart::HostIo;
    const uint8_t req[] = {'S', 'R', 'L', '1', static_cast<uint8_t>(Command::Upload), 0, 5, 'a', 'b', 'c', 'd', 'e'};
    MockCS0::SetReadData(req, sizeof(req));

    Command cmd;
    uint8_t payload[3]; // Capacity is 3, payload is 5
    size_t payloadSize = 0;
    mu_assert(!TryReadRequest(cmd, payload, sizeof(payload), payloadSize), "TryReadRequest should fail for overflow");
    // Ensure the remainder of the payload was discarded (CS0::Read called for all payload bytes)
    mu_assert_int_eq(sizeof(req), MockCS0::readBufferOffset);
}

MU_TEST(test_send_response_no_payload)
{
    using namespace SRL::DevCart::HostIo;
    uint8_t outBuf[20] = {0};
    MockCS0::SetWriteBuffer(outBuf, sizeof(outBuf));

    mu_assert(SendResponse(Status::Ok, nullptr, 0), "SendResponse failed");
    mu_assert_int_eq(HEADER_SIZE, MockCS0::writtenDataSize);
    mu_assert_int_eq('S', outBuf[0]);
    mu_assert_int_eq('R', outBuf[1]);
    mu_assert_int_eq('L', outBuf[2]);
    mu_assert_int_eq('1', outBuf[3]);
    mu_assert_int_eq(static_cast<uint8_t>(Status::Ok), outBuf[4]);
    mu_assert_int_eq(0, outBuf[5]);
    mu_assert_int_eq(0, outBuf[6]);
}

MU_TEST(test_send_response_with_payload)
{
    using namespace SRL::DevCart::HostIo;
    uint8_t outBuf[20] = {0};
    MockCS0::SetWriteBuffer(outBuf, sizeof(outBuf));

    const uint8_t payload[] = {'o', 'k'};
    mu_assert(SendResponse(Status::Handled, payload, sizeof(payload)), "SendResponse failed");
    mu_assert_int_eq(HEADER_SIZE + sizeof(payload), MockCS0::writtenDataSize);
    mu_assert_int_eq('S', outBuf[0]);
    mu_assert_int_eq('R', outBuf[1]);
    mu_assert_int_eq('L', outBuf[2]);
    mu_assert_int_eq('1', outBuf[3]);
    mu_assert_int_eq(static_cast<uint8_t>(Status::Handled), outBuf[4]);
    mu_assert_int_eq(0, outBuf[5]);
    mu_assert_int_eq(2, outBuf[6]);
    mu_assert_int_eq('o', outBuf[7]);
    mu_assert_int_eq('k', outBuf[8]);
}

MU_TEST(test_send_response_overflow)
{
    using namespace SRL::DevCart::HostIo;
    mu_assert(!SendResponse(Status::Ok, nullptr, 0x10000), "SendResponse should fail for payload > 0xFFFF");
}

MU_TEST(test_write_all_partial)
{
    using namespace SRL::DevCart::HostIo;
    uint8_t outBuf[3] = {0};
    MockCS0::SetWriteBuffer(outBuf, sizeof(outBuf));

    const uint8_t data[] = {1, 2, 3, 4, 5};
    mu_assert(!WriteAll(data, 5), "WriteAll should fail on partial write");
    mu_assert_int_eq(3, MockCS0::writtenDataSize);
}

MU_TEST(test_read_all_zero_size)
{
    using namespace SRL::DevCart::HostIo;
    mu_assert(ReadAll(nullptr, 0), "ReadAll with zero size should succeed");
}

MU_TEST(test_try_read_request_empty_capacity)
{
    using namespace SRL::DevCart::HostIo;
    const uint8_t req[] = {'S', 'R', 'L', '1', static_cast<uint8_t>(Command::Upload), 0, 5, 'a', 'b', 'c', 'd', 'e'};
    MockCS0::SetReadData(req, sizeof(req));

    Command cmd;
    size_t payloadSize = 0;
    mu_assert(!TryReadRequest(cmd, nullptr, 0, payloadSize), "TryReadRequest should fail when capacity is 0");
    mu_assert_int_eq(sizeof(req), MockCS0::readBufferOffset);
}

MU_TEST(test_try_read_request_unknown_command)
{
    using namespace SRL::DevCart::HostIo;
    const uint8_t req[] = {'S', 'R', 'L', '1', 99, 0, 0};
    MockCS0::SetReadData(req, sizeof(req));

    Command cmd;
    uint8_t payload[10];
    size_t payloadSize = 0;
    mu_assert(TryReadRequest(cmd, payload, sizeof(payload), payloadSize), "TryReadRequest should succeed for unknown command");
    mu_assert_int_eq(99, static_cast<uint8_t>(cmd));
}

MU_TEST(test_send_response_header_fail)
{
    using namespace SRL::DevCart::HostIo;
    uint8_t outBuf[5] = {0};
    MockCS0::SetWriteBuffer(outBuf, sizeof(outBuf));

    mu_assert(!SendResponse(Status::Ok, nullptr, 0), "SendResponse should fail when header cannot be fully written");
}

MU_TEST(test_send_response_payload_fail)
{
    using namespace SRL::DevCart::HostIo;
    uint8_t outBuf[8] = {0};
    MockCS0::SetWriteBuffer(outBuf, sizeof(outBuf));

    const uint8_t payload[] = {1, 2};
    mu_assert(!SendResponse(Status::Ok, payload, 2), "SendResponse should fail when payload cannot be fully written");
}

MU_TEST(test_send_response_max_size)
{
    using namespace SRL::DevCart::HostIo;
    // writtenData is nullptr, MockCS0::Write will count bytes but not store them
    mu_assert(SendResponse(Status::Ok, nullptr, 0xFFFF), "SendResponse with 0xFFFF size should succeed");
    mu_assert_int_eq(HEADER_SIZE + 0xFFFF, MockCS0::writtenDataSize);
}

MU_TEST_SUITE(hostio_test_suite)
{
    MU_SUITE_CONFIGURE_WITH_HEADER(&hostio_test_setup,
        &hostio_test_teardown,
        &hostio_test_output_header);

    MU_RUN_TEST(test_decode_u16_be);
    MU_RUN_TEST(test_write_all);
    MU_RUN_TEST(test_read_all);
    MU_RUN_TEST(test_try_read_request_success_no_payload);
    MU_RUN_TEST(test_try_read_request_success_with_payload);
    MU_RUN_TEST(test_try_read_request_wrong_magic);
    MU_RUN_TEST(test_try_read_request_payload_overflow);
    MU_RUN_TEST(test_send_response_no_payload);
    MU_RUN_TEST(test_send_response_with_payload);
    MU_RUN_TEST(test_send_response_overflow);
    MU_RUN_TEST(test_write_all_partial);
    MU_RUN_TEST(test_read_all_zero_size);
    MU_RUN_TEST(test_try_read_request_empty_capacity);
    MU_RUN_TEST(test_try_read_request_unknown_command);
    MU_RUN_TEST(test_send_response_header_fail);
    MU_RUN_TEST(test_send_response_payload_fail);
    MU_RUN_TEST(test_send_response_max_size);
}
}
