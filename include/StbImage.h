#pragma once
#ifndef __STBIMAGE_H
#define __STBIMAGE_H
#include "stb_image.h"
#include <memory>
#include <string>
#include <assimp/scene.h>


class StbImage
{
    int m_width, m_height, m_bpp, m_channels;
    std::unique_ptr<unsigned char[]> m_data = nullptr;

public:
    StbImage();

    void loadFromFile(const std::string& filepath);
    void loadFromMemory(aiTexture* embeddedTexture);

    int getWidth() const;
    int getHeight() const;
    int getBpp() const;
    unsigned char* getData() const;
};

#endif