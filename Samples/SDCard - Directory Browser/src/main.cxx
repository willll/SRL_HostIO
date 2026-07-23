/**
 * @file main.cxx
 * @brief Sega Saturn SD Card Directory Browser Sample
 * @details Implements a gamepad-controlled filesystem navigator using Sega Saturn Ring Library (SRL).
 * Features folder traversal, paging support, and color-inverted highlight selection via direct CRAM access.
 */

#include <srl.hpp>
#include <srl_devcart_sdcard.hpp>

using namespace SRL::Types;

/**
 * @brief Maximum directory entries that can be loaded into memory.
 */
#define MAX_ENTRIES 256

/**
 * @brief Number of directory entry items rendered per page.
 */
#define ENTRIES_PER_PAGE 15

/**
 * @brief Local structure representing a simplified directory entry.
 */
struct MyDirectoryEntry
{
    char Name[256];      /**< Name of the file or directory. */
    bool IsDirectory;    /**< True if this entry is a directory, false if it is a file. */
    uint32_t Size;       /**< Size of the file in bytes. */
    uint16_t Date;       /**< FatFS-encoded modification date. */
};

/**
 * @brief Global array storing loaded directory entries.
 */
MyDirectoryEntry entries[MAX_ENTRIES];

/**
 * @brief Total count of loaded directory entries.
 */
int numEntries = 0;

/**
 * @brief Currently selected directory entry index.
 */
int selectedIndex = 0;

/**
 * @brief Currently viewed page index (0-based).
 */
int currentPage = 0;

/**
 * @brief Current path of the directory browser on the SD card.
 */
char currentPath[256] = "/";

/**
 * @brief Load directory contents into memory.
 * @details Traverses the given path using SRL DevCart SD API, populating the global entries array.
 * Automatically injects "." (reload) and ".." (parent directory) entries at the beginning of the list.
 * @param path The filesystem path to read.
 */
void LoadDirectory(const char* path)
{
    numEntries = 0;

    // Manually add "." entry
    strcpy(entries[numEntries].Name, ".");
    entries[numEntries].IsDirectory = true;
    entries[numEntries].Size = 0;
    entries[numEntries].Date = 0;
    numEntries++;

    // Manually add ".." entry
    strcpy(entries[numEntries].Name, "..");
    entries[numEntries].IsDirectory = true;
    entries[numEntries].Size = 0;
    entries[numEntries].Date = 0;
    numEntries++;

    SRL::DevCart::SD::Directory dir;
    if (dir.Open(path))
    {
        SRL::DevCart::SD::DirectoryEntry entry;
        while (dir.Read(entry))
        {
            // Skip "." and ".." if returned by the filesystem to avoid duplicates
            if (strcmp(entry.Name, ".") == 0 || strcmp(entry.Name, "..") == 0)
            {
                continue;
            }

            if (numEntries < MAX_ENTRIES)
            {
                strncpy(entries[numEntries].Name, entry.Name, sizeof(entries[numEntries].Name) - 1);
                entries[numEntries].Name[sizeof(entries[numEntries].Name) - 1] = '\0';
                entries[numEntries].IsDirectory = entry.IsDirectory;
                entries[numEntries].Size = entry.Size;
                entries[numEntries].Date = entry.Date;
                numEntries++;
            }
        }
        dir.Close();
    }
}

/**
 * @brief Go up one directory level.
 * @details Modifies the currentPath variable to point to the parent folder,
 * reloads the directory structure, and resets the selection cursor.
 */
void GoUpDirectory()
{
    if (strcmp(currentPath, "/") == 0)
    {
        return; // Already at root
    }

    char* lastSlash = strrchr(currentPath, '/');
    if (lastSlash != nullptr)
    {
        if (lastSlash == currentPath)
        {
            // Root folder
            currentPath[1] = '\0';
        }
        else
        {
            *lastSlash = '\0';
        }
    }
    LoadDirectory(currentPath);
    selectedIndex = 0;
    currentPage = 0;
}

/**
 * @brief Navigate to the selected directory.
 * @details Evaluates the selection: performs directory traversal if the selection is a folder,
 * otherwise does nothing for standard files.
 * @param entry The MyDirectoryEntry to navigate to.
 */
void NavigateTo(const MyDirectoryEntry& entry)
{
    if (!entry.IsDirectory)
    {
        return; // Do nothing for files
    }

    if (strcmp(entry.Name, ".") == 0)
    {
        LoadDirectory(currentPath);
        selectedIndex = 0;
        currentPage = 0;
        return;
    }

    if (strcmp(entry.Name, "..") == 0)
    {
        GoUpDirectory();
        return;
    }

    // Normal folder navigation
    char newPath[256];
    if (strcmp(currentPath, "/") == 0)
    {
        snprintf(newPath, sizeof(newPath), "/%s", entry.Name);
    }
    else
    {
        snprintf(newPath, sizeof(newPath), "%s/%s", currentPath, entry.Name);
    }

    SRL::DevCart::SD::Directory testDir;
    if (testDir.Open(newPath))
    {
        testDir.Close();
        strncpy(currentPath, newPath, sizeof(currentPath) - 1);
        currentPath[sizeof(currentPath) - 1] = '\0';
        LoadDirectory(currentPath);
        selectedIndex = 0;
        currentPage = 0;
    }
}

/**
 * @brief Draw the current page of directory contents.
 * @details Formats and renders the header, current path, page counter, files list,
 * and controls footer. Selected item is highlighted using Palette 1 (Color Inversion).
 */
void DrawScreen()
{
    // Header
    SRL::Debug::Print(1, 1, "========================================");
    SRL::Debug::Print(1, 2, "   SDCard - Directory Browser Sample    ");
    SRL::Debug::Print(1, 3, "========================================");

    // Current Path (truncated if too long to fit comfortably)
    int pathLen = strlen(currentPath);
    SRL::Debug::Print(1, 5, "                                        ");
    if (pathLen > 34)
    {
        SRL::Debug::Print(1, 5, "Path: ...%s", currentPath + (pathLen - 34));
    }
    else
    {
        SRL::Debug::Print(1, 5, "Path: %s", currentPath);
    }

    int totalPages = (numEntries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
    if (totalPages == 0) totalPages = 1;
    SRL::Debug::Print(1, 7, "Page: %d/%d (Total: %d)          ", currentPage + 1, totalPages, numEntries);
    SRL::Debug::Print(1, 8, "----------------------------------------");

    int startIdx = currentPage * ENTRIES_PER_PAGE;
    for (int i = 0; i < ENTRIES_PER_PAGE; ++i)
    {
        int entryIdx = startIdx + i;
        int line = 10 + i;

        if (entryIdx < numEntries)
        {
            const auto& entry = entries[entryIdx];
            bool isSelected = (entryIdx == selectedIndex);

            if (isSelected)
            {
                SRL::ASCII::SetPalette(1); // Set to inverted palette (White bg, Black text)
            }
            else
            {
                SRL::ASCII::SetPalette(0); // Set to normal palette
            }

            char textBuf[64];
            if (entry.IsDirectory)
            {
                snprintf(textBuf, sizeof(textBuf), " [DIR]  %s", entry.Name);
            }
            else
            {
                snprintf(textBuf, sizeof(textBuf), " [FILE] %s (%lu B)", entry.Name, entry.Size);
            }

            // Format line to be exactly 38 characters for full-width highlight block
            int len = strlen(textBuf);
            if (len > 38)
            {
                textBuf[35] = '.';
                textBuf[36] = '.';
                textBuf[37] = '.';
                textBuf[38] = '\0';
            }
            else
            {
                for (int j = len; j < 38; ++j)
                {
                    textBuf[j] = ' ';
                }
                textBuf[38] = '\0';
            }

            SRL::Debug::Print(1, line, textBuf);
        }
        else
        {
            // Clear empty slots in the list
            SRL::ASCII::SetPalette(0);
            SRL::Debug::Print(1, line, "                                      ");
        }
    }

    // Always restore normal palette
    SRL::ASCII::SetPalette(0);

    // Controls footer
    SRL::Debug::Print(1, 26, "----------------------------------------");
    SRL::Debug::Print(1, 27, "Pad Up/Down: Select | A: Open | B: Back ");
    SRL::Debug::Print(1, 28, "Pad L/R Trigger: Page Up/Down           ");
}

/**
 * @brief Application Entry Point.
 * @details Initializes the VDP2 video system, configures CRAM palettes, mounts the SD filesystem,
 * and enters the main gamepad-polling and rendering loop.
 * @return 0 on successful termination.
 */
int main()
{
    // Initialize library with black background
    SRL::Core::Initialize(HighColor::Colors::Black);

    // Disable transparent pixels for NBG3 layer so color index 0 is rendered
    SRL::VDP2::NBG3::TransparentDisable();

    // Initialize normal text palette (index 0) and inverted text palette (index 1) directly in CRAM.
    // In 16-color mode (COL_TYPE_16), NBG3 palette banks are spaced by 16 words (32 bytes).
    // Therefore:
    // - Palette Bank 0 starts at word 0
    // - Palette Bank 1 starts at word 16
    volatile uint16_t* cram = (volatile uint16_t*)0x25f00000;
    
    // Bank 0: Normal text (Black background, White foreground)
    cram[0] = static_cast<uint16_t>(HighColor::Colors::Black);
    cram[1] = static_cast<uint16_t>(HighColor::Colors::White);

    // Bank 1: Inverted text (White background, Black foreground)
    cram[16] = static_cast<uint16_t>(HighColor::Colors::White);
    cram[17] = static_cast<uint16_t>(HighColor::Colors::Black);

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

    // Load initial directory listing
    LoadDirectory(currentPath);

    // Initialize controller on port 0
    SRL::Input::Digital pad(0);

    while (true)
    {
        if (pad.IsConnected())
        {
            if (pad.WasPressed(SRL::Input::Digital::Button::Down))
            {
                if (numEntries > 0)
                {
                    selectedIndex = (selectedIndex + 1) % numEntries;
                    currentPage = selectedIndex / ENTRIES_PER_PAGE;
                }
            }
            else if (pad.WasPressed(SRL::Input::Digital::Button::Up))
            {
                if (numEntries > 0)
                {
                    selectedIndex = (selectedIndex - 1 + numEntries) % numEntries;
                    currentPage = selectedIndex / ENTRIES_PER_PAGE;
                }
            }
            else if (pad.WasPressed(SRL::Input::Digital::Button::A))
            {
                if (numEntries > 0)
                {
                    NavigateTo(entries[selectedIndex]);
                }
            }
            else if (pad.WasPressed(SRL::Input::Digital::Button::B))
            {
                GoUpDirectory();
            }
            else if (pad.WasPressed(SRL::Input::Digital::Button::R))
            {
                // Page Down
                int totalPages = (numEntries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
                if (totalPages > 1)
                {
                    currentPage = (currentPage + 1) % totalPages;
                    selectedIndex = currentPage * ENTRIES_PER_PAGE;
                    if (selectedIndex >= numEntries)
                    {
                        selectedIndex = numEntries - 1;
                    }
                }
            }
            else if (pad.WasPressed(SRL::Input::Digital::Button::L))
            {
                // Page Up
                int totalPages = (numEntries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
                if (totalPages > 1)
                {
                    currentPage = (currentPage - 1 + totalPages) % totalPages;
                    selectedIndex = currentPage * ENTRIES_PER_PAGE;
                    if (selectedIndex >= numEntries)
                    {
                        selectedIndex = numEntries - 1;
                    }
                }
            }
        }

        // Draw the directory list page
        DrawScreen();

        // Refresh screen
        SRL::Core::Synchronize();
    }

    return 0;
}
