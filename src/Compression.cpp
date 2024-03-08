#include "Compression.hpp"

namespace Compression {

namespace Yaz0 {

void Decompress(bStream::CStream* src_data, bStream::CStream* dst_data, uint32_t offset, uint32_t length){
    uint32_t count = 0, src_pos = 0, dst_pos = 0;
    uint8_t bits;

    size_t decompressedSize = length;
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

            for (size_t i = 0; i < len; ++i)
            {
                dst[dst_pos] = dst[copy_src];
                copy_src++;
                dst_pos++;
            }
            
        }
        
        bits <<= 1;
        --count;
    }

    std::cout << std::hex << *((uint32_t*)dst) << std::endl;

    dst_data->writeBytes(dst, decompressedSize);

    delete[] src;
    delete[] dst;
}

void Compress(bStream::CStream* src_data, bStream::CStream* dst_data, uint8_t level){

}

}


namespace Yay0 {

void Decompress(bStream::CStream* src_data, bStream::CStream* dst_data, uint32_t offset, uint32_t length){
    uint32_t expand_limit, bit_offset, expand_offset, ref_offset, read_offset, bit_count, bits;

    size_t decompressedSize = length;
    uint8_t* src = new uint8_t[src_data->getSize()];
    
    if(length == 0){
        src_data->seek(4);
        decompressedSize = src_data->readUInt32(); 
    }
    
    uint8_t* dst = new uint8_t[decompressedSize];

    src_data->readBytesTo(src, src_data->getSize());


	expand_limit = ((src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7]);
	ref_offset = ((src[8] << 24) | (src[9] << 16) | (src[10] << 8) | src[11]);
	read_offset = ((src[12] << 24) | (src[13] << 16) | (src[14] << 8) | src[15]);
    
    expand_offset = 0;
    bit_count = 0;
    bit_offset = 16;

    if(length == 0 || offset > expand_limit){
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

void Compress(bStream::CStream*  src_data, bStream::CStream* dst_data){

}

}

}