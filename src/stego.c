#include "stego.h"
#include <stdint.h>
#include <limits.h>
#include <openssl/rand.h>

static int scalar_product(uint64_t a, uint64_t b) {
    int out = 0;
    for (int i = 0; i < 63; i++){
        if (((a >> i) & 1U)*((b >> i) & 1U) == 1){
            out = !out;
        }
    }
    return out;
}



static uint64_t* generate_random_key(void){
    uint64_t* out = (uint64_t*) malloc(sizeof(uint64_t)*2);
    uint64_t buf[2];
    RAND_bytes((unsigned char*) buf, sizeof(buf));
    uint64_t rnd_int = out[1];
    rnd_int -= rnd_int%16;
    out[1] = rnd_int;
    return out;
}

int stego_gen_keys(void* buf, int count) {
    int total_keys_size = count * (STEGO_KEY_BITS/CHAR_BIT);
    uint64_t * keys = (uint64_t *) malloc(total_keys_size);
    for (int i = 0; i < count*2; i+=2){
        uint64_t* rnd_key = generate_random_key();
        keys[i] = rnd_key[0];
        keys[i+1] = rnd_key[1];
        for (int j = 0; j < i; j+=2){
            uint64_t part_1 = scalar_product(keys[i], keys[j]);
            uint64_t part_2 = scalar_product(keys[i+1], keys[j+1]);
            if (part_1 + part_2 == 1){
                keys[i]+=keys[j];
                keys[i+1]+=keys[j+1];
            }           
        }
    }
    buf = keys;
    return 0;
}