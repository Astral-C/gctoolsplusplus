#include "Bti.hpp"
#include "Util.hpp"
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <tuple>

namespace ColorFormat {

// a few of these are based off of wwlib texture utils by LagoLunatic
uint8_t toGrayscale8(uint8_t r, uint8_t g, uint8_t  b){
    return std::round(((r * 30) + (g * 59) + (b * 11)) / 100);
}

uint16_t toGrayscale(uint8_t r, uint8_t g, uint8_t  b){
    return std::round(((r * 30) + (g * 59) + (b * 11)) / 100);
}

uint8_t RGBA8toI4(uint8_t r, uint8_t g, uint8_t  b, uint8_t a){
    uint8_t greyscale = toGrayscale8(r, g, b);
    return (greyscale >> 4) & 0xF;
}

uint8_t RGBA8toIA4(uint8_t r, uint8_t g, uint8_t  b, uint8_t a){
    uint16_t greyscale = toGrayscale(r, g, b);
    uint8_t output = 0;
    output |= (greyscale >> 4) & 0xF;
    output |= (a & 0xF0);
    return output;
}

uint8_t RGBA8toI8(uint8_t r, uint8_t g, uint8_t  b, uint8_t a){
    uint16_t greyscale = toGrayscale(r, g, b);
    return greyscale & 0xFF;
}

uint16_t RGBA8toIA8(uint8_t r, uint8_t g, uint8_t  b, uint8_t a){
    uint16_t greyscale = toGrayscale(r, g, b);
    uint16_t output = 0x0000;
    output |= greyscale & 0x00FF;
    output |= (a << 8) & 0xFF00;
    return output;
}

uint16_t RGBA8toRGB565(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    uint16_t color = 0x0000;
    color |= ((r >> 3) & 0x1F) << 11;
    color |= ((g >> 2) & 0x3F) << 5;
    color |= ((b >> 3) & 0x1F) << 0;
	return color;
}

uint16_t RGBA8toRGB5A3(uint8_t r, uint8_t g, uint8_t  b, uint8_t a){
    uint16_t color = 0x0000;
    if(a != 255){
        uint8_t na = a >> 5;
        uint8_t nr = r >> 4;
        uint8_t ng = g >> 4;
        uint8_t nb = b >> 4;
        color = 0x0000;
        color |= ((na & 0x7) << 12);
        color |= ((nr & 0xF) << 8);
        color |= ((ng & 0xF) << 4);
        color |= ((nb & 0xF) << 0);
    } else {
        uint8_t nr = r >> 3;
        uint8_t ng = g >> 3;
        uint8_t nb = b >> 3;
        color = 0x8000;
        color |= ((nr & 0x1F) << 10);
        color |= ((ng & 0x1F) << 5);
        color |= ((nb & 0x1F) << 0);
    }
    return color;
}

uint32_t RGB565toRGBA8(uint16_t data) {
	uint8_t r = (data & 0xF100) >> 11;
	uint8_t g = (data & 0x07E0) >> 5;
	uint8_t b = (data & 0x001F);

	uint32_t output = 0x000000FF;
	output |= (r << 3) << 24;
	output |= (g << 2) << 16;
	output |= (b << 3) << 8;

	return output;
}

uint32_t RGB5A3toRGBA8(uint16_t data) {
	uint8_t r, g, b, a;

	// No alpha bits to extract.
	if (data & 0x8000) {
		a = 0xFF;

		r = (data & 0x7C00) >> 10;
		g = (data & 0x03E0) >> 5;
		b = (data & 0x001F);

		r = (r << (8 - 5)) | (r >> (10 - 8));
		g = (g << (8 - 5)) | (g >> (10 - 8));
		b = (b << (8 - 5)) | (b >> (10 - 8));
	}
	// Alpha bits present.
	else {
		a = (data & 0x7000) >> 12;
		r = (data & 0x0F00) >> 8;
		g = (data & 0x00F0) >> 4;
		b = (data & 0x000F);

		a = (a << (8 - 3)) | (a << (8 - 6)) | (a >> (9 - 8));
		r = (r << (8 - 4)) | r;
		g = (g << (8 - 4)) | g;
		b = (b << (8 - 4)) | b;
	}

	uint32_t output = a;
	output |= r << 24;
	output |= g << 16;
	output |= b << 8;

	return output;
}

uint8_t* DecodeCMPRSubBlock(bStream::CStream* stream) {
	uint8_t* data = new uint8_t[4 * 4 * 4]{};

	uint16_t color0 = stream->readUInt16();
	uint16_t color1 = stream->readUInt16();
	uint32_t bits = stream->readUInt32();

	uint32_t colorTable[4]{};
	colorTable[0] = ColorFormat::RGB565toRGBA8(color0);
	colorTable[1] = ColorFormat::RGB565toRGBA8(color1);

	uint8_t r0, g0, b0, a0, r1, g1, b1, a1;
	r0 = (colorTable[0] & 0xFF000000) >> 24;
	g0 = (colorTable[0] & 0x00FF0000) >> 16;
	b0 = (colorTable[0] & 0x0000FF00) >> 8;
	a0 = (colorTable[0] & 0x000000FF);

	r1 = (colorTable[1] & 0xFF000000) >> 24;
	g1 = (colorTable[1] & 0x00FF0000) >> 16;
	b1 = (colorTable[1] & 0x0000FF00) >> 8;
	a1 = (colorTable[1] & 0x000000FF);

	if (color0 > color1) {
		colorTable[2] |= ((2 * r0 + r1) / 3) << 24;
		colorTable[2] |= ((2 * g0 + g1) / 3) << 16;
		colorTable[2] |= ((2 * b0 + b1) / 3) << 8;
		colorTable[2] |= 0xFF;

		colorTable[3] |= ((r0 + 2 * r1) / 3) << 24;
		colorTable[3] |= ((g0 + 2 * g1) / 3) << 16;
		colorTable[3] |= ((b0 + 2 * b1) / 3) << 8;
		colorTable[3] |= 0xFF;
	}
	else {
		colorTable[2] |= ((r0 + r1) / 2) << 24;
		colorTable[2] |= ((g0 + g1) / 2) << 16;
		colorTable[2] |= ((b0 + b1) / 2) << 8;
		colorTable[2] |= 0xFF;

		colorTable[3] |= ((r0 + 2 * r1) / 3) << 24;
		colorTable[3] |= ((g0 + 2 * g1) / 3) << 16;
		colorTable[3] |= ((b0 + 2 * b1) / 3) << 8;
		colorTable[3] |= 0x00;
	}

	for (int pixelY = 0; pixelY < 4; pixelY++) {
		for (int pixelX = 0; pixelX < 4; pixelX++) {
			uint32_t i = pixelY * 4 + pixelX;
			uint32_t bitOffset = (15 - i) * 2;
			uint32_t di = i * 4;
			uint32_t si = (bits >> bitOffset) & 3;

			data[di + 0] = (colorTable[si] & 0xFF000000) >> 24;
			data[di + 1] = (colorTable[si] & 0x00FF0000) >> 16;
			data[di + 2] = (colorTable[si] & 0x0000FF00) >> 8;
			data[di + 3] = (colorTable[si] & 0x000000FF);
		}
	}

	return data;
}

}

namespace ImageFormat {

namespace Decode {
    void CMPR(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData){
        if (imageData == nullptr)
            return;

        uint32_t numBlocksW = (width + 7) / 8;
        uint32_t numBlocksH = (height + 7) / 8;

        // Iterate the blocks in the image
        for (int blockY = 0; blockY < numBlocksH; blockY++) {
            for (int blockX = 0; blockX < numBlocksW; blockX++) {
                // Each block has a set of 2x2 sub-blocks.
                for (int subBlockY = 0; subBlockY < 2; subBlockY++) {
                    for (int subBlockX = 0; subBlockX < 2; subBlockX++) {
                        uint32_t subBlockWidth = std::max(0, std::min(4, width - (subBlockX * 4 + blockX * 8)));
                        uint32_t subBlockHeight = std::max(0, std::min(4, height - (subBlockY * 4 + blockY * 8)));

                        uint8_t* subBlockData = ColorFormat::DecodeCMPRSubBlock(stream);

                        for (int pixelY = 0; pixelY < subBlockHeight; pixelY++) {
                            uint32_t destX = blockX * 8 + subBlockX * 4;
                            uint32_t destY = blockY * 8 + (subBlockY * 4) + pixelY;

                            if (destX >= width || destY >= height)
                                continue;

                            uint32_t destOffset = (destY * width + destX) * 4;
                            memcpy(imageData + destOffset, subBlockData + (pixelY * 4 * 4), subBlockWidth * 4);
                        }

                        delete[] subBlockData;
                    }
                }
            }
        }
    }

    void RGB5A3(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData){
        if (imageData == nullptr)
            return;

        uint32_t numBlocksW = width / 4;
        uint32_t numBlocksH = height / 4;

        // Iterate the blocks in the image
        for (int blockY = 0; blockY < numBlocksH; blockY++) {
            for (int blockX = 0; blockX < numBlocksW; blockX++) {
                // Iterate the pixels in the current block
                for (int pixelY = 0; pixelY < 4; pixelY++) {
                    for (int pixelX = 0; pixelX < 4; pixelX++) {
                        // Bounds check to ensure the pixel is within the image.
                        uint16_t data = stream->readUInt16();
                        if ((blockX * 4 + pixelX >= width) || (blockY * 4 + pixelY >= height))
                            continue;

                        // RGB values for this pixel are stored in a 16-bit integer.
                        uint32_t rgba8 = ColorFormat::RGB5A3toRGBA8(data);

                        uint32_t destIndex = (width * ((blockY * 4) + pixelY) + (blockX * 4) + pixelX) * 4;

                        imageData[destIndex] = (rgba8 & 0xFF000000) >> 24;
                        imageData[destIndex + 1] = (rgba8 & 0x00FF0000) >> 16;
                        imageData[destIndex + 2] = (rgba8 & 0x0000FF00) >> 8;
                        imageData[destIndex + 3] = rgba8 & 0x000000FF;
                    }
                }
            }
        }
    }

    void RGB565(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData){
        if (imageData == nullptr)
            return;

        uint32_t numBlocksW = width / 4;
        uint32_t numBlocksH = height / 4;

        // Iterate the blocks in the image
        for (int blockY = 0; blockY < numBlocksH; blockY++) {
            for (int blockX = 0; blockX < numBlocksW; blockX++) {
                // Iterate the pixels in the current block
                for (int pixelY = 0; pixelY < 4; pixelY++) {
                    for (int pixelX = 0; pixelX < 4; pixelX++) {
                        // Bounds check to ensure the pixel is within the image.
                        if ((blockX * 4 + pixelX >= width) || (blockY * 4 + pixelY >= height))
                            continue;

                        // RGB values for this pixel are stored in a 16-bit integer.
                        uint16_t data = stream->readUInt16();
                        uint32_t rgba8 = ColorFormat::RGB565toRGBA8(data);

                        uint32_t destIndex = (width * ((blockY * 4) + pixelY) + (blockX * 4) + pixelX) * 4;

                        imageData[destIndex] = (rgba8 & 0xFF000000) >> 24;
                        imageData[destIndex + 1] = (rgba8 & 0x00FF0000) >> 16;
                        imageData[destIndex + 2] = (rgba8 & 0x0000FF00) >> 8;
                        imageData[destIndex + 3] = rgba8 & 0x000000FF;
                    }
                }
            }
        }
    }

    void I4(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData){
        if(imageData == nullptr) return;

        // Iterate the blocks in the image
        for (int blockY = 0; blockY < height; blockY += 8) {
            for (int blockX = 0; blockX < width; blockX += 8) {
                // Iterate the pixels in the current block
                for (int pixelY = 0; pixelY < 8; pixelY++) {
                    for (int pixelX = 0; pixelX < 8; pixelX += 2) {
                        uint8_t data = stream->readUInt8();

                        // Bounds check to ensure the pixel is within the image.
                        if (blockY + pixelY >= height)
                            continue;

                        // Each byte represents two pixels.
                        uint8_t pixel0 = (data >> 4) & 0xF;
                        uint8_t pixel1 = (data & 0xF);

                        uint32_t destIndex = (width * (blockY + pixelY) + (blockX + pixelX)) * 4;

                        if (blockX + pixelX < width){
                            imageData[destIndex + 0] = (pixel0 << 4) | pixel0;
                            imageData[destIndex + 1] = (pixel0 << 4) | pixel0;
                            imageData[destIndex + 2] = (pixel0 << 4) | pixel0;
                            imageData[destIndex + 3] = (pixel0 << 4) | pixel0;
                        }

                        if (blockX + pixelX + 1 < width){
                            imageData[destIndex + 4] = (pixel1 << 4) | pixel1;
                            imageData[destIndex + 5] = (pixel1 << 4) | pixel1;
                            imageData[destIndex + 6] = (pixel1 << 4) | pixel1;
                            imageData[destIndex + 7] = (pixel1 << 4) | pixel1;
                        }
                    }
                }
            }
        }
    }

    void I8(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData){
        if(imageData == nullptr) return;

        // Iterate the blocks in the image
        for (int blockY = 0; blockY < height; blockY += 4) {
            for (int blockX = 0; blockX < width; blockX += 8) {
                // Iterate the pixels in the current block
                for (int pixelY = 0; pixelY < 4; pixelY++) {
                    for (int pixelX = 0; pixelX < 8; pixelX++) {
                        uint8_t data = stream->readUInt8();
                        // Bounds check to ensure the pixel is within the image.
                        if ((blockX + pixelX >= width) || (blockY + pixelY >= height))
                            continue;


                        uint32_t destIndex = (width * (blockY + pixelY) + (blockX + pixelX)) * 4;

                        imageData[destIndex + 0] = data;
                        imageData[destIndex + 1] = data;
                        imageData[destIndex + 2] = data;
                        imageData[destIndex + 3] = data;
                    }
                }
            }
        }
    }

    void IA4(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData){
        if(imageData == nullptr) return;

        // Iterate the blocks in the image
        for (int blockY = 0; blockY < height; blockY += 4) {
            for (int blockX = 0; blockX < width; blockX += 8) {
                // Iterate the pixels in the current block
                for (int pixelY = 0; pixelY < 4; ++pixelY) {
                    for (int pixelX = 0; pixelX < 8; ++pixelX) {
                        uint8_t data = stream->readUInt8();
                        // Bounds check to ensure the pixel is within the image.
                        if ((blockX + pixelX >= width) || (blockY + pixelY >= height))
                            continue;


                        uint32_t destIndex = (width * (blockY  + pixelY) + (blockX + pixelX)) * 4;

                        uint8_t intensity = data & 0xF;
                        uint8_t alpha = (data >> 4) & 0xF;

                        imageData[destIndex + 0] = (intensity << 4) | intensity;
                        imageData[destIndex + 1] = (intensity << 4) | intensity;
                        imageData[destIndex + 2] = (intensity << 4) | intensity;
                        imageData[destIndex + 3] = (alpha << 4) | alpha;
                    }
                }
            }
        }
    }

    void IA8(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData){
        if(imageData == nullptr) return;

        // Iterate the blocks in the image
        for (int blockY = 0; blockY < height; blockY+=4) {
            for (int blockX = 0; blockX < width; blockX+=8) {
                // Iterate the pixels in the current block
                for (int pixelY = 0; pixelY < 4; pixelY++) {
                    for (int pixelX = 0; pixelX < 8; pixelX++) {
                        uint8_t intensity = stream->readUInt8();
                        uint8_t alpha = stream->readUInt8();

                        // Bounds check to ensure the pixel is within the image.
                        if ((blockX + pixelX >= width) || (blockY + pixelY >= height))
                            continue;


                        uint32_t destIndex = (width * (blockY + pixelY) + (blockX + pixelX)) * 4;

                        imageData[destIndex + 0] = intensity;
                        imageData[destIndex + 1] = intensity;
                        imageData[destIndex + 2] = intensity;
                        imageData[destIndex + 3] = alpha;
                    }
                }
            }
        }
    }
}

namespace Encode {

    // Based off of gclib by lagolunatic
    namespace DXT {
        inline int ColorDist(std::array<uint8_t, 4> a, std::array<uint8_t, 4> b){
            return abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2]);
        }

        std::pair<std::array<uint8_t, 4>, std::array<uint8_t, 4>> GetKeyColors(std::array<std::array<uint8_t, 4>, 16> colors){
            std::pair<std::array<uint8_t, 4>, std::array<uint8_t, 4>> keyColors;

            int maxDist = -1;
            for(int i = 0; i < 16; i++){
                std::array<uint8_t, 4> color0 = colors[i];
                for(int j = i+1; j < 16; j++){
                    std::array<uint8_t, 4> color1 = colors[j];
                    int curDist = ColorDist(color0, color1);
                    if(curDist > maxDist){
                        maxDist = curDist;
                        keyColors.first = color0;
                        keyColors.second = color1;
                    }
                }
            }

            if(maxDist == -1){
                return {{0x00, 0x00, 0x00, 0xFF}, {0xFF, 0xFF, 0xFF, 0xFF}};
            } else {
                int r1 = keyColors.first[0];
                int g1 = keyColors.first[1];
                int b1 = keyColors.first[2];
                int r2 = keyColors.second[0];
                int g2 = keyColors.second[1];
                int b2 = keyColors.second[2];
                keyColors.first[3] = 0xFF;
                keyColors.second[3] = 0xFF;

                if((r1 >> 3) == (r2 >> 3) && (g1 >> 2) == (g2 >> 2) && (b1 >> 3) == (b2 >> 3)){
                    if((r1 >> 3) == 0 && (g1 >> 2) == 0 && (b1 >> 3) == 0){
                        keyColors.second = {0xFF, 0xFF, 0xFF, 0xFF};
                    } else {
                        keyColors.second = {0x00, 0x00, 0x00, 0xFF};
                    }
                }
            }

            return keyColors;
        }

        std::array<std::array<uint8_t, 4>, 4> InterpolateColors(std::array<uint8_t, 4> c0, std::array<uint8_t, 4> c1, uint16_t c0565, uint16_t c1565){
            std::array<std::array<uint8_t, 4>, 4> colors;
            colors[0] = c0;
            colors[1] = c1;
            if(c0565 > c1565){
                colors[2] = {
                    static_cast<uint8_t>(floor((2*c0[0] + 1*c1[0]) / 3.0f)),
                    static_cast<uint8_t>(floor((2*c0[1] + 1*c1[1]) / 3.0f)),
                    static_cast<uint8_t>(floor((2*c0[2] + 1*c1[2]) / 3.0f)),
                    0xFF
                };
                colors[3] = {
                    static_cast<uint8_t>(floor((1*c0[0] + 2*c1[0]) / 3.0f)),
                    static_cast<uint8_t>(floor((1*c0[1] + 2*c1[1]) / 3.0f)),
                    static_cast<uint8_t>(floor((1*c0[2] + 2*c1[2]) / 3.0f)),
                    0xFF
                };
            } else {
                colors[2] = {static_cast<uint8_t>(floor(c0[0]/2.0f)+floor(c1[0]/2.0f)), static_cast<uint8_t>(floor(c0[1]/2.0f)+floor(c1[1]/2.0f)), static_cast<uint8_t>(floor(c0[2]/2.0f)+floor(c1[2]/2.0f)), 0xFF};
                colors[3] = {0,0,0,0};
            }
            return colors;
        }

        uint32_t GetNearestColorIndex(std::array<uint8_t, 4> color, std::array<std::array<uint8_t, 4>, 4> colorsInterp){
            if(color[3] < 16){
                for(int i = 0; i < 4; i++){
                    if(colorsInterp[i][3] == 0) return i;
                }
            }

            int minDist = INT_MAX;
            int idx = 0;

            for(int i = 0; i < 4; i++){
                int curDist = ColorDist(color, colorsInterp[i]);
                if(curDist < minDist){
                    if(curDist == 0) return i;
                    minDist = curDist;
                    idx = i;
                }
            }

            return idx;
        }
    }

    void CMPR(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData){
        if (width == 0 || height == 0) return;

        // Iterate the blocks in the image
        for (int tileY = 0; tileY < height; tileY += 8) {
            for (int tileX = 0; tileX < width; tileX += 8) {
                for(int sb = 0; sb < 4; sb++){
                    int sbx = tileX + (sb % 2) * 4;
                    int sby = tileY + floor(sb / 2.0f) * 4;

                    std::array<std::array<uint8_t, 4>, 16> colors;
                    bool transparent = false;
                    for(int i = 0; i < 16; i++){
                        int x = (i % 4) + sbx;
                        int y = floor(i / 4.0f) + sby;
                        if(x >= width || y >= height) continue;

                        uint32_t pixel = ((y * width) + x)*4;

                        std::array<uint8_t, 4> color = {imageData[pixel+0], imageData[pixel+1], imageData[pixel+2], imageData[pixel+3]};
                        if(color[3] < 16){
                            transparent = true;
                        } else {
                            colors[i] = color;
                        }
                    }

                    auto keyColors = DXT::GetKeyColors(colors);
                    auto c0 = keyColors.first;
                    auto c1 = keyColors.second;

                    uint16_t c0565 = ColorFormat::RGBA8toRGB565(c0[0], c0[1], c0[2], c0[3]);
                    uint16_t c1565 = ColorFormat::RGBA8toRGB565(c1[0], c1[1], c1[2], c1[3]);

                    if(transparent && c0565 > c1565){
                        std::swap(c0565, c1565);
                        std::swap(c0, c1);
                    } else if(!transparent && c0565 < c1565){
                        std::swap(c0565, c1565);
                        std::swap(c0, c1);
                    }

                    // interp
                    auto colorsInterp = DXT::InterpolateColors(c0, c1, c0565, c1565);

                    stream->writeUInt16(c0565);
                    stream->writeUInt16(c1565);

                    uint32_t indices = 0;
                    for(int i = 0; i < 16; i++){
                        int x = (i % 4) + sbx;
                        int y = floor(i / 4.0f) + sby;
                        if(x >= width || y >= height) continue;

                        uint32_t pixel = ((y * width) + x)*4;
                        std::array<uint8_t, 4> color = {imageData[pixel+0], imageData[pixel+1], imageData[pixel+2], imageData[pixel+3]};

                        auto c = std::find(colorsInterp.begin(), colorsInterp.end(), color);
                        uint32_t colorIndex = 0;
                        if(c != colorsInterp.end()){
                            colorIndex = c - colorsInterp.begin();
                        } else {
                            colorIndex = DXT::GetNearestColorIndex(color, colorsInterp);
                        }
                        indices |= (colorIndex << ((15-i)*2));
                    }
                    stream->writeUInt32(indices);
                }
            }
        }
    }

    void RGB565(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData){
        if (width == 0 || height == 0)
            return;

        uint32_t numBlocksW = width / 4;
        uint32_t numBlocksH = height / 4;

        // Iterate the blocks in the image
        for (int blockY = 0; blockY < numBlocksH; blockY++) {
            for (int blockX = 0; blockX < numBlocksW; blockX++) {
                // Iterate the pixels in the current block
                for (int pixelY = 0; pixelY < 4; pixelY++) {
                    for (int pixelX = 0; pixelX < 4; pixelX++) {
                        // Bounds check to ensure the pixel is within the image.
                        if ((blockX * 4 + pixelX >= width) || (blockY * 4 + pixelY >= height)){
                            continue;
                        }

                        // RGB values for this pixel are stored in a 16-bit integer.
                        uint32_t destIndex = (width * ((blockY * 4) + pixelY) + (blockX * 4) + pixelX) * 4;
                        uint8_t r = imageData[destIndex];
                        uint8_t g = imageData[destIndex + 1];
                        uint8_t b = imageData[destIndex + 2];
                        uint8_t a = imageData[destIndex + 3];

                        uint16_t rgb565 = ColorFormat::RGBA8toRGB565(r,g,b,a);
                        stream->writeUInt16(rgb565);

                    }
                }
            }
        }
    }

    void RGB5A3(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData){
        if (width == 0 || height == 0)
            return;

        uint32_t numBlocksW = width / 4;
        uint32_t numBlocksH = height / 4;

        // Iterate the blocks in the image
        for (int blockY = 0; blockY < height; blockY+=4) {
            for (int blockX = 0; blockX < width; blockX+=4) {
                // Iterate the pixels in the current block
                for (int pixelY = 0; pixelY < 4; pixelY++) {
                    for (int pixelX = 0; pixelX < 4; pixelX++) {
                        // Bounds check to ensure the pixel is within the image.
                        if ((blockX + pixelX >= width) || (blockY + pixelY >= height)){
                            //stream->writeUInt16(0xFFFF);
                            continue;
                        }

                        // RGB values for this pixel are stored in a 16-bit integer.
                        uint32_t destIndex = ((width * (blockY + pixelY)) + (blockX + pixelX)) * 4;
                        uint8_t r = imageData[destIndex];
                        uint8_t g = imageData[destIndex + 1];
                        uint8_t b = imageData[destIndex + 2];
                        uint8_t a = imageData[destIndex + 3];

                        uint16_t rgb5a3 = ColorFormat::RGBA8toRGB5A3(r,g,b,a);
                        stream->writeUInt16(rgb5a3);

                    }
                }
            }
        }
    }

    void I4(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData){
        if(width == 0 || height == 0) return;

        uint32_t numBlocksW = width / 8;
        uint32_t numBlocksH = height / 8;

        // Iterate the blocks in the image
        for (int blockY = 0; blockY < numBlocksH; blockY++) {
            for (int blockX = 0; blockX < numBlocksW; blockX++) {
                // Iterate the pixels in the current block
                for (int pixelY = 0; pixelY < 8; pixelY++) {
                    for (int pixelX = 0; pixelX < 8; pixelX += 2) {
                        // Bounds check to ensure the pixel is within the image.
                        if ((blockX * 8 + pixelX >= width) || (blockY * 8 + pixelY >= height)){
                            //stream->writeUInt8(0xFF);
                            continue;
                        }

                        uint32_t srcIndex1 = (width * ((blockY * 8) + pixelY) + (blockX * 8) + pixelX) * 4;
                        uint32_t srcIndex2 = (width * ((blockY * 8) + pixelY) + (blockX * 8) + (pixelX + 1)) * 4;
                        uint8_t color1 = ColorFormat::RGBA8toI4(imageData[srcIndex1], imageData[srcIndex1+1], imageData[srcIndex1+2], imageData[srcIndex1+3]);
                        uint8_t color2 = ColorFormat::RGBA8toI4(imageData[srcIndex2], imageData[srcIndex2+1], imageData[srcIndex2+2], imageData[srcIndex2+3]);

                        stream->writeUInt8((color1 & 0xF) << 4 | (color2 & 0xF));
                    }
                }
            }
        }
    }

    void IA4(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData){
        if(width == 0 || height == 0) return;

        uint32_t numBlocksW = width / 8;
        uint32_t numBlocksH = height / 4;

        // Iterate the blocks in the image
        for (int blockY = 0; blockY < numBlocksH; blockY++) {
            for (int blockX = 0; blockX < numBlocksW; blockX++) {
                // Iterate the pixels in the current block
                for (int pixelY = 0; pixelY < 4; pixelY++) {
                    for (int pixelX = 0; pixelX < 8; pixelX++) {
                        // Bounds check to ensure the pixel is within the image.
                        if ((blockX * 8 + pixelX >= width) || (blockY * 4 + pixelY >= height)){
                            //stream->writeUInt8(0xFF);
                            continue;
                        }

                        uint32_t srcIndex = (width * ((blockY * 4) + pixelY) + (blockX * 8) + pixelX) * 4;

                        stream->writeUInt8(ColorFormat::RGBA8toIA4(imageData[srcIndex], imageData[srcIndex+1], imageData[srcIndex+2], imageData[srcIndex+3]));
                    }
                }
            }
        }
    }

    void I8(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData){
        if(width == 0 || height == 0) return;

        uint32_t numBlocksW = width / 8;
        uint32_t numBlocksH = height / 4;

        // Iterate the blocks in the image
        for (int blockY = 0; blockY < numBlocksH; blockY++) {
            for (int blockX = 0; blockX < numBlocksW; blockX++) {
                // Iterate the pixels in the current block
                for (int pixelY = 0; pixelY < 4; pixelY++) {
                    for (int pixelX = 0; pixelX < 8; pixelX++) {
                        // Bounds check to ensure the pixel is within the image.
                        if ((blockX * 8 + pixelX >= width) || (blockY * 4 + pixelY >= height)){
                        //    stream->writeUInt8(0xFF);
                            continue;
                        }

                        uint32_t srcIndex = (width * ((blockY * 4) + pixelY) + (blockX * 8) + pixelX) * 4;

                        stream->writeUInt8(ColorFormat::RGBA8toI8(imageData[srcIndex], imageData[srcIndex+1], imageData[srcIndex+2], imageData[srcIndex+3]));
                    }
                }
            }
        }
    }

    void IA8(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData){
        if(width == 0 || height == 0) return;

        uint32_t numBlocksW = width / 8;
        uint32_t numBlocksH = height / 4;

        // Iterate the blocks in the image
        for (int blockY = 0; blockY < numBlocksH; blockY++) {
            for (int blockX = 0; blockX < numBlocksW; blockX++) {
                // Iterate the pixels in the current block
                for (int pixelY = 0; pixelY < 4; pixelY++) {
                    for (int pixelX = 0; pixelX < 8; pixelX++) {
                        // Bounds check to ensure the pixel is within the image.
                        if ((blockX * 8 + pixelX >= width) || (blockY * 4 + pixelY >= height)){
                        //    stream->writeUInt16(0xFFFF);
                            continue;
                        }

                        uint32_t srcIndex = (width * ((blockY * 4) + pixelY) + (blockX * 8) + pixelX) * 4;

                        stream->writeUInt16(ColorFormat::RGBA8toIA8(imageData[srcIndex], imageData[srcIndex+1], imageData[srcIndex+2], imageData[srcIndex]+3));
                    }
                }
            }
        }
    }
}
};

void Bti::SetData(uint16_t width, uint16_t height, uint8_t* imageData){
    if(mWidth != width || mHeight != height){
        delete[] mImageData;
        mImageData = new uint8_t[width*height*4]{0};
    }

    mWidth = width;
    mHeight = height;

    std::memcpy(mImageData, imageData, (mWidth*mHeight*4));
}

void Bti::Save(bStream::CStream* stream){
    stream->writeUInt8(mFormat);
    stream->writeUInt8(mEnableAlpha);

    stream->writeUInt16(mWidth);
    stream->writeUInt16(mHeight);

    stream->writeUInt8(mWrapS);
    stream->writeUInt8(mWrapT);
    stream->writeUInt16(mPaletteFormat);
    stream->writeUInt16(mNumPaletteEntries);
    stream->writeUInt32(mPaletteOffsetData);
    stream->writeUInt8(mMipMapEnabled);
    stream->writeUInt8(mEdgeLODEnabled);
    stream->writeUInt8(mClampLODBias);
    stream->writeUInt8(mMaxAnisotropy);
    stream->writeUInt8(mMinFilterType);
    stream->writeUInt8(mMagFilterType);
    stream->writeUInt8(mMinLOD);
    stream->writeUInt8(mMaxLOD);
    stream->writeUInt8(mNumImages);
    stream->writeUInt8(0);
    stream->writeUInt16(mLODBias);

    std::size_t dataOffset = Util::PadTo32(stream->tell()+sizeof(uint32_t));
    stream->writeUInt32(dataOffset);

    while(stream->tell() != dataOffset) stream->writeUInt8(0);

    switch (mFormat){
    case 0x00:
        ImageFormat::Encode::I4(stream, mWidth, mHeight, mImageData);
        break;
    case 0x01:
        ImageFormat::Encode::I8(stream, mWidth, mHeight, mImageData);
        break;
    case 0x02:
        ImageFormat::Encode::IA4(stream, mWidth, mHeight, mImageData);
        break;
    case 0x03:
        ImageFormat::Encode::IA8(stream, mWidth, mHeight, mImageData);
        break;
    case 0x04:
        ImageFormat::Encode::RGB565(stream, mWidth, mHeight, mImageData);
        break;
    case 0x05:
        ImageFormat::Encode::RGB5A3(stream, mWidth, mHeight, mImageData);
        break;
    case 0x0E:
        //ImageFormat::Encode::CMPR(stream, mWidth, mHeight, imageData);
        break;
    default:
        break;
    }
}

bool Bti::Load(bStream::CStream* stream){
    mFormat = stream->readUInt8();
    mEnableAlpha = stream->readUInt8();

    mWidth = stream->readUInt16();
    mHeight = stream->readUInt16();

    mWrapS = stream->readUInt8();
    mWrapT = stream->readUInt8();
    mPaletteFormat = stream->readUInt16();
    mNumPaletteEntries = stream->readUInt16();
    mPaletteOffsetData = stream->readUInt32();
    mMipMapEnabled = stream->readUInt8();
    mEdgeLODEnabled = stream->readUInt8();
    mClampLODBias = stream->readUInt8();
    mMaxAnisotropy = stream->readUInt8();
    mMinFilterType = stream->readUInt8();
    mMagFilterType = stream->readUInt8();
    mMinLOD = stream->readUInt8();
    mMaxLOD = stream->readUInt8();
    mNumImages = stream->readUInt8();

    stream->skip(1);

    mLODBias = stream->readUInt16();
    uint32_t imageDataOffset = stream->readUInt32();

    if(mWidth == 0 || mHeight == 0){
        return false;
    }

    stream->seek(imageDataOffset);

    if(mImageData != nullptr){
        delete[] mImageData;
    }

    mImageData = new uint8_t[mWidth * mHeight * 4]{0};

    switch (mFormat){
    case 0x00:
        ImageFormat::Decode::I4(stream, mWidth, mHeight, mImageData);
        break;
    case 0x01:
        ImageFormat::Decode::I8(stream, mWidth, mHeight, mImageData);
        break;
    case 0x02:
        ImageFormat::Decode::IA4(stream, mWidth, mHeight, mImageData);
        break;
    case 0x03:
        ImageFormat::Decode::IA8(stream, mWidth, mHeight, mImageData);
        break;
    case 0x04:
        ImageFormat::Decode::RGB565(stream, mWidth, mHeight, mImageData);
        break;
    case 0x05:
        ImageFormat::Decode::RGB5A3(stream, mWidth, mHeight, mImageData);
        break;
    case 0x0E:
        ImageFormat::Decode::CMPR(stream, mWidth, mHeight, mImageData);
        break;
    default:
        break;
    }

    return true;
}

bool TplImage::Load(bStream::CStream* stream){
    mHeight = stream->readUInt16();
    mWidth = stream->readUInt16();

    mFormat = stream->readUInt32();

    uint32_t imageDataOffset = stream->readUInt32();

    mWrapS = stream->readUInt32();
    mWrapT = stream->readUInt32();
    mMinFilterType = stream->readUInt32();
    mMagFilterType = stream->readUInt32();
    mLODBias = stream->readFloat();
    mEdgeLODEnabled = stream->readUInt8();
    mMinLOD = stream->readUInt8();
    mMaxLOD = stream->readUInt8();

    stream->skip(1);

    if(mWidth == 0 || mHeight == 0){
        return false;
    }

    std::cout << "reading" << std::endl;
    stream->seek(imageDataOffset);

    if(mImageData != nullptr){
        delete[] mImageData;
    }

    mImageData = new uint8_t[mWidth * mHeight * 4]{0};

    switch (mFormat){
    case 0x00:
        ImageFormat::Decode::I4(stream, mWidth, mHeight, mImageData);
        break;
    case 0x01:
        ImageFormat::Decode::I8(stream, mWidth, mHeight, mImageData);
        break;
    case 0x02:
        ImageFormat::Decode::IA4(stream, mWidth, mHeight, mImageData);
        break;
    case 0x03:
        ImageFormat::Decode::IA8(stream, mWidth, mHeight, mImageData);
        break;
    case 0x04:
        ImageFormat::Decode::RGB565(stream, mWidth, mHeight, mImageData);
        break;
    case 0x05:
        ImageFormat::Decode::RGB5A3(stream, mWidth, mHeight, mImageData);
        break;
    case 0x0E:
        ImageFormat::Decode::CMPR(stream, mWidth, mHeight, mImageData);
        break;
    default:
        break;
    }

    return true;

}

void TplImage::Save(bStream::CStream* stream){

}

void TplImage::SetData(uint16_t width, uint16_t height, uint8_t* imageData){
    if(mWidth != width || mHeight != height){
        delete[] mImageData;
        mImageData = new uint8_t[width*height*4]{0};
    }

    mWidth = width;
    mHeight = height;

    std::memcpy(mImageData, imageData, (mWidth*mHeight*4));
}

void Tpl::SetData(std::size_t idx, uint16_t width, uint16_t height, uint8_t* imageData){
    mImages[idx]->SetData(width, height, imageData);
}

void Tpl::Save(bStream::CStream* stream){
}

bool Tpl::Load(bStream::CStream* stream){
    uint32_t magic = stream->readUInt32();

    if(magic != 0x0020AF30){
        return false;
    }

    mNumImages = stream->readUInt32();
    mImages.resize(mNumImages);

    uint32_t imgTableOffset = stream->readUInt32();

    stream->seek(imgTableOffset);

    std::vector<std::pair<uint32_t, uint32_t>> imgHeaders(mNumImages);

    for (size_t i = 0; i < mNumImages; i++){
        imgHeaders[i].first = stream->readUInt32();
        imgHeaders[i].second = stream->readUInt32();
    }

    uint32_t imgIndex = 0;
    for(auto [imgOffset, palOffset] : imgHeaders){
        stream->seek(imgOffset);

        mImages[imgIndex] = std::make_shared<TplImage>();
        mImages[imgIndex]->Load(stream);
        imgIndex++;
    }

    return true;
}
