#include <bstream.h>
#include "Util.hpp"

namespace Util {
    uint32_t AlignTo(uint32_t x, uint32_t y){
        return ((x + (y-1)) & ~(y-1));
    }

    uint32_t PadTo32(uint32_t x){
        return ((x + (32-1)) & ~(32-1));
    }
}