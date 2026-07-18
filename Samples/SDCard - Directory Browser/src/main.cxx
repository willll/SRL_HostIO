#include <srl.hpp>
#include <srl_devcart_sdcard.hpp>

using namespace SRL::Types;

// Program entry point
int main()
{
    // Initialize library with black background
    SRL::Core::Initialize(HighColor::Colors::Black);

    SRL::Debug::Print(1, 1, "========================================");
    SRL::Debug::Print(1, 2, "   SDCard - Directory Browser Sample    ");
    SRL::Debug::Print(1, 3, "========================================");

    // Ensure the SD card driver and FatFs volume are mounted and ready
    SRL::Debug::Print(1, 5, "Initializing SD Card filesystem...");
    if (!SRL::DevCart::SD::EnsureSgclibReady())
    {
        SRL::Debug::Print(1, 6, "Error: SGCLIB initialization/mount failed.");
        while (true)
        {
            SRL::Core::Synchronize();
        }
    }
    SRL::Debug::Print(1, 6, "SD Card mounted successfully!");

    // Print current directory
    char currentDir[128] = {0};
    if (SRL::DevCart::SD::g_fatfsBackend.GetCurrentDirectory(currentDir, sizeof(currentDir)))
    {
        SRL::Debug::Print(1, 8, "Current Directory: %s", currentDir);
    }

    // Open root directory
    SRL::Debug::Print(1, 10, "Listing Root Directory (/) Contents:");
    SRL::Debug::Print(1, 11, "----------------------------------------");

    SRL::DevCart::SD::Directory dir;
    if (dir.Open("/"))
    {
        SRL::DevCart::SD::DirectoryEntry entry;
        int line = 12;

        while (dir.Read(entry))
        {
            // Limit output to fit the screen
            if (line >= 26)
            {
                SRL::Debug::Print(1, line++, "... (too many entries to show)");
                break;
            }

            if (entry.IsDirectory)
            {
                SRL::Debug::Print(1, line++, " [DIR]  %s", entry.Name);
            }
            else
            {
                // Format date (MS-DOS format: YYYYYYYMMMMDDDDD where Y is offset from 1980)
                const uint16_t year = ((entry.Date >> 9) & 0x7F) + 1980;
                const uint16_t month = (entry.Date >> 5) & 0x0F;
                const uint16_t day = entry.Date & 0x1F;

                SRL::Debug::Print(1, line++, " [FILE] %s (%lu bytes) %02u/%02u/%04u",
                    entry.Name, entry.Size, day, month, year);
            }
        }
        dir.Close();
    }
    else
    {
        SRL::Debug::Print(1, 12, "Error: Failed to open root directory.");
    }

    // Main loop
    while (true)
    {
        SRL::Core::Synchronize();
    }

    return 0;
}
