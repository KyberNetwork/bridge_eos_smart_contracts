#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/types.h>
#include <eosiolib/crypto.h>
#include <eosiolib/asset.hpp>
#include <eosiolib/symbol.hpp>

#include <string>

using std::string;
using namespace eosio;

/* helper functions - TODO - remove in production*/
static std::vector<unsigned char> hex_to_bytes(const std::string& hex) {
    std::vector<unsigned char> bytes;

    for (unsigned int i = 0; i < hex.length(); i += 2) {
            std::string byteString = hex.substr(i, 2);
            unsigned char byte = (unsigned char) strtol(byteString.c_str(), NULL, 16);
            bytes.push_back(byte);
    }
    return bytes;
}

void hex_to_arr(const std::string& hex, uint8_t *arr) {
    std::vector<unsigned char> bytes = hex_to_bytes(hex);
    std::copy(bytes.begin(), bytes.end(), arr);
}

void print_uint8_array(uint8_t *arr, uint size){
    for(int j = 0; j < size; j++) {
        const uint8_t* a = &arr[j];
        unsigned int y = (unsigned int)a[0];
        eosio::print(y);
        eosio::print(",");
    }
    eosio::print("\n");
}
/* end of helper functions */


void sha256(uint8_t* ret, uint8_t* input, uint input_size) {
    capi_checksum256 csum; //TODO - change to get on input
    eosio:sha256((char *)input, input_size, &csum);
    memcpy(ret, csum.hash, 32);
}
