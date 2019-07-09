#include <eosiolib/print.hpp>
using eosio::print;

static unsigned __int128 decode_number128(unsigned char* input, unsigned int len) {
    unsigned int i;
    unsigned __int128 result;
    result = 0;
    for(i = 0 ; i < len ; i++) {
        result = result << 8;
        result += (unsigned char)input[i];
    }

    return result;
}

/*****************************************************************************/

static int check_pow(unsigned char* difficulty,
             unsigned int difficulty_len,
             unsigned char* ethash) {

    unsigned __int128 difficulty_value = decode_number128(difficulty, difficulty_len);
    unsigned __int128 ethash_value[4];
    unsigned __int128 result[4];
    unsigned __int128 max_uint64 = 0xFFFFFFFFFFFFFFFFL;
    unsigned __int128 temp;
    unsigned __int128 temp2;

    unsigned int i;

    if(difficulty_len > 8) printf("error: unexpected difficulty len\n");
    memset(result, 0x00, sizeof(result));

    for(i = 0 ; i < 4; i++) ethash_value[i] = decode_number128(ethash + 8*i, 8);

    temp = difficulty_value * ethash_value[3];
    result[3] = temp & max_uint64;
    result[2] = temp >> 64;

    temp = difficulty_value * ethash_value[2];
    temp2 = (temp & max_uint64) + result[2];
    result[2] = temp2 & max_uint64;
    result[1] = (temp2 >> 64) + (temp >> 64);

    temp = difficulty_value * ethash_value[1];
    temp2 = (temp & max_uint64) + result[1];
    result[1] = temp2 & max_uint64;
    result[0] = (temp2 >> 64) + (temp >> 64);;

    temp = difficulty_value * ethash_value[0];
    temp2 = (temp & max_uint64) + result[0];
    result[0] = temp2 & max_uint64;

    //printf("0x%016lx%016lx%016lx%016lx\n", (long unsigned int)result[0] , (long unsigned int)result[1], (long unsigned int)result[2], (long unsigned int)result[3]);

    return ((temp2 >> 64) + (temp >> 64)) == 0;
}

