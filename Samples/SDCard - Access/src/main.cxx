#include <srl.hpp>
#include <srl_devcart_sdcard.hpp>

// Using to shorten names for Vector and HighColor
using namespace SRL::Types;
using namespace SRL::Math::Types;

// Simple moving point structure for distorted sprite animation
struct MovingPoint
{
    /** @brief Point location
     */
    Vector2D Point;

    /** @brief Point velocity vector
     */
    Vector2D Velocity;
};

/**
 * @brief Helper function to load an uncompressed 24-bit TGA image from the SD card FAT filesystem.
 *
 * @param filename Path to the file on the SD card (e.g. "/TEST.TGA" or "TEST.TGA")
 * @param width Reference to store the image width
 * @param height Reference to store the image height
 * @param pixelData Reference to store the pointer to the allocated pixel buffer
 * @return true if loading and decoding succeeded, false otherwise
 */
bool LoadTGAFromSD(const char* filename, uint16_t& width, uint16_t& height, HighColor*& pixelData)
{
    // Ensure SGCLIB and FAT filesystem are initialized and mounted
    if (!SRL::DevCart::SD::EnsureSgclibReady())
    {
        SRL::Debug::Print(1, 3, "SD Card error: SGCLIB init/mount failed.");
        return false;
    }

    SRL::DevCart::SD::File file;
    if (!file.Open(filename, SRL::DevCart::SD::FileMode::Read))
    {
        SRL::Debug::Print(1, 3, "SD Card error: Failed to open file.");
        return false;
    }

    // TGA header is 18 bytes
    uint8_t header[18];
    size_t readBytes = 0;
    if (!file.Read(header, sizeof(header), readBytes) || readBytes != 18)
    {
        SRL::Debug::Print(1, 3, "SD Card error: Failed to read header.");
        file.Close();
        return false;
    }

    // Parse image parameters from header
    // Width is at offset 12 (16-bit little-endian)
    width = header[12] | (header[13] << 8);
    // Height is at offset 14 (16-bit little-endian)
    height = header[14] | (header[15] << 8);
    // Pixel depth is at offset 16 (bits per pixel)
    uint8_t bitsPerPixel = header[16];
    // Image type is at offset 2 (2 = uncompressed true-color)
    uint8_t imageType = header[2];

    if (imageType != 2 || bitsPerPixel != 24)
    {
        SRL::Debug::Print(1, 3, "SD Card error: Unsupported TGA format.");
        file.Close();
        return false;
    }

    // Skip image ID if present (length is at offset 0)
    uint8_t idLength = header[0];
    if (idLength > 0)
    {
        file.Seek(18 + idLength);
    }

    // Allocate memory for HighColor pixels (width * height * sizeof(HighColor))
    uint32_t numPixels = width * height;
    pixelData = new HighColor[numPixels];

    // Read BGR 24-bit pixel data from file in chunks to save memory
    const uint32_t chunkSize = 256;
    uint8_t buffer[chunkSize * 3];
    uint32_t pixelsLoaded = 0;

    // Check if the image is stored bottom-to-top or top-to-bottom
    // Descriptor bit 5 (0x20) indicates vertical direction: 0 = bottom-to-top, 1 = top-to-bottom
    bool flipY = (header[17] & 0x20) == 0;

    while (pixelsLoaded < numPixels)
    {
        uint32_t toReadPixels = numPixels - pixelsLoaded;
        if (toReadPixels > chunkSize)
        {
            toReadPixels = chunkSize;
        }

        if (!file.Read(buffer, toReadPixels * 3, readBytes) || readBytes != toReadPixels * 3)
        {
            SRL::Debug::Print(1, 3, "SD Card error: Pixel read failed.");
            delete[] pixelData;
            pixelData = nullptr;
            file.Close();
            return false;
        }

        for (uint32_t i = 0; i < toReadPixels; ++i)
        {
            uint8_t b = buffer[i * 3 + 0];
            uint8_t g = buffer[i * 3 + 1];
            uint8_t r = buffer[i * 3 + 2];

            uint32_t pixelIndex = pixelsLoaded + i;
            uint32_t x = pixelIndex % width;
            uint32_t y = pixelIndex / width;

            // Flip Y coordinate if the TGA is stored bottom-up
            uint32_t targetY = flipY ? (height - 1 - y) : y;
            uint32_t targetIndex = targetY * width + x;

            // Convert BGR888 to Saturn HighColor (ABGR1555) format
            pixelData[targetIndex] = HighColor(r, g, b);
        }

        pixelsLoaded += toReadPixels;
    }

    file.Close();
    return true;
}

// Main program entry
int main()
{
    // Initialize library
    SRL::Core::Initialize(HighColor::Colors::Black);

    uint16_t width = 40;
    uint16_t height = 40;
    HighColor* pixelData = nullptr;
    int32_t textureIndex = -1;

    // Load uncompressed TGA texture from the SD card root directory or SD_TEST folder
    if (!LoadTGAFromSD("/SD_TEST/TEST.TGA", width, height, pixelData) &&
        !LoadTGAFromSD("/TEST.TGA", width, height, pixelData) &&
        !LoadTGAFromSD("TEST.TGA", width, height, pixelData))
    {
        width = 40;
        height = 40;
        pixelData = new HighColor[40 * 40];
        for (int y = 0; y < 40; ++y)
        {
            for (int x = 0; x < 40; ++x)
            {
                bool checker = ((x / 8) + (y / 8)) % 2 == 0;
                pixelData[y * 40 + x] = checker ? HighColor(255, 0, 0) : HighColor(0, 0, 255);
            }
        }
    }

    textureIndex = SRL::VDP1::TryLoadTexture(width, height, SRL::CRAM::TextureColorMode::RGB555, 0, pixelData);
    delete[] pixelData;

    // Get screen boundaries
    const int16_t halfWidth = SRL::TV::Width >> 1;
    Fxp minimumWidth = (int16_t)-halfWidth;
    Fxp maximumWidth = halfWidth;

    const int16_t halfHeight = SRL::TV::Height >> 1;
    Fxp minimumHeight = (int16_t)-halfHeight;
    Fxp maximumHeight = halfHeight;

    // Initialize random number generator
    auto rnd = SRL::Math::Random<int16_t>(1234);

    // Initial movement vectors
    Vector2D velocities[] =
        {
            Vector2D(-1.0, 1.0),
            Vector2D(1.0, 1.0),
            Vector2D(-1.0, -1.0),
            Vector2D(1.0, -1.0)};

    // Define moving sprite points for distortion effect
    MovingPoint points[] =
        {
            {
                Vector2D(rnd.GetNumber(-halfWidth, halfWidth), rnd.GetNumber(-halfHeight, halfHeight)),
                Vector2D(velocities[rnd.GetNumber(0, 3)]),
            },
            {
                Vector2D(rnd.GetNumber(-halfWidth, halfWidth), rnd.GetNumber(-halfHeight, halfHeight)),
                Vector2D(velocities[rnd.GetNumber(0, 3)]),
            },
            {
                Vector2D(rnd.GetNumber(-halfWidth, halfWidth), rnd.GetNumber(-halfHeight, halfHeight)),
                Vector2D(velocities[rnd.GetNumber(0, 3)]),
            },
            {
                Vector2D(rnd.GetNumber(-halfWidth, halfWidth), rnd.GetNumber(-halfHeight, halfHeight)),
                Vector2D(velocities[rnd.GetNumber(0, 3)]),
            },
        };

    // Main game/program loop
    while (true)
    {
        Vector2D spritePoints[4];

        // Move the corners of the distorted sprite
        for (uint8_t point = 0; point < 4; point++)
        {
            points[point].Point += points[point].Velocity;

            // Bounce off horizontal screen bounds
            if (points[point].Point.X < minimumWidth || points[point].Point.X > maximumWidth)
            {
                points[point].Velocity.X *= -1.0;
            }

            // Bounce off vertical screen bounds
            if (points[point].Point.Y < minimumHeight || points[point].Point.Y > maximumHeight)
            {
                points[point].Velocity.Y *= -1.0;
            }

            spritePoints[point] = points[point].Point;
        }

        // Draw the distorted sprite using our loaded texture if successful
        if (textureIndex >= 0)
        {
            SRL::Scene2D::DrawSprite(textureIndex, spritePoints, 500.0);
        }

        // Synchronize graphics frame rate
        SRL::Core::Synchronize();
    }

    return 0;
}
