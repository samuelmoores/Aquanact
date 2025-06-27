#define STB_IMAGE_IMPLEMENTATION
#include "StbImage.h"

#include <string>
#include <iostream>

StbImage::StbImage() : m_width(0), m_height(0), m_bpp(0) {
}

void StbImage::loadFromFile(const std::string& filepath) {
    unsigned char* data = stbi_load(filepath.c_str(), &m_width, &m_height, &m_bpp, 4);

    if (data == nullptr)
        throw std::runtime_error("Could not load file " + filepath);

    m_data = std::unique_ptr<unsigned char[]>(data);
}

int StbImage::getWidth() const { return m_width; }

int StbImage::getHeight() const { return m_height; }

int StbImage::getBpp() const { return m_bpp; }

unsigned char* StbImage::getData() const { return m_data.get(); }