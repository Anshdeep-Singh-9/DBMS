#ifndef SHA256_H
#define SHA256_H

#include <string>
#include <vector>
#include <stdint.h>

class SHA256 {
public:
    static std::string hash(const std::string& input);

private:
    static const uint32_t k[64];

    struct Context {
        uint64_t datalen;
        uint64_t bitlen;
        uint8_t data[64];
        uint32_t state[8];
    };

    static void transform(Context* ctx, const uint8_t* data);
    static void init(Context* ctx);
    static void update(Context* ctx, const uint8_t* data, size_t len);
    static void final(Context* ctx, uint8_t* hash);
};

#endif
