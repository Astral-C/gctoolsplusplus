#pragma once
#include <Util.hpp>
#include <bstream.h>
#include <cstdint>

namespace Compression {

    enum class Format {
        None,
        YAZ0,
        YAY0
    };

    std::size_t GetDecompressedSize(bStream::CStream* stream);

    namespace Yaz0 {
        // if no length provided, it will be read as if this is a full compressed file
        void Decompress(bStream::CStream* src_data, bStream::CStream* dst_data, uint32_t offset=0, uint32_t length=0);
        void Compress(bStream::CStream* src_data, bStream::CStream* dst_data, uint8_t level);
    }

    namespace Yay0 {
        void Decompress(bStream::CStream* src_data, bStream::CStream* dst_data, uint32_t offset=0, uint32_t length=0);
        void Compress(bStream::CStream*  src_data, bStream::CStream* dst_data);
    }

}