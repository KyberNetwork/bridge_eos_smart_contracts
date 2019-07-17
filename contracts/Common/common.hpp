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
using bytes = vector<uint8_t>;

using namespace eosio;

typedef unsigned int uint;

/* helper functions - TODO - remove in production*/
static bytes hex_to_bytes(const string& hex) {
    bytes bytes;

    for (unsigned int i = 0; i < hex.length(); i += 2) {
            string byteString = hex.substr(i, 2);
            uint8_t byte = (uint8_t) strtol(byteString.c_str(), NULL, 16);
            bytes.push_back(byte);
    }
    return bytes;
}

void hex_to_arr(const string& hex, uint8_t *arr) {
    bytes bytes = hex_to_bytes(hex);
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

uint64_t get_reciept_header_hash(const bytes &receipt_rlp,
                                 capi_checksum256 &header_hash) {

    // get combined sha256 of (sha256(receipt rlp),header hash)
    capi_checksum256 rlp_receipt_hash = sha256(receipt_rlp.data(), receipt_rlp.size());

    bytes combined_hash_input(64);
    std::copy(header_hash.hash, header_hash.hash + 32, combined_hash_input.begin());
    std::copy(rlp_receipt_hash.hash, rlp_receipt_hash.hash + 32, combined_hash_input.begin() + 32);

    capi_checksum256 combined_hash_output = sha256(combined_hash_input.data(), combined_hash_input.size());

    // crop only 64 bits
    return *(uint64_t *)combined_hash_output.hash;
}

uint64_t crop(const uint8_t *full_hash) {
    return *((uint64_t *)full_hash);
}

void async_pay(name from, name to, asset quantity, name dest_contract, string memo) {
    action {
        permission_level{from, "active"_n},
        dest_contract,
        "transfer"_n,
        std::make_tuple(from, to, quantity, memo)
    }.send();
}
