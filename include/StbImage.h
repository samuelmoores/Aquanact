#pragma once
#ifndef __STBIMAGE_H
#define __STBIMAGE_H
#include "stb_image.h"
#include <memory>
#include <string>
class StbImage
{
    int m_width, m_height, m_bpp;
    std::unique_ptr<unsigned char[]> m_data = nullptr;

public:
    StbImage();

    void loadFromFile(const std::string& filepath);

    int getWidth() const;
    int getHeight() const;
    int getBpp() const;
    unsigned char* getData() const;
};

#endif