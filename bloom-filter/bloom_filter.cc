
/*
 * Copyright (C) Jianyong Chen
 */

#include <vector>
#include <hash.hh>


static uint32_t BloomHash(const std::string &key) {
    return Hash(key, 0xbc9f1d34);
}


class BloomFilter {
    private:
        size_t bits_per_key_;
        size_t k_;
        std::string bitmap_;
        std::vector<std::string> *keys_;


        void Resize() {
            // 位图扩大一倍
            const size_t  new_size = bitmap_.size() << 1;
            std::string  new_bitmap = std::string(new_size, 0);
            bitmap_ = new_bitmap;

            for (const auto &key : *keys_) {
                _AddKey(key);
            }
        }


        void _AddKey(const std::string &key) {
            const size_t  bits = bitmap_.size() * 8;
            char  *array = &bitmap_[0];
            uint32_t  h = BloomHash(key);
            const uint32_t  delta = (h >> 17) | (h << 15);

            for (int i = 0; i < k_; i++) {
                const uint32_t  bitpos = h % bits;
                array[bitpos/8] |= (1 << (bitpos % 8));
                h += delta;
            }
        }


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

            // 初始化 key 集合大小为 64
            keys_ = new std::vector<std::string>;
            keys_->reserve(64);
        }


        void AddKey(const std::string &key) {
            if (bitmap_.empty()) {
                // 将位图的初始大小设置为 8 * 8 bits
                bitmap_.resize(8, 0);
            }

            /*
             * 随着 key 的添加，误判率会变高，所以需要及时扩大位图
             */
            if (bitmap_.size() * 8 <= keys_->size() * bits_per_key_) {
                Resize();
            }

            _AddKey(key);

            keys_->push_back(key);
        }



        bool KeyMayMatch(const std::string &key) const {
            if (bitmap_.empty()) {
                return false;
            }

            const size_t len = bitmap_.size();
            const char *array = bitmap_.data();
            const size_t bits = len * 8;

            if (k_ > 30) {
                return true;
            }

            uint32_t h = BloomHash(key);
            const uint32_t delta = (h >> 17) | (h << 15);
            for (size_t j = 0; j < k_; j++) {
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

    std::vector<std::string> keys =
        { "abc", "leveldb", "moon", "funny", "lucy", "tommy",
          "rimbaud", "tom", "pikazza", "chythia", "kolo", "meetyou" };
    std::vector<std::string> keys2 =
        { "hello", "world", "who", "programming language pragmatics",
          "jerry", "sam", "nice", "whoami", "locate", "network", "iamyou" };


    for (int i = 0; i < keys.size(); i++) {
        bloom_filter.AddKey(keys[i]);
    }


    for (int i = 0; i < keys.size(); i++) {
        exist = bloom_filter.KeyMayMatch(keys[i]);
        printf("exist(%s): %d\n", keys[i].c_str(), exist);
    }

    printf("\n");

    for (int i = 0; i < keys2.size(); i++) {
        exist = bloom_filter.KeyMayMatch(keys2[i]);
        printf("exist(%s): %d\n", keys2[i].c_str(), exist);
    }

    return 0;
}
