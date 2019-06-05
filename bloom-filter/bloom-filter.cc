#include "../include/hash.h"


static uint32_t BloomHash(const std::string &key) {
    return Hash(key, 0xbc9f1d34);
}


class BloomFilter {
    private:
        size_t bits_per_key_;
        size_t k_;

    public:
        explicit BloomFilter(int bits_per_key)
            : bits_per_key_(bits_per_key) {
            // k_ 表示要进行 hash 的次数
            // 研究表明 k = bits_per_key * ln(2) 时误判率低
            k_ = static_cast<size_t>(bits_per_key * 0.69);

            if (k_ < 1) {
                k_ = 1;
            }
            if (k_ > 30) {
                k_ = 30;
            }
        }


        void CreteFileter(const std::string *keys,
            int n, std::string *dst) const
        {
            size_t  bits = n * bits_per_key_;

            // 如果位图很小，那么误判率就会很高，所以适当扩大
            if (bits < 64) {
                bits = 64;
            }

            // 位图所占字节数，以及将 bit 数向上取整为 8 的倍数
            size_t bytes = (bits + 7) / 8;
            bits = bytes * 8;

            // 为什么要这两句
            const size_t init_size = dst->size();
            dst->resize(init_size + bytes, 0);
            // 这里把 hash 所用的次数也存进去
            dst->push_back(static_cast<char>(k_));

            char *array = &(*dst)[init_size];
            for (int i = 0; i < n; i++) {
                uint32_t h = BloomHash(keys[i]);
                // 向右旋转 17bit
                const uint32_t delta = (h >> 17) | (h << 15);
                // 本来应该进行 k_ 次 hash
                // 但是这里为了速度就使用上一轮的 hash 值来得到新的 hash 值
                for (size_t j = 0; j < k_; j++) {
                    const uint32_t bitpos = h % bits;
                    array[bitpos/8] |= (1 << (bitpos % 8));
                    h += delta;
                }
            }
        }

        bool KeyMayMatch(const std::string &key,
            const std::string &bloom_filter) const
        {
            const size_t len = bloom_filter.size();
            if (len < 2) {
                return false;
            }

            const char *array = bloom_filter.data();
            const size_t bits = (len - 1) * 8;
            const size_t k = array[len-1];

            if (k > 30) {
                return true;
            }

            uint32_t h = BloomHash(key);
            const uint32_t delta = (h >> 17) | (h << 15);
            for (size_t j = 0; j < k; j++) {
                // 依次计算每一轮的 hash
                // 只有有一个位置不为 1，就说明 key 不在集合中
                const uint32_t bitpos = h % bits;
                if ((array[bitpos/8] & (1 << (bitpos%8))) == 0) {
                    return false;
                }
                h+= delta;
            }

            return true;
        }
};


int
main(int argc, char **argv)
{
    bool         exist;
    std::string  bitmap;
    BloomFilter  bloom_filter(3);

    std::string keys[] = { "abc", "leveldb", "moon" };

    bloom_filter.CreteFileter(keys, 3, &bitmap);
    exist = bloom_filter.KeyMayMatch("funny", bitmap);
    printf("exist(%s): %d\n", "funny", exist);


    printf("bitmap: %s(%ld)\n", bitmap.c_str(), bitmap.size());

    for (int i = 0; i < 3; i++) {
        exist = bloom_filter.KeyMayMatch(keys[i], bitmap);
        printf("exist(%s): %d\n", keys[i].c_str(), exist);
    }

    return 0;
}
