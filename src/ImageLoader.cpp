// ImageLoader.cpp
#include "ImageLoader.h"
#include "PathUtils.h"

// Include stbi ONLY in this .cpp file
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void ImageData::free() {
    if (pixels != nullptr) {
        stbi_image_free(pixels);
        pixels = nullptr;
    }
}

ImageData ImageLoader::load(const std::string& filePath, int desiredChannels) {
    ImageData data;
    std::filesystem::path resolvedPath = ResolveFromExeDir(filePath);
    data.pixels = stbi_load(resolvedPath.string().c_str(), &data.width, &data.height,
        &data.channels, desiredChannels);

    if (!data.pixels) {
        throw std::runtime_error("Failed to load image: " + filePath);
    }

    // Update channels if forced
    if (desiredChannels > 0) {
        data.channels = desiredChannels;
    }

    return data;
}

void ImageLoader::setFlipVerticallyOnLoad(bool flip) {
    stbi_set_flip_vertically_on_load(flip);
}
