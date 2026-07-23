#include <srl.hpp>
#include <srl_devcart_sdcard.hpp>

using namespace SRL::Types;

// Program entry point
int main()
{
    // Initialize library with black background
    SRL::Core::Initialize(HighColor::Colors::Black);

    // Capture the initial time
    SRL::Tickstamp last_toggle = SRL::Timer::Capture();
    bool led_state = false;

    // Main loop
    while (true)
    {
        // Capture current time
        SRL::Tickstamp now = SRL::Timer::Capture();
        SRL::Tickstamp elapsed = now - last_toggle;

        // Toggle state every 250 milliseconds (2 blinks / 4 state changes per second)
        if (elapsed.ToMilliseconds().As<int>() >= 250)
        {
            led_state = !led_state;

            // Set both Green and Red LEDs on the SD Card reader
            SRL::DevCart::SD::SetLedGreen(led_state);
            SRL::DevCart::SD::SetLedRed(led_state);

            last_toggle = now;
        }

        // Print header info
        SRL::Debug::Print(1, 1, "========================================");
        SRL::Debug::Print(1, 2, "       SDCard - LED Blink Sample        ");
        SRL::Debug::Print(1, 3, "========================================");

        // Print LED status
        SRL::Debug::Print(1, 5, "Blinking LEDs 2 times per second (250ms)");
        SRL::Debug::Print(1, 7, "Green LED: %s", led_state ? "ON " : "OFF");
        SRL::Debug::Print(1, 8, "Red LED:   %s", led_state ? "ON " : "OFF");

        // Print running clock time
        auto clock = now.ToClock();
        SRL::Debug::Print(1, 11, "Uptime: %02u:%02u:%02u.%03u",
            clock.Hours(),
            clock.Minutes(),
            clock.Seconds(),
            clock.Milliseconds());

        // Synchronize and refresh screen
        SRL::Core::Synchronize();
    }

    return 0;
}
