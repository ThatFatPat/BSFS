#include "stego.h"
#include <stdint.h>
#include <limits.h>
#include <openssl/rand.h>

int stego_gen_keys(void* buf, int count) {
    int total_keys_size = count * (STEGO_KEY_BITS/CHAR_BIT);
    uint_64t * keys = (uint_64t *) malloc(total_keys_size);
    for (int i = 0; i < count*2; i+=2){
        uint_64t rnd_key[] = generate_random_key();
        keys[i] = rnd_key[0];
        keys[i+1] = rnd_key[1];
        for (int j = 0; j < i; j+=2){
            part_1 = scalar_product(keys[i], keys[j]);
            part_2 = scalar_product(keys[i+1], keys[j+1]);
            if (part_1 + part_2 == 1){
                keys[i]+=keys[j];
                keys[i+1]+=keys[j+1];
            } 
        }
    }
    buf = arr;
    return 0;
}

static int scalar_product(uint_64t * a, uint_64t * b) {
    int out = 0;
    for (int i = 0; i < 63; i++){
        if (((a >> i) & 1U) == 1 && ((b >> i) & 1U) == 1){
            out = !out;
        }
    }
    return out;
}

static uint_64t * generate_random_key(void){
    uint_64t * out = malloc(sizeof(uint_64t)*2);
    uint_64t buf[2];
    RAND_bytes((unsigned char*) buf, sizeof(buf));
    uint_64t rnd_int = out[1];
    rnd_int -= rnd_int%16;
    out[1] = rnd_int
    return out;
}
