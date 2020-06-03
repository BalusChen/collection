#ifndef HASH_H__
#define HASH_H__


#include <string>
#include <string.h>


static uint32_t DecodeFixed32(const char *ptr) {
    uint32_t  result;

    memcpy(&result, ptr, sizeof(result));
    return result;
}


static uint32_t Hash(const std::string &key, uint32_t seed) {
    const uint32_t  m = 0xc6a4a893;
    const uint32_t  r = 24;
    const char  *data = key.data();
    const uint32_t  n = key.size();
    const char  *limit = data + n;
    uint32_t  h = seed ^ (n * m);

    /*
     * 每次取 4 个字节
     * 如果有多余，会在后面将其用上
     */
    while (data + 4 <= limit) {
        uint32_t w = DecodeFixed32(data);
        data += 4;
        h += w;
        h *= m;
        h ^= (h >> 16);
    }

    switch (limit - data) {
    case 3:
        h += static_cast<unsigned char>(data[2]) << 16;
        /* fall through */

    case 2:
        h += static_cast<unsigned char>(data[1]) << 8;
        /* fall through */

    case 1:
        h += static_cast<unsigned char>(data[0]);
        h *= m;
        h ^= (h >> r);
        break;
    }

    return h;
}


#endif /* HASH_H__ */
