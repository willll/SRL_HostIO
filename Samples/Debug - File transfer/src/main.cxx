#include <srl.hpp>
#include <srl_devcart.hpp>
#include <srl_devcart_hostio.hpp>
#include "srl_devcart_sdcart.hpp"

#include <cstddef>
#include <cstdint>
#include <stdio.h>
#include <string.h>

using namespace SRL::Types;

namespace
{
    const int kShellStartY = 2;
    const int kShellMaxY = 27;
    int g_shellY = kShellStartY;

    /**
     * @brief Prints text to a scrolling shell-like view on the screen.
     *
     * Wraps long lines and handles scrolling when the text reaches the bottom of the screen.
     *
     * @param text The text to print.
     */
    void PrintShell(const char *text)
    {
        if (!text) return;
        const char *p = text;
        while (*p)
        {
            char lineBuf[45] = {0};
            int len = 0;
            while (*p && *p != '\n' && len < 44)
            {
                lineBuf[len++] = *p++;
            }
            if (*p == '\n') p++;

            SRL::Debug::PrintClearLine(g_shellY);
            SRL::Debug::Print(1, g_shellY, lineBuf);
            g_shellY++;

            if (g_shellY > kShellMaxY)
            {
                g_shellY = kShellStartY;
                for (int y = kShellStartY; y <= kShellMaxY; ++y)
                {
                    SRL::Debug::PrintClearLine(y);
                }
            }
        }
        SRL::Debug::PrintClearLine(g_shellY);
    }

    /**
     * @brief Reads and processes an incoming Host I/O request.
     *
     * Decodes the request command, routes it to the appropriate handler (e.g., HandleList,
     * HandleUpload), and sends the response back to the host via the DevCart interface.
     * Also echoes the command and response to the on-screen shell.
     */
    void HandleHostIoRequest()
    {
        uint8_t requestPayload[SRL::DevCart::SD::kMaxRequestBytes + 1];
        size_t requestLen = 0;
        SRL::DevCart::HostIo::Command command = SRL::DevCart::HostIo::Command::List;

        if (!SRL::DevCart::HostIo::TryReadRequest(command, requestPayload,
                SRL::DevCart::SD::kMaxRequestBytes, requestLen))
        {
            const char msg[] = "[HostIo] Bad request frame\n";
            SRL::DevCart::HostIo::SendResponse(
                SRL::DevCart::HostIo::Status::BadRequest,
                reinterpret_cast<const uint8_t *>(msg), sizeof(msg) - 1);
            return;
        }

        requestPayload[requestLen] = '\0';
        const char *path = reinterpret_cast<const char *>(requestPayload);

        const char *commandName = "UNKNOWN";
        switch (command)
        {
        case SRL::DevCart::HostIo::Command::List:
            commandName = "LS";
            break;
        case SRL::DevCart::HostIo::Command::Remove:
            commandName = "RM";
            break;
        case SRL::DevCart::HostIo::Command::Crc:
            commandName = "CRC";
            break;
        case SRL::DevCart::HostIo::Command::Upload:
            commandName = "UPLOAD";
            break;
        case SRL::DevCart::HostIo::Command::Download:
            commandName = "DOWNLOAD";
            break;
        case SRL::DevCart::HostIo::Command::Mkdir:
            commandName = "MKDIR";
            break;
        case SRL::DevCart::HostIo::Command::Rmdir:
            commandName = "RMDIR";
            break;
        case SRL::DevCart::HostIo::Command::Rename:
            commandName = "MV";
            break;
        default:
            break;
        }

        // Keep latest incoming host command visible during request handling.
        char cmdBuf[80];
        snprintf(cmdBuf, sizeof(cmdBuf), "> %s %s\n", commandName, path);
        PrintShell(cmdBuf);

        char response[SRL::DevCart::SD::kMaxResponseBytes + 1] = {0};
        size_t responseLen = 0;
        SRL::DevCart::HostIo::Status status = SRL::DevCart::HostIo::Status::Error;

        switch (command)
        {
        case SRL::DevCart::HostIo::Command::List:
            status = SRL::DevCart::SD::HandleList(path, response, responseLen);
            break;
        case SRL::DevCart::HostIo::Command::Remove:
            status = SRL::DevCart::SD::HandleRemove(path, response, responseLen);
            break;
        case SRL::DevCart::HostIo::Command::Crc:
            status = SRL::DevCart::SD::HandleCrc(path, response, responseLen);
            break;
        case SRL::DevCart::HostIo::Command::Upload:
            status = SRL::DevCart::SD::HandleUpload(path, response, responseLen);
            break;
        case SRL::DevCart::HostIo::Command::Download:
            status = SRL::DevCart::SD::HandleDownload(path, response, responseLen);
            break;
        case SRL::DevCart::HostIo::Command::Mkdir:
            status = SRL::DevCart::SD::HandleMkdir(path, response, responseLen);
            break;
        case SRL::DevCart::HostIo::Command::Rmdir:
            status = SRL::DevCart::SD::HandleRmdir(path, response, responseLen);
            break;
        case SRL::DevCart::HostIo::Command::Rename:
            status = SRL::DevCart::SD::HandleRename(path, response, responseLen);
            break;
        default:
            APPEND_FMT(response, SRL::DevCart::SD::kMaxResponseBytes + 1, responseLen,
                "[HostIo] Unsupported command: %u\n",
                static_cast<unsigned int>(command));
            status = SRL::DevCart::HostIo::Status::Unsupported;
            break;
        }

        if (status != SRL::DevCart::HostIo::Status::Handled)
        {
            SRL::DevCart::HostIo::SendResponse(
                status, reinterpret_cast<const uint8_t *>(response), responseLen);

            // Send explicit end-of-listing sentinel for List command
            if (command == SRL::DevCart::HostIo::Command::List && status == SRL::DevCart::HostIo::Status::Ok)
            {
                SRL::DevCart::HostIo::SendResponse(SRL::DevCart::HostIo::Status::Ok, nullptr, 0);
            }
        }
        if (responseLen > 0)
        {
            PrintShell(response);
        }
    }
} // namespace

#undef APPEND_FMT

int main()
{
    SRL::Core::Initialize(HighColor::Colors::Black);

    SRL::Debug::Print(1, 0, "File Transfer - FTX");

    while (true)
    {
        if (!SRL::DevCart::CS0::IsRxfEmpty())
        {
            HandleHostIoRequest();
        }

        SRL::Core::Synchronize();
    }

    return 0;
}
