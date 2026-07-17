#include <stddef.h>
#include <stdint.h>
#include <vector>
#include <srl.hpp>
#include <srl_log.hpp>

// Display utils
#include "display.hpp"

// Test suite
#include "testHostIO.hpp"

// Define tags for test start and end
const char *const StrStart = "***UT_START***";
const char *const StrEnd = "***UT_END***";

int main()
{
    SRL::Input::Digital pad(0);
    size_t startIndex = 0;

    // Initialize SRL core
    SRL::Core::Initialize(SRL::Types::HighColor(20, 10, 50));
    ASCII::Clear();

    // Tag the beginning of the tests
    LogInfo(StrStart);
    PushResultLine(StrStart);

    // Run hostio test suite
    RUN_AND_DISPLAY_SUITE(hostio_test_suite);

    // Generate tests report
    MU_REPORT();

    // Display test statistics
    BuildStatsLine(resultsBuffer, BufferSize,
        static_cast<unsigned int>(minunit_run),
        static_cast<unsigned int>(minunit_assert),
        static_cast<unsigned int>(minunit_fail));

    PushResultLine(resultsBuffer);

    // Tag the end of the tests
    LogInfo(StrEnd);
    PushResultLine(StrEnd);

    if (gResults.size() > DisplayLines)
    {
        startIndex = gResults.size() - DisplayLines;
    }

    RenderResults(startIndex);

    // Main program loop with scrolling support
    uint8_t upHoldFrames = 0;
    uint8_t downHoldFrames = 0;
    const uint8_t repeatDelay = 20;
    const uint8_t repeatRate = 3;

    while (1)
    {
        SRL::Core::Synchronize();
        bool refresh = false;

        const bool upHeld = pad.IsHeld(SRL::Input::Digital::Button::Up);
        const bool downHeld = pad.IsHeld(SRL::Input::Digital::Button::Down);

        if (upHeld && !downHeld)
        {
            if (upHoldFrames < 255)
            {
                ++upHoldFrames;
            }
            downHoldFrames = 0;
        }
        else if (downHeld && !upHeld)
        {
            if (downHoldFrames < 255)
            {
                ++downHoldFrames;
            }
            upHoldFrames = 0;
        }
        else
        {
            upHoldFrames = 0;
            downHoldFrames = 0;
        }

        if (pad.WasPressed(SRL::Input::Digital::Button::Up))
        {
            if (startIndex > 0)
            {
                --startIndex;
                refresh = true;
            }
        }
        else if (upHeld && upHoldFrames > repeatDelay &&
                 ((upHoldFrames - repeatDelay) % repeatRate == 0))
        {
            if (startIndex > 0)
            {
                --startIndex;
                refresh = true;
            }
        }

        if (pad.WasPressed(SRL::Input::Digital::Button::Down))
        {
            if (startIndex + DisplayLines < gResults.size())
            {
                ++startIndex;
                refresh = true;
            }
        }
        else if (downHeld && downHoldFrames > repeatDelay &&
                 ((downHoldFrames - repeatDelay) % repeatRate == 0))
        {
            if (startIndex + DisplayLines < gResults.size())
            {
                ++startIndex;
                refresh = true;
            }
        }

        if (refresh)
        {
            RenderResults(startIndex);
        }
    }

    return 0;
}
