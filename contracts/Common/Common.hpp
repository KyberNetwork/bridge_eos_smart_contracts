#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/types.h>
#include <eosiolib/crypto.h>
#include <eosiolib/asset.hpp>
#include <eosiolib/symbol.hpp>
#include <eosiolib/singleton.hpp>

#include "sha3/sha3.hpp"

#include <string>
#include <vector>

using std::string;
using std::vector;
using namespace eosio;

typedef unsigned int uint;

/* helper functions - TODO - remove in production*/
static vector<uint8_t> hex_to_bytes(const string& hex) {
    vector<uint8_t> bytes;

    for (unsigned int i = 0; i < hex.length(); i += 2) {
            string byteString = hex.substr(i, 2);
            uint8_t byte = (uint8_t) strtol(byteString.c_str(), NULL, 16);
            bytes.push_back(byte);
    }
    return bytes;
}

void hex_to_arr(const string& hex, uint8_t *arr) {
    vector<uint8_t> bytes = hex_to_bytes(hex);
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

capi_checksum256 sha256(const uint8_t* input, uint input_size) {
    capi_checksum256 ret;
    eosio:sha256((char *)input, input_size, &ret);
    return ret;
}

uint64_t get_reciept_header_hash(const vector<uint8_t> &rlp_receipt,
                                 capi_checksum256 &header_hash) {

    // get combined sha256 of (sha256(receipt rlp),header hash)
    capi_checksum256 rlp_receipt_hash = sha256(rlp_receipt.data(), rlp_receipt.size());

    vector<uint8_t> combined_hash_input(64);
    std::copy(header_hash.hash, header_hash.hash + 32, combined_hash_input.begin());
    std::copy(rlp_receipt_hash.hash, rlp_receipt_hash.hash + 32, combined_hash_input.begin() + 32);

    capi_checksum256 combined_hash_output = sha256(combined_hash_input.data(), combined_hash_input.size());

    // crop only 64 bits
    return *(uint64_t *)combined_hash_output.hash;
}
