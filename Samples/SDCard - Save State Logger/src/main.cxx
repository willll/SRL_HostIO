#include <srl.hpp>
#include <srl_devcart_sdcard.hpp>

using namespace SRL::Types;

// Structure representing the game save state to persist
struct SaveState
{
    char Signature[4];     // "SRL!"
    uint32_t PlayTime;     // Total play time in seconds
    int32_t PlayerX;       // Player X coordinate
    int32_t PlayerY;       // Player Y coordinate
    uint16_t Health;       // Player health points
    uint16_t LevelNum;     // Current level number
};

// Program entry point
int main()
{
    // Initialize library with black background
    SRL::Core::Initialize(HighColor::Colors::Black);

    SRL::Debug::Print(1, 1, "========================================");
    SRL::Debug::Print(1, 2, "    SDCard - Save State Logger Sample   ");
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

    const char* filename = "/SAVE.DAT";
    SaveState state{};
    bool loaded = false;

    // 1. Attempt to read an existing save state
    SRL::Debug::Print(1, 8, "Checking for existing save: %s...", filename);
    SRL::DevCart::SD::File file;
    if (file.Open(filename, SRL::DevCart::SD::FileMode::Read))
    {
        size_t bytesRead = 0;
        if (file.Read(&state, sizeof(state), bytesRead) && bytesRead == sizeof(state))
        {
            // Verify signature
            if (state.Signature[0] == 'S' && state.Signature[1] == 'R' &&
                state.Signature[2] == 'L' && state.Signature[3] == '!')
            {
                loaded = true;
                SRL::Debug::Print(1, 9, "Save state loaded successfully!");
                SRL::Debug::Print(1, 10, "Playtime : %lu seconds", state.PlayTime);
                SRL::Debug::Print(1, 11, "Position : (%ld, %ld)", state.PlayerX, state.PlayerY);
                SRL::Debug::Print(1, 12, "Health   : %u HP", state.Health);
                SRL::Debug::Print(1, 13, "Level    : %u", state.LevelNum);
            }
            else
            {
                SRL::Debug::Print(1, 9, "Warning: Save state signature mismatch.");
            }
        }
        else
        {
            SRL::Debug::Print(1, 9, "Failed to read save state data.");
        }
        file.Close();
    }
    else
    {
        SRL::Debug::Print(1, 8, "No existing save file found.");
    }

    // 2. Prepare new data to write (incrementing/simulating progress)
    SRL::Debug::Print(1, 15, "Preparing new save state data...");
    SaveState newState{};
    newState.Signature[0] = 'S';
    newState.Signature[1] = 'R';
    newState.Signature[2] = 'L';
    newState.Signature[3] = '!';

    if (loaded)
    {
        newState.PlayTime = state.PlayTime + 120; // Simulated 2 minutes of gameplay
        newState.PlayerX = state.PlayerX + 50;
        newState.PlayerY = state.PlayerY - 20;
        newState.Health = 100;
        newState.LevelNum = state.LevelNum + 1;
    }
    else
    {
        newState.PlayTime = 0;
        newState.PlayerX = 1000;
        newState.PlayerY = 2000;
        newState.Health = 100;
        newState.LevelNum = 1;
    }

    // 3. Write the new save state
    SRL::Debug::Print(1, 17, "Writing new save to %s...", filename);
    if (file.Open(filename, SRL::DevCart::SD::FileMode::Write))
    {
        size_t bytesWritten = 0;
        if (file.Write(&newState, sizeof(newState), bytesWritten) && bytesWritten == sizeof(newState))
        {
            SRL::Debug::Print(1, 18, "Save state written successfully!");
        }
        else
        {
            SRL::Debug::Print(1, 18, "Error: Failed to write save state.");
        }
        file.Close();
    }
    else
    {
        SRL::Debug::Print(1, 17, "Error: Failed to open file for writing.");
    }

    // 4. Retrieve and print file size using Stat
    SRL::DevCart::SD::DirectoryEntry entry{};
    if (SRL::DevCart::SD::g_fatfsBackend.Stat(filename, entry))
    {
        SRL::Debug::Print(1, 20, "Verify file on SD Card via Stat():");
        SRL::Debug::Print(1, 21, "Name: %s, Size: %lu bytes", entry.Name, entry.Size);
    }

    // Main loop
    while (true)
    {
        SRL::Core::Synchronize();
    }

    return 0;
}
