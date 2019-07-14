#include <eosiolib/eosio.hpp>
#include <eosiolib/types.h>

using std::vector;

#define MERKLE_CONVENTIONAL_LEN 128
#define MERKLE_ELEMENT_LEN 16

void reverseBytes(uint8_t *ret, uint8_t *data, uint size) {
    for( int i = 0; i < size; i++ ) {
        ret[i] = data[size - 1 - i];
    }
}

void merkle_conventional_encoding(uint8_t *ret, uint8_t *data) {
    reverseBytes(ret , data, 32);
    reverseBytes(ret + 32, data + 32, 32);
    reverseBytes(ret + 64, data + 64, 32);
    reverseBytes(ret + 96, data + 96, 32);
    return;
}

void merkle_element_hash(uint8_t *ret, uint8_t *data){
  uint8_t conventional[MERKLE_CONVENTIONAL_LEN];
  merkle_conventional_encoding(conventional, data);

  capi_checksum256 tmp = sha256(conventional, 128);
  memcpy(ret, tmp.hash + 16, 16); // last 16 bytes
  return;
}

void merkle_hash_siblings(uint8_t *ret, uint8_t *a, uint8_t *b){
    uint8_t padded_pair[64] = {0};

    memcpy(padded_pair + 48, a, 16);
    memcpy(padded_pair + 16, b, 16);

    capi_checksum256 tmp = sha256(padded_pair, 64);
    memcpy(ret, tmp.hash + 16, 16); // last 16 bytes
    return;
}

void merkle_apply_path(uint index,
                uint8_t *res, // 16B
                uint8_t *full_element, // 128B
                uint8_t *proofs_start, /* does not include root and leaf */
                uint proof_size) {
   uint8_t *leaf = res;
   uint8_t *left;
   uint8_t *right;

   merkle_element_hash(leaf, full_element);

   for(int i = 0; i < proof_size; i++) {
       uint8_t *current_proof = &proofs_start[i * MERKLE_ELEMENT_LEN];
       uint side = index & 0x1;
       if( side ) {
           left = leaf;
           right = current_proof;
       } else {
           right = leaf;
           left = current_proof;
       }
       merkle_hash_siblings(leaf, left, right);
       index = index / 2;
   }
}
