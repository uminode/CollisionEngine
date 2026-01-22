// ImageLoader.h
#pragma once
#include <string>
#include <vector>
#include <filesystem>

struct ImageData {
    uint8_t* pixels = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;

    ~ImageData() {
        free(); // Auto-cleanup
    }

    void free();
    bool isValid() const { return pixels != nullptr; }
};

class ImageLoader {
public:
    // Load image with desired channel count (0 = keep original, 4 = force RGBA)
    static ImageData load(const std::string& filePath, int desiredChannels = 0);

    // Set flip on load (useful for OpenGL vs D3D coordinate differences)
    static void setFlipVerticallyOnLoad(bool flip);
};
