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

#include "fatfs/ff.h"
#include "sgclib/sgclib.h"

// Define the global dir required by sgclib/fatfs
DIR dir;

namespace MockFatFs
{
    static FRESULT nextFResult = FR_OK;
    static int fsInitResult = 0;
    static char lastPath[256] = {0};
    static char lastPath2[256] = {0};
    static char lastCwd[256] = {0};
    static bool fsInitCalled = false;
    static bool fMountCalled = false;
    static bool fUnlinkCalled = false;
    static bool fMkdirCalled = false;
    static bool fRenameCalled = false;
    static bool fStatCalled = false;
    static bool fOpendirCalled = false;
    static bool fReaddirCalled = false;
    static bool fGetcwdCalled = false;
    static bool fChdirCalled = false;
    static bool fOpenCalled = false;
    static bool fReadCalled = false;
    static bool fWriteCalled = false;
    static bool fCloseCalled = false;

    inline void Reset()
    {
        nextFResult = FR_OK;
        fsInitResult = 0;
        lastPath[0] = '\0';
        lastPath2[0] = '\0';
        lastCwd[0] = '\0';
        fsInitCalled = false;
        fMountCalled = false;
        fUnlinkCalled = false;
        fMkdirCalled = false;
        fRenameCalled = false;
        fStatCalled = false;
        fOpendirCalled = false;
        fReaddirCalled = false;
        fGetcwdCalled = false;
        fChdirCalled = false;
        fOpenCalled = false;
        fReadCalled = false;
        fWriteCalled = false;
        fCloseCalled = false;
    }
}

extern "C" {
    int sgc_init(void)
    {
        MockFatFs::fsInitCalled = true;
        return MockFatFs::fsInitResult;
    }
    int sgc_open(const char *filename, int flags)
    {
        (void)flags;
        strncpy(MockFatFs::lastPath, filename, sizeof(MockFatFs::lastPath) - 1);
        MockFatFs::fOpenCalled = true;
        return 0;
    }
    int sgc_close(int fd)
    {
        (void)fd;
        MockFatFs::fCloseCalled = true;
        return 0;
    }
    int sgc_read(int fd, void *buf, int len)
    {
        (void)fd; (void)buf; (void)len;
        MockFatFs::fReadCalled = true;
        return 0;
    }
    int sgc_write(int fd, const void *buf, int len)
    {
        (void)fd; (void)buf; (void)len;
        MockFatFs::fWriteCalled = true;
        return 0;
    }
    int sgc_stat(const char *filename, sgc_stat_t *stat, int statsize)
    {
        (void)statsize;
        strncpy(MockFatFs::lastPath, filename, sizeof(MockFatFs::lastPath) - 1);
        MockFatFs::fStatCalled = true;
        if (stat)
        {
            stat->attrib = AM_DIR;
            strcpy(stat->name, "testdir");
        }
        return MockFatFs::nextFResult;
    }
    int sgc_rename(const char *oldname, const char *newname)
    {
        strncpy(MockFatFs::lastPath, oldname, sizeof(MockFatFs::lastPath) - 1);
        strncpy(MockFatFs::lastPath2, newname, sizeof(MockFatFs::lastPath2) - 1);
        MockFatFs::fRenameCalled = true;
        return MockFatFs::nextFResult;
    }
    int sgc_mkdir(const char *filename)
    {
        strncpy(MockFatFs::lastPath, filename, sizeof(MockFatFs::lastPath) - 1);
        MockFatFs::fMkdirCalled = true;
        return MockFatFs::nextFResult;
    }
    int sgc_unlink(const char *filename)
    {
        strncpy(MockFatFs::lastPath, filename, sizeof(MockFatFs::lastPath) - 1);
        MockFatFs::fUnlinkCalled = true;
        return MockFatFs::nextFResult;
    }
    int sgc_opendir(const char *path)
    {
        strncpy(MockFatFs::lastPath, path, sizeof(MockFatFs::lastPath) - 1);
        MockFatFs::fOpendirCalled = true;
        return MockFatFs::nextFResult;
    }
    int sgc_chdir(const char *path)
    {
        strncpy(MockFatFs::lastCwd, path, sizeof(MockFatFs::lastCwd) - 1);
        MockFatFs::fChdirCalled = true;
        return MockFatFs::nextFResult;
    }
    int sgc_getcwd(char *buff, int buflen)
    {
        MockFatFs::fGetcwdCalled = true;
        strncpy(buff, "/mock/cwd", buflen - 1);
        return 0;
    }

    FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt)
    {
        (void)fs; (void)path; (void)opt;
        MockFatFs::fMountCalled = true;
        return MockFatFs::nextFResult;
    }
    FRESULT f_unlink(const TCHAR* path)
    {
        strncpy(MockFatFs::lastPath, path, sizeof(MockFatFs::lastPath) - 1);
        MockFatFs::fUnlinkCalled = true;
        return MockFatFs::nextFResult;
    }
    FRESULT f_mkdir(const TCHAR* path)
    {
        strncpy(MockFatFs::lastPath, path, sizeof(MockFatFs::lastPath) - 1);
        MockFatFs::fMkdirCalled = true;
        return MockFatFs::nextFResult;
    }
    FRESULT f_rename(const TCHAR* old_name, const TCHAR* new_name)
    {
        strncpy(MockFatFs::lastPath, old_name, sizeof(MockFatFs::lastPath) - 1);
        strncpy(MockFatFs::lastPath2, new_name, sizeof(MockFatFs::lastPath2) - 1);
        MockFatFs::fRenameCalled = true;
        return MockFatFs::nextFResult;
    }
    FRESULT f_stat(const TCHAR* path, FILINFO* fno)
    {
        strncpy(MockFatFs::lastPath, path, sizeof(MockFatFs::lastPath) - 1);
        MockFatFs::fStatCalled = true;
        if (fno)
        {
            fno->fattrib = AM_DIR;
            strcpy(fno->fname, "testdir");
        }
        return MockFatFs::nextFResult;
    }
    FRESULT f_opendir(DIR* dp, const TCHAR* path)
    {
        (void)dp;
        strncpy(MockFatFs::lastPath, path, sizeof(MockFatFs::lastPath) - 1);
        MockFatFs::fOpendirCalled = true;
        return MockFatFs::nextFResult;
    }
    static int g_readdir_count = 0;
    FRESULT f_readdir(DIR* dp, FILINFO* fno)
    {
        (void)dp;
        MockFatFs::fReaddirCalled = true;
        if (fno && g_readdir_count == 0)
        {
            fno->fattrib = 0;
            fno->fsize = 1234;
            fno->fdate = (2026 - 1980) << 9 | 7 << 5 | 18;
            fno->ftime = 12 << 11 | 30 << 5 | 0;
            strcpy(fno->fname, "testfile.txt");
            g_readdir_count++;
            return FR_OK;
        }
        else if (fno && g_readdir_count == 1)
        {
            fno->fattrib = AM_DIR;
            fno->fsize = 0;
            fno->fdate = (2026 - 1980) << 9 | 7 << 5 | 18;
            fno->ftime = 12 << 11 | 30 << 5 | 0;
            strcpy(fno->fname, "subdir");
            g_readdir_count++;
            return FR_OK;
        }
        if (fno)
        {
            fno->fname[0] = '\0';
        }
        return FR_OK;
    }
    FRESULT f_getcwd(TCHAR* buff, UINT len)
    {
        MockFatFs::fGetcwdCalled = true;
        strncpy(buff, "/mock/cwd", len - 1);
        return FR_OK;
    }
    FRESULT f_chdir(const TCHAR* path)
    {
        strncpy(MockFatFs::lastCwd, path, sizeof(MockFatFs::lastCwd) - 1);
        MockFatFs::fChdirCalled = true;
        return MockFatFs::nextFResult;
    }
    static uint32_t g_file_offset = 0;
    static uint32_t g_file_size = 10;
    FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode)
    {
        (void)fp; (void)mode;
        strncpy(MockFatFs::lastPath, path, sizeof(MockFatFs::lastPath) - 1);
        MockFatFs::fOpenCalled = true;
        g_file_offset = 0;
        return MockFatFs::nextFResult;
    }
    FRESULT f_read(FIL* fp, void* buff, UINT btr, UINT* br)
    {
        (void)fp;
        MockFatFs::fReadCalled = true;
        if (br)
        {
            uint32_t remaining = g_file_size - g_file_offset;
            uint32_t to_read = (btr < remaining) ? btr : remaining;
            memset(buff, 0xAA, to_read);
            g_file_offset += to_read;
            *br = to_read;
        }
        return FR_OK;
    }
    FRESULT f_write(FIL* fp, const void* buff, UINT btw, UINT* bw)
    {
        (void)fp; (void)buff;
        MockFatFs::fWriteCalled = true;
        if (bw)
        {
            *bw = btw;
        }
        return FR_OK;
    }
    FRESULT f_close(FIL* fp)
    {
        (void)fp;
        MockFatFs::fCloseCalled = true;
        return FR_OK;
    }
}

#include "srl_devcart_sdcart.hpp"
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
    MockFatFs::Reset();
    g_readdir_count = 0;
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

MU_TEST(test_try_read_request_exact_capacity)
{
    using namespace SRL::DevCart::HostIo;
    const uint8_t req[] = {'S', 'R', 'L', '1', static_cast<uint8_t>(Command::List), 0, 4, '1', '2', '3', '4'};
    MockCS0::SetReadData(req, sizeof(req));

    Command cmd;
    uint8_t payload[4]; // capacity is exactly 4
    size_t payloadSize = 0;
    mu_assert(TryReadRequest(cmd, payload, sizeof(payload), payloadSize), "TryReadRequest with exact capacity failed");
    mu_assert(cmd == Command::List, "Incorrect command");
    mu_assert_int_eq(4, payloadSize);
    mu_assert_int_eq('1', payload[0]);
    mu_assert_int_eq('4', payload[3]);
}

MU_TEST(test_try_read_request_incomplete_header)
{
    using namespace SRL::DevCart::HostIo;
    const uint8_t req[] = {'S', 'R', 'L'}; // Only 3 bytes
    MockCS0::SetReadData(req, sizeof(req));

    Command cmd;
    uint8_t payload[10];
    size_t payloadSize = 0;
    mu_assert(!TryReadRequest(cmd, payload, sizeof(payload), payloadSize), "TryReadRequest should fail on incomplete header");
}

MU_TEST(test_try_read_request_all_commands)
{
    using namespace SRL::DevCart::HostIo;
    const Command commands[] = {
        Command::List,
        Command::Remove,
        Command::Crc,
        Command::Upload,
        Command::Mkdir,
        Command::Rmdir,
        Command::Rename,
        Command::Download
    };

    for (size_t i = 0; i < sizeof(commands)/sizeof(commands[0]); ++i)
    {
        MockCS0::Reset();
        const uint8_t req[] = {'S', 'R', 'L', '1', static_cast<uint8_t>(commands[i]), 0, 0};
        MockCS0::SetReadData(req, sizeof(req));

        Command cmd;
        uint8_t payload[10];
        size_t payloadSize = 999;
        mu_assert(TryReadRequest(cmd, payload, sizeof(payload), payloadSize), "TryReadRequest failed for command");
        mu_assert(cmd == commands[i], "Parsed command mismatch");
        mu_assert_int_eq(0, payloadSize);
    }
}

MU_TEST(test_send_response_all_statuses)
{
    using namespace SRL::DevCart::HostIo;
    const Status statuses[] = {
        Status::Ok,
        Status::Error,
        Status::Unsupported,
        Status::BadRequest,
        Status::Handled
    };

    for (size_t i = 0; i < sizeof(statuses)/sizeof(statuses[0]); ++i)
    {
        MockCS0::Reset();
        uint8_t outBuf[10] = {0};
        MockCS0::SetWriteBuffer(outBuf, sizeof(outBuf));

        mu_assert(SendResponse(statuses[i], nullptr, 0), "SendResponse failed for status");
        mu_assert_int_eq(HEADER_SIZE, MockCS0::writtenDataSize);
        mu_assert_int_eq(static_cast<uint8_t>(statuses[i]), outBuf[4]);
    }
}

MU_TEST(test_try_read_request_sequential)
{
    using namespace SRL::DevCart::HostIo;
    // Sequential requests: List (no payload) then Remove (4 bytes payload)
    const uint8_t req[] = {
        'S', 'R', 'L', '1', static_cast<uint8_t>(Command::List), 0, 0,
        'S', 'R', 'L', '1', static_cast<uint8_t>(Command::Remove), 0, 4, 'a', 'b', 'c', 'd'
    };
    MockCS0::SetReadData(req, sizeof(req));

    Command cmd;
    uint8_t payload[10];
    size_t payloadSize = 999;

    // First request
    mu_assert(TryReadRequest(cmd, payload, sizeof(payload), payloadSize), "Sequential read 1 failed");
    mu_assert(cmd == Command::List, "First command mismatch");
    mu_assert_int_eq(0, payloadSize);

    // Second request
    mu_assert(TryReadRequest(cmd, payload, sizeof(payload), payloadSize), "Sequential read 2 failed");
    mu_assert(cmd == Command::Remove, "Second command mismatch");
    mu_assert_int_eq(4, payloadSize);
    mu_assert_int_eq('a', payload[0]);
    mu_assert_int_eq('d', payload[3]);
}

MU_TEST(test_is_sd_fs_path)
{
    using namespace SRL::DevCart::SD;
    mu_assert(IsSdFsPath("/test"), "Should be SD path");
    mu_assert(IsSdFsPath("/"), "Should be SD path");
    mu_assert(!IsSdFsPath("sdraw:0:100"), "Should not be SD path");
    mu_assert(!IsSdFsPath(""), "Should not be SD path");
    mu_assert(!IsSdFsPath(nullptr), "Should not be SD path");
}

MU_TEST(test_split_path_and_name)
{
    using namespace SRL::DevCart::SD;
    char path1[100];
    
    strcpy(path1, "/dir1/dir2/file.txt");
    char *name1 = SplitPathAndName(path1);
    mu_assert_string_eq("file.txt", name1);
    mu_assert_string_eq("/dir1/dir2", path1);

    strcpy(path1, "file.txt");
    char *name2 = SplitPathAndName(path1);
    mu_assert_string_eq("file.txt", name2);

    strcpy(path1, "/file.txt");
    char *name3 = SplitPathAndName(path1);
    mu_assert_string_eq("file.txt", name3);
    mu_assert_string_eq("", path1);
}

MU_TEST(test_backend_raw_paths)
{
    using namespace SRL::DevCart::SD::Backend;
    mu_assert(IsRawPath("sdraw:0:100"), "Should be raw path");
    mu_assert(!IsRawPath("/test"), "Should not be raw path");

    mu_assert(IsRawRootPath("sdraw:/"), "Should be raw root path");
    mu_assert(IsRawRootPath("sdraw:\\"), "Should be raw root path");
    mu_assert(IsRawRootPath("sdraw:"), "Should be raw root path");
    mu_assert(!IsRawRootPath("sdraw:0:100"), "Should not be raw root");

    uint32_t start = 0, count = 0;
    mu_assert(TryParseRawRange("sdraw:123:456", start, count), "Should parse range");
    mu_assert_int_eq(123, start);
    mu_assert_int_eq(456, count);

    mu_assert(!TryParseRawRange("sdraw:abc:def", start, count), "Should fail parse");
}

MU_TEST(test_handle_list_sd_fs)
{
    using namespace SRL::DevCart::SD;
    char response[1024] = {0};
    size_t responseLen = 0;

    MockFatFs::nextFResult = FR_OK;
    g_readdir_count = 0;

    SRL::DevCart::HostIo::Status status = HandleList("/testdir", response, responseLen);
    mu_assert(status == SRL::DevCart::HostIo::Status::Ok, "HandleList failed");
    mu_assert(MockFatFs::fOpendirCalled, "f_opendir should have been called");
    mu_assert(MockFatFs::fReaddirCalled, "f_readdir should have been called");
    mu_assert_string_eq("/testdir", MockFatFs::lastPath);
}

MU_TEST(test_handle_remove_sd_fs)
{
    using namespace SRL::DevCart::SD;
    char response[256] = {0};
    size_t responseLen = 0;

    MockFatFs::nextFResult = FR_OK;
    SRL::DevCart::HostIo::Status status = HandleRemove("/testdir/file.txt", response, responseLen);
    mu_assert(status == SRL::DevCart::HostIo::Status::Ok, "HandleRemove failed");
    mu_assert(MockFatFs::fUnlinkCalled, "f_unlink should have been called");
    mu_assert_string_eq("/testdir/file.txt", MockFatFs::lastPath);
}

MU_TEST(test_handle_mkdir_sd_fs)
{
    using namespace SRL::DevCart::SD;
    char response[256] = {0};
    size_t responseLen = 0;

    MockFatFs::nextFResult = FR_OK;
    SRL::DevCart::HostIo::Status status = HandleMkdir("/testdir/newdir", response, responseLen);
    mu_assert(status == SRL::DevCart::HostIo::Status::Ok, "HandleMkdir failed");
    mu_assert(MockFatFs::fMkdirCalled, "f_mkdir should have been called");
    mu_assert_string_eq("/testdir/newdir", MockFatFs::lastPath);
}

MU_TEST(test_handle_rmdir_sd_fs)
{
    using namespace SRL::DevCart::SD;
    char response[256] = {0};
    size_t responseLen = 0;

    MockFatFs::nextFResult = FR_OK;
    SRL::DevCart::HostIo::Status status = HandleRmdir("/testdir/olddir", response, responseLen);
    mu_assert(status == SRL::DevCart::HostIo::Status::Ok, "HandleRmdir failed");
    mu_assert(MockFatFs::fUnlinkCalled, "f_unlink should have been called");
    mu_assert_string_eq("/testdir/olddir", MockFatFs::lastPath);
}

MU_TEST(test_handle_rename_sd_fs)
{
    using namespace SRL::DevCart::SD;
    char response[256] = {0};
    size_t responseLen = 0;

    char payload[100];
    char *p = payload;
    strcpy(p, "/old.txt");
    p += strlen("/old.txt") + 1;
    strcpy(p, "/new.txt");

    MockFatFs::nextFResult = FR_OK;
    SRL::DevCart::HostIo::Status status = HandleRename(payload, response, responseLen);
    mu_assert(status == SRL::DevCart::HostIo::Status::Ok, "HandleRename failed");
    mu_assert(MockFatFs::fRenameCalled, "f_rename should have been called");
    mu_assert_string_eq("/old.txt", MockFatFs::lastPath);
    mu_assert_string_eq("/new.txt", MockFatFs::lastPath2);
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
    MU_RUN_TEST(test_try_read_request_exact_capacity);
    MU_RUN_TEST(test_try_read_request_incomplete_header);
    MU_RUN_TEST(test_try_read_request_all_commands);
    MU_RUN_TEST(test_send_response_all_statuses);
    MU_RUN_TEST(test_try_read_request_sequential);

    MU_RUN_TEST(test_is_sd_fs_path);
    MU_RUN_TEST(test_split_path_and_name);
    MU_RUN_TEST(test_backend_raw_paths);
    MU_RUN_TEST(test_handle_list_sd_fs);
    MU_RUN_TEST(test_handle_remove_sd_fs);
    MU_RUN_TEST(test_handle_mkdir_sd_fs);
    MU_RUN_TEST(test_handle_rmdir_sd_fs);
    MU_RUN_TEST(test_handle_rename_sd_fs);
}
}

