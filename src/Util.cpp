#include "Util.hpp"

namespace Util {
    uint32_t PadTo32(uint32_t x){
        return ((x + (32-1)) & ~(32-1));
    }
}