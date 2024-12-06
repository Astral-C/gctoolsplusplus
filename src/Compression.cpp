#include "Compression.hpp"

namespace Compression {

std::size_t GetDecompressedSize(bStream::CStream* stream){
    std::size_t pos = stream->tell();

    stream->seek(4);
    std::size_t decompressedSize = stream->readUInt32();
    stream->seek(pos);

    return decompressedSize;
}

namespace Yaz0 {

struct _MatchResult {
    std::size_t position;
    std::size_t length;
};


void Decompress(bStream::CStream* src_data, bStream::CStream* dst_data, uint32_t offset, uint32_t length){
    uint32_t count = 0, src_pos = 0, dst_pos = 0;
    uint8_t bits;

    std::size_t decompressedSize = length;
    uint8_t* src = new uint8_t[src_data->getSize()];
    
    if(length == 0){
        src_data->seek(4);
        decompressedSize = src_data->readUInt32(); 
    }
    
    uint8_t* dst = new uint8_t[decompressedSize];

    src_data->seek(0);
    src_data->readBytesTo(src, src_data->getSize());

    src_pos = 16;
    while(dst_pos < decompressedSize) {
        if(count == 0){
            bits = src[src_pos];
            ++src_pos;
            count = 8;
        }
        
        if((bits & 0x80) != 0){
            dst[dst_pos] = src[src_pos];
            dst_pos++;
            src_pos++;

        } else {
            uint8_t b1 = src[src_pos], b2 = src[src_pos + 1];

            uint32_t len = b1 >> 4;
            uint32_t dist = ((b1 & 0xF) << 8) | b2;
            uint32_t copy_src = dst_pos - (dist + 1);

            src_pos += 2;

            if(len == 0){
                len = src[src_pos] + 0x12;
                src_pos++;
            } else {
                len += 2;
            }

            for (std::size_t i = 0; i < len; ++i)
            {
                dst[dst_pos] = dst[copy_src];
                copy_src++;
                dst_pos++;
            }
            
        }
        
        bits <<= 1;
        --count;
    }

    dst_data->writeBytes(dst, decompressedSize);

    delete[] src;
    delete[] dst;
}

_MatchResult findMatch(uint8_t* src, std::size_t readPtr, std::size_t matchMaxLength, std::size_t searchRange, std::size_t srcSize){
    _MatchResult result = {0, 1};

    if(readPtr + 2 < srcSize){
        int matchPtr = readPtr - searchRange;
        int windowEnd = readPtr + matchMaxLength;

        if(matchPtr < 0) matchPtr = 0;
        if(windowEnd > srcSize) windowEnd = srcSize;

        uint16_t v = *((uint16_t*)&src[readPtr]);
        while(matchPtr < readPtr){
            if(v != *((uint16_t*)&src[matchPtr])){
                matchPtr++;
                continue;
            }

            int matchWindowPtr = matchPtr + 1;
            int srcWindowPtr = readPtr + 1;

            while(srcWindowPtr < windowEnd && src[srcWindowPtr] == src[matchWindowPtr]){
                matchWindowPtr++;
                srcWindowPtr++;
            }

            int matchLength = srcWindowPtr - readPtr;

            if(result.length < matchLength){
                result.length = matchLength;
                result.position = matchPtr;
                if(result.length == matchMaxLength){
                    break;
                }
            }

            matchPtr++;

        }
    }

    return result;

}

void Compress(bStream::CStream* src_data, bStream::CStream* dst_data, uint8_t level){
    uint8_t* src = new uint8_t[src_data->getSize()]{};
    uint8_t* result = new uint8_t[src_data->getSize()]{};
    src_data->seek(0);
    src_data->readBytesTo(src, src_data->getSize());

    std::size_t searchRange = 0x10E0 * level / 9 - 0x0E0;

    std::size_t readPtr = 0;
    std::size_t writePtr = 0;
    std::size_t codeBytePos = 0;
    std::size_t maxLength = 0x111;

    while(readPtr < src_data->getSize()){
        codeBytePos = writePtr++;

        for(int i = 0; i < 8; i++){
            if(readPtr >= src_data->getSize()){
                break;
            }

            std::size_t matchLength = 1;
            std::size_t matchPosition = 0;

            _MatchResult match = findMatch(src, readPtr, maxLength, searchRange, src_data->getSize());;

            matchPosition = match.position;
            matchLength = match.length;

            if(matchLength > 2){
                std::size_t delta = readPtr - matchPosition - 1;

                if(matchLength < 0x12) {
                    result[writePtr++] = (delta >> 8 | (matchLength - 2) << 4);
                    result[writePtr++] = (delta & 0xFF);
                } else {
                    result[writePtr++] = (delta >> 8);
                    result[writePtr++] = (delta & 0xFF);
                    result[writePtr++] = ((matchLength - 0x12) & 0xFF);
                }
                readPtr += matchLength;
            } else {
                result[codeBytePos] |= 1 << (7 - i);
                result[writePtr++] = src[readPtr++];
            }

        }

    }

    dst_data->writeString("Yaz0");
    dst_data->writeUInt32(src_data->getSize());
    dst_data->writeUInt32(0);
    dst_data->writeUInt32(0);
    dst_data->writeBytes(result, writePtr);

    delete[] src;
    delete[] result;
}

}


namespace Yay0 {

void Decompress(bStream::CStream* src_data, bStream::CStream* dst_data, uint32_t offset, uint32_t length){
    uint32_t expand_limit, bit_offset, expand_offset, ref_offset, read_offset, bit_count, bits;

    std::size_t decompressedSize = length;
    uint8_t* src = new uint8_t[src_data->getSize()];
    
    if(length == 0){
        src_data->seek(4);
        decompressedSize = src_data->readUInt32(); 
    }
    
    uint8_t* dst = new uint8_t[decompressedSize];

    src_data->seek(0);
    src_data->readBytesTo(src, src_data->getSize());


	expand_limit = ((src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7]);
	ref_offset = ((src[8] << 24) | (src[9] << 16) | (src[10] << 8) | src[11]);
	read_offset = ((src[12] << 24) | (src[13] << 16) | (src[14] << 8) | src[15]);
    
    expand_offset = 0;
    bit_count = 0;
    bit_offset = 16;

    if(decompressedSize == 0 || offset > expand_limit){
        return;
    }

    uint8_t* read_dst = dst;
    uint8_t* read_src = src + read_offset;

    do {
        if(bit_count == 0){
            uint8_t* data = (src + bit_offset);
            bits = ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
        
            bit_count = 32;
            bit_offset += 4;
        }

        if(bits & 0x80000000) {
            if(offset == 0){

                *read_dst = *read_src;

                if(--length == 0){
                    return;
                }
            } else {
                --offset;
            }
            ++expand_offset;
            ++read_dst;
            ++read_offset;
            ++read_src;
        } else {
            uint8_t* data = (src + ref_offset);
            ref_offset += 2;

            uint16_t run = (data[0] << 8 | data[1]);
            uint16_t run_length = (run >> 12);
            uint16_t run_offset = (run & 0xFFF);
            uint32_t ref_start = (expand_offset - run_offset);
            uint32_t ref_length;

            if(run_length == 0){
                ref_length = (src[read_offset++] + 18);
                ++read_src;
            } else {
                ref_length = (run_length + 2);
            }

            if(ref_length > (expand_limit - expand_offset)){
                ref_length = expand_limit - expand_offset;
            }

            uint8_t* ref_ptr = (dst + expand_offset);
            while(ref_length > 0){
                if(offset == 0){
                    *ref_ptr = dst[ref_start - 1];
                    if(--length == 0){
                        return;
                    }
                } else {
                    --offset;
                }

                ++expand_offset;
                ++read_dst;
                ++ref_ptr;
                ++ref_start;
                --ref_length;
            }
        }

        bits <<= 1;
        --bit_count;
    } while(expand_offset < expand_limit);

    dst_data->writeBytes(dst, decompressedSize);

    delete[] src;
    delete[] dst;
}

// Adapted from Cuyler36's GCNToolkit.
void Compress(bStream::CStream*  src_data, bStream::CStream* dst_data){
    const uint32_t OFSBITS = 12;
    int32_t decPtr = 0;
    
    // Set up for mask buffer
    uint32_t maskMaxSize = (src_data->getSize() + 32) >> 3;
    uint32_t maskBitCount = 0, mask = 0;
    int32_t maskPtr = 0;
    
    // Set up for link buffer
    uint32_t linkMaxSize = src_data->getSize() >> 1;
    uint16_t linkOffset = 0;
    int32_t linkPtr = 0;
    uint16_t minCount = 3, maxCount = 273;

    // Set up chunk and window settings
    int32_t chunkPtr = 0, windowPtr = 0, windowLen = 0, length = 0, maxlen = 0;

    // Initialize all the buffers with proper size
    uint32_t* maskBuffer = new uint32_t[maskMaxSize >> 2];
    uint16_t* linkBuffer = new uint16_t[linkMaxSize];
    uint8_t* chunkBuffer = new uint8_t[src_data->getSize()];

    uint8_t* src = new uint8_t[src_data->getSize()];
    src_data->seek(0);
    src_data->readBytesTo(src, src_data->getSize());

    while(decPtr < src_data->getSize()){
        if(windowLen >= 1 << OFSBITS){
            windowLen -= (1 << OFSBITS);
            windowPtr = decPtr - windowLen;
        }

        if(src_data->getSize() - decPtr < maxCount){
            maxCount = (uint16_t)(src_data->getSize() - decPtr);
        }

        maxlen = 0;

        for (std::size_t i = 0; i < windowLen; i++)
        {
            for (length = 0; length < (windowLen - i) && length < maxCount; length++)
            {
                if(src[decPtr + length] != src[windowPtr + length + i]) break;
            }
            if(length > maxlen)
            {
                maxlen = length;
                linkOffset = (uint16_t)windowLen - i;

            }
            
        }
        
        length = maxlen;

        mask <<= 1;
        if(length >= minCount){
            uint16_t link = (uint16_t)((linkOffset - 1) & 0x0FFF);

            if(length < 18){
                link |= (uint16_t)((length - 2) << 12);
            } else {
                chunkBuffer[chunkPtr++] = (uint8_t)(length - 18);
            }

            linkBuffer[linkPtr++] = bStream::swap16(link);
            decPtr += length;
            windowLen += length;
        } else {
            chunkBuffer[chunkPtr++] = src[decPtr++];
            windowLen++;
            mask |= 1;
        }

        maskBitCount++;
        if(maskBitCount == 32){
            maskBuffer[maskPtr] = bStream::swap32(mask);
            maskPtr++;
            maskBitCount =0;
        }

    }

    if(maskBitCount > 0){
        mask <<= 32 - maskBitCount;
        maskBuffer[maskPtr] = bStream::swap32(mask);
        maskPtr++;
    }

    const char* fourcc = "Yay0";
    uint32_t linkSecOff = 0x10 + (sizeof(uint32_t) * maskPtr);
    uint32_t chunkSecOff = linkSecOff + (sizeof(uint16_t) * linkPtr);

    dst_data->writeString(fourcc);
    dst_data->writeUInt32(src_data->getSize());
    dst_data->writeUInt32(linkSecOff);
    dst_data->writeUInt32(chunkSecOff);

    dst_data->seek(0x10);
    dst_data->writeBytes((uint8_t*)maskBuffer, maskPtr*sizeof(uint32_t));
    dst_data->seek(linkSecOff);
    dst_data->writeBytes((uint8_t*)linkBuffer, linkPtr*sizeof(uint16_t));
    dst_data->seek(chunkSecOff);
    dst_data->writeBytes((uint8_t*)chunkBuffer, chunkPtr);

    delete[] maskBuffer;
    delete[] linkBuffer;
    delete[] chunkBuffer;

    delete[] src;
}

}

}