#define VERIFY true

#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/types.h>
#include <eosiolib/crypto.h>
typedef unsigned int uint;

#include <string.h>
#include <vector>

#include "data_sizes.h"
#include "sha3/sha3.hpp"
#include "Rlp.hpp"

#define ETHASH_EPOCH_LENGTH 30000U
#define ETHASH_MIX_BYTES 128
#define ETHASH_ACCESSES 64

#define NODE_WORDS (64/4)
#define MIX_WORDS (ETHASH_MIX_BYTES/4) // 32
#define MIX_NODES (MIX_WORDS / NODE_WORDS) // 2

#define MAX_PROOF_DEPTH 40

#define fix_endian32(dst_ ,src_) dst_ = src_
#define fix_endian32_same(val_)
#define fix_endian64(dst_, src_) dst_ = src_
#define fix_endian64_same(val_)
#define fix_endian_arr32(arr_, size_)
#define fix_endian_arr64(arr_, size_)

typedef struct ethash_h256 { uint8_t b[32]; } ethash_h256_t;

typedef struct ethash_return_value {
    ethash_h256_t result;
    ethash_h256_t mix_hash;
} ethash_return_value_t;

typedef union node {
    uint8_t bytes[NODE_WORDS * 4];
    uint32_t words[NODE_WORDS];
    uint64_t double_words[NODE_WORDS / 2];
} node;

typedef struct proof_struct {
    uint8_t leaves[MAX_PROOF_DEPTH][16];
} proof;

struct header_info_struct {
    uint64_t nonce;
    uint64_t block_num;
    ethash_h256_t header_hash;
    uint proof_length;
    uint8_t expected_root[16];
    uint8_t difficulty;
};

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

using namespace eosio;
CONTRACT Bridge : public contract {

    public:
        using contract::contract;

        ACTION verify(const std::vector<unsigned char>& header_rlp_vec,
                      const std::vector<unsigned char>& dag_vec,
                      const std::vector<unsigned char>& proof_vec,
                      uint proof_length);

    private:
};

#define FNV_PRIME 0x01000193
uint32_t fnv_hash(uint32_t const x, uint32_t const y)
{
    return x * FNV_PRIME ^ y;
}

void sha256(uint8_t* ret, uint8_t* input, uint input_size) {
    capi_checksum256 csum; //TODO - change to get on input
    eosio:sha256((char *)input, input_size, &csum);
    memcpy(ret, csum.hash, 32);
}

void keccak256(uint8_t* ret, uint8_t* input, uint input_size) {
    sha3_ctx shactx;
    rhash_keccak_256_init(&shactx);
    rhash_keccak_update(&shactx, input, input_size);
    rhash_keccak_final(&shactx, ret);

}

void keccak512(uint8_t* ret, uint8_t* input, uint input_size) {
    sha3_ctx shactx;
    rhash_keccak_512_init(&shactx);
    rhash_keccak_update(&shactx, input, input_size);
    rhash_keccak_final(&shactx, ret);
}

uint64_t ethash_get_datasize(uint64_t const block_number)
{
    eosio_assert(block_number / ETHASH_EPOCH_LENGTH < 1000, "block number too big");
    int index = block_number / ETHASH_EPOCH_LENGTH;
    int array_num = index / 200;
    int index_in_array = index % 200;

    uint64_t* array;
    switch (array_num) {
        case 0:
            array = dag_sizes_0;
            break;
        case 1:
            array = dag_sizes_1;
            break;
        case 2:
            array = dag_sizes_2;
            break;
        case 3:
            array = dag_sizes_3;
            break;
        case 4:
            array = dag_sizes_4;
            break;
    }

    return array[index_in_array];
}

void reverseBytes(uint8_t *ret, uint8_t *data, uint size) {
    for( int i = 0; i < size; i++ ) {
        ret[i] = data[size - 1 - i];
    }
}

/*
assume the element is abcd where a, b, c, d are 32 bytes word
first = concat(reverse(a), reverse(b)) where reverse reverses the bytes
second = concat(reverse(c), reverse(d))
conventional encoding of abcd is concat(first, second)
*/
void merkle_conventional_encoding(uint8_t *ret, uint8_t *data) {
    reverseBytes(ret , data, 32);
    reverseBytes(ret + 32, data + 32, 32);
    reverseBytes(ret + 64, data + 64, 32);
    reverseBytes(ret + 96, data + 96, 32);
    return;
}

/*
 * Hash function for data element(elementhash) elementhash returns 16 bytes hash of the dataset element.
    function elementhash(data) => 16bytes {
          h = keccak256(conventional(data)) // conventional function is defined in dataset element encoding section
          return last16Bytes(h)
    }
 */
void merkle_element_hash(uint8_t *ret /* 16B */, uint8_t *data /* 128B */ ){
  uint8_t conventional[128];
  merkle_conventional_encoding(conventional, data);

  unsigned char tmp[32];
  sha256(tmp, conventional, 128);
  memcpy(ret, tmp + 16, 16); // last 16 bytes
  return;
}

/*
Hash function for 2 sibling nodes (hash) hash returns 16 bytes hash of 2 consecutive elements in a working level.
function hash(a, b) => 16bytes {
  h = keccak256(zeropadded(a), zeropadded(b)) // where zeropadded function prepend 16 bytes of 0 to its param
  return last16Bytes(h)
}
*/
void merkle_hash_siblings(uint8_t *ret, uint8_t *a, uint8_t *b){
    uint8_t padded_pair[64] = {0};

    memcpy(padded_pair + 48, a, 16);
    memcpy(padded_pair + 16, b, 16);

    unsigned char tmp[32];
    sha256(tmp, padded_pair, 64);
    memcpy(ret, tmp + 16, 16); // last 16 bytes
    return;
}

void merkle_apply_path(uint index,
                uint8_t *res, //16B
                uint8_t *full_element, //128B
                uint8_t proof[][16], /* does not include root and leaf */
                uint proof_size) {
   uint8_t *leaf = res;
   uint8_t *left;
   uint8_t *right;

   merkle_element_hash(leaf, full_element);

   for(int i = 0; i < proof_size; i++) {
       uint side = index & 0x1;
       if( side ) {
           left = leaf;
           right = proof[i];
       } else {
           right = leaf;
           left = proof[i];
       }
       merkle_hash_siblings(leaf, left, right);
       index = index / 2;
   }
}

static bool hashimoto(
    ethash_return_value_t* ret,
    node const* full_nodes,
    ethash_h256_t const header_hash,
    uint64_t const nonce,
    uint64_t block_num,
    proof witnesses[],
    uint proof_length,
    uint8_t *expected_root,
    uint64_t difficulty
)
{
    uint64_t full_size = ethash_get_datasize(block_num);
    if (full_size % MIX_WORDS != 0) {
        return false;
    }

    // pack hash and nonce together into first 40 bytes of s_mix
    node s_mix[MIX_NODES + 1];

    memset(s_mix[0].bytes, 0, 64);
    memset(s_mix[1].bytes, 0, 64);
    memset(s_mix[2].bytes, 0, 64);

    memcpy(s_mix[0].bytes, &header_hash, 32);
    fix_endian64(s_mix[0].double_words[4], nonce);

    // compute sha3-512 hash and replicate across mix

    unsigned char res[64] = {0};
    keccak512(res, s_mix->bytes, 40);
    memcpy(s_mix->bytes, res, 64);

    fix_endian_arr32(s_mix[0].words, 16);
    node* const mix = s_mix + 1;
    for (uint32_t w = 0; w != MIX_WORDS; ++w) {
        mix->words[w] = s_mix[0].words[w % NODE_WORDS];
    }

    unsigned const page_size = sizeof(uint32_t) * MIX_WORDS;
    unsigned const num_full_pages = (unsigned) (full_size / page_size);

    for (unsigned i = 0; i != ETHASH_ACCESSES; ++i) {

        uint32_t const index = fnv_hash(s_mix->words[0] ^ i, mix->words[i % MIX_WORDS]) % num_full_pages;

        if(i < 30 && VERIFY) { //TODO: remove check once we figure out how to load 64 proofs
            uint8_t res[16];
            merkle_apply_path(index, res, (uint8_t *)full_nodes[i*2].bytes, witnesses[i].leaves, proof_length);
            eosio_assert(0 == memcmp(res,expected_root,16), "merkle veification failure");
        }
        for (unsigned n = 0; n != MIX_NODES; ++n) {
            node const* dag_node = &full_nodes[i*2 + n];

            for (unsigned w = 0; w != NODE_WORDS; ++w) {
                mix[n].words[w] = fnv_hash(mix[n].words[w], dag_node->words[w]);
            }
        }
    }

    // compress mix
    for (uint32_t w = 0; w != MIX_WORDS; w += 4) {
        uint32_t reduction = mix->words[w + 0];
        reduction = reduction * FNV_PRIME ^ mix->words[w + 1];
        reduction = reduction * FNV_PRIME ^ mix->words[w + 2];
        reduction = reduction * FNV_PRIME ^ mix->words[w + 3];
        mix->words[w / 4] = reduction;
    }

    fix_endian_arr32(mix->words, MIX_WORDS / 4);
    memcpy(&ret->mix_hash, mix->bytes, 32);

    // final Keccak hash
    unsigned char tmp[32] = {0};
    keccak256(tmp, s_mix->bytes, 64 + 32);
    memcpy(&ret->result.b[0], tmp, 16); // first 16 bytes
    memcpy(&ret->result.b[0] + 16, tmp + 16, 16); // last 16 bytes

    print("result: ");
    print_uint8_array(ret->result.b, 32);

    print("mixed_hash: ");
    print_uint8_array(ret->mix_hash.b, 32);

    // TODO: verify answer is below difficulty
    return true;
}

void verify_header(struct header_info_struct* header_info,
                   const std::vector<unsigned char>& dag_vec,
                   const std::vector<unsigned char>& proof_vec,
                   uint proof_length) {

    node* full_nodes_arr = new node[128];
    for(int i = 0; i < 128; i++) {
        for(int j = 0; j < 64; j++){
            full_nodes_arr[i].bytes[j] = dag_vec[i * 64 + j];
        }
    }

    proof *witnesses = new proof[64];
    for(int i = 0; i < 64; i++) {
        for(int m = 0; m < proof_length; m++) {
            for (int k = 0; k < 16; k ++){
                witnesses[i].leaves[m][k] =
                    proof_vec[i * proof_length * 16 + m *16 + k];
            }
        }
    }

    ethash_return_value_t r;
    bool ret = hashimoto(&r,
                         full_nodes_arr,
                         header_info->header_hash,
                         header_info->nonce,
                         header_info->block_num,
                         witnesses,
                         proof_length,
                         header_info->expected_root,
                         header_info->difficulty);
    if (!ret) {
        // TODO: implement failure case
    }
}

void hash_header_rlp(struct header_info_struct* header_info,
                     const std::vector<unsigned char>& header_rlp_vec,
                     rlp_item* items) {

    int trim_len = remove_last_field_from_rlp((unsigned char *)header_rlp_vec.data(), items[14].len);
    eosio_assert(trim_len == (header_rlp_vec.size() - 9), "wrong 1st trim length");

    trim_len = remove_last_field_from_rlp((unsigned char *)header_rlp_vec.data(), items[13].len);
    eosio_assert(trim_len == (header_rlp_vec.size() - 42), "wrong 2nd trim length");

    keccak256(header_info->header_hash.b,
              (unsigned char *)header_rlp_vec.data(),
              trim_len);

}

void parse_header(struct header_info_struct* header_info,
                  const std::vector<unsigned char>& header_rlp_vec) {

    rlp_item items[15];
    decode_list((unsigned char *)header_rlp_vec.data(), items);

    header_info->nonce = get_uint64(&items[NONCE_FIELD]);
    header_info->block_num = get_uint64(&items[NUMBER_FIELD]);
    header_info->difficulty = get_uint64(&items[DIFFICULTY_FIELD]);

    hash_header_rlp(header_info, header_rlp_vec, items);

    // TODO - get proofs related data from pre-computed storage.
    header_info->proof_length = 25;
    hex_to_arr("d0eb4a9ff0dc08a9149b275e3a64e93d", header_info->expected_root);
}

void store_header(struct header_info_struct* header_info) {
    // TODO - implement with relevant data structure.
}

ACTION Bridge::verify(const std::vector<unsigned char>& header_rlp_vec,
                      const std::vector<unsigned char>& dag_vec,
                      const std::vector<unsigned char>& proof_vec,
                      uint proof_length) {

    struct header_info_struct header_info;
    parse_header(&header_info, header_rlp_vec);
    verify_header(&header_info, dag_vec, proof_vec, proof_length);
    store_header(&header_info);
    return;
}

extern "C" {

    void apply(uint64_t receiver, uint64_t code, uint64_t action) {

        if (code == receiver){
            switch( action ) {
                EOSIO_DISPATCH_HELPER( Bridge, (verify))
            }
        }
        eosio_exit(0);
    }
}
