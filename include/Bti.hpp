#pragma once
#include "bstream.h"
#include <vector>
#include <memory>

namespace ImageFormat {
    namespace Decode {
        void CMPR(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData);
        
        void RGB5A3(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData);
        void RGB565(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData);
        
        void I4(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData);
        void I8(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData);

        void IA4(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData);
        void IA8(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData);
    }

    namespace Encode {
        void CMPR(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData);
        void RGB5A3(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData);
        void RGB565(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData);


        void I4(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData);
        void I8(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData);

        void IA4(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData);
        void IA8(bStream::CStream* stream, uint16_t width, uint16_t height, uint8_t* imageData);
    }
};

class Bti {
    uint8_t mFormat { 0 };
    uint8_t mEnableAlpha { 0 };
    uint8_t mWrapS { 0 };
    uint8_t mWrapT { 0 };
    uint16_t mPaletteFormat { 0 };
    uint16_t mNumPaletteEntries { 0 };
    uint32_t mPaletteOffsetData { 0 };
    uint8_t mMipMapEnabled { 0 };
    uint8_t mEdgeLODEnabled { 0 };
    uint8_t mClampLODBias { 0 };
    uint8_t mMaxAnisotropy { 0 };
    uint8_t mMinFilterType { 0 };
    uint8_t mMagFilterType { 0 };
    uint8_t mMinLOD { 0 };
    uint8_t mMaxLOD { 0 };
    uint8_t mNumImages { 0 };
    uint16_t mLODBias { 0 };

    uint8_t* mImageData { nullptr };

public:
    uint16_t mWidth { 0 };
    uint16_t mHeight { 0 };

    uint8_t GetFormat() { return mFormat; }
    uint8_t* GetData() { return mImageData; }

    bool Load(bStream::CStream* stream);
    void Save(bStream::CStream* stream);
    void SetData(uint16_t width, uint16_t height, uint8_t* imageData);
    void SetFormat(uint8_t fmt) { mFormat = fmt; }

    Bti(){}
    ~Bti(){ if(mImageData != nullptr){ delete[] mImageData; } }

};

class TplImage {
    uint32_t mFormat { 0 };
    uint32_t mWrapS { 0 };
    uint32_t mWrapT { 0 };
    uint32_t mMinFilterType { 0 };
    uint32_t mMagFilterType { 0 };
    uint32_t mMinLOD { 0 };
    uint32_t mMaxLOD { 0 };
    uint8_t mEdgeLODEnabled { 0 };
    float mLODBias { 0 };
    uint8_t* mImageData { nullptr };

public:
    uint16_t mWidth { 0 };
    uint16_t mHeight { 0 };

    uint8_t GetFormat() { return mFormat; }
    void SetFormat(uint8_t fmt) { mFormat = fmt; }
    void SetData(uint16_t width, uint16_t height, uint8_t* imageData);
    uint8_t* GetData() { return mImageData; }

    bool Load(bStream::CStream* stream);
    void Save(bStream::CStream* stream);

    TplImage(){}
    ~TplImage() { if(mImageData != nullptr){ delete[] mImageData; } }
};

class Tpl {
    uint32_t mNumImages { 0 };
    std::vector<std::shared_ptr<TplImage>> mImages {};

public:

    std::shared_ptr<TplImage> NewImage(){ mImages.push_back(std::make_shared<TplImage>()); return mImages.back(); }
    std::shared_ptr<TplImage> GetImage(std::size_t idx) { return mImages[idx]; }

    void SetData(std::size_t idx, uint16_t width, uint16_t height, uint8_t* imageData);

    bool Load(bStream::CStream* stream);
    void Save(bStream::CStream* stream);

    Tpl(){}
    ~Tpl(){}

};