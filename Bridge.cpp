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
#include "LongMult.hpp"

#define ETHASH_EPOCH_LENGTH 30000U
#define ETHASH_MIX_BYTES 128
#define ETHASH_ACCESSES 64
#define MERKLE_CONVENTIONAL_LEN 128
#define MERKLE_ELEMENT_LEN 32

#define NODE_BYTES 64
#define NODE_WORDS (64/4)
#define MIX_WORDS (ETHASH_MIX_BYTES/4) // 32
#define MIX_NODES (MIX_WORDS / NODE_WORDS) // 2

#define FNV_PRIME 0x01000193

#define MAX_PROOF_DEPTH 40

#define fix_endian32(dst_ ,src_) dst_ = src_
#define fix_endian32_same(val_)
#define fix_endian64(dst_, src_) dst_ = src_
#define fix_endian64_same(val_)
#define fix_endian_arr32(arr_, size_)
#define fix_endian_arr64(arr_, size_)

typedef union node {
    uint8_t bytes[NODE_WORDS * 4];
    uint32_t words[NODE_WORDS];
    uint64_t double_words[NODE_WORDS / 2];
} node;

typedef struct proof_struct {
    uint8_t leaves[MAX_PROOF_DEPTH][MERKLE_ELEMENT_LEN];
} proof;

struct header_info_struct {
    uint64_t nonce;
    uint64_t block_num;
    uint8_t header_hash[32];
    uint8_t *expected_root;
    uint8_t *difficulty;
    uint difficulty_len;
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

        ACTION storeroots(const std::vector<uint64_t>& epoch_num_vec,
                          const std::vector<unsigned char>& root_vec);

        TABLE roots {
            uint64_t                     epoch_num;
            std::vector<unsigned char>   root;
            uint64_t        primary_key() const { return epoch_num; }
        };

        typedef eosio::multi_index<"roots"_n, roots> roots_type;

    private:
        void parse_header(struct header_info_struct* header_info,
                          const std::vector<unsigned char>& header_rlp_vec);
};

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
    eosio_assert(block_number / ETHASH_EPOCH_LENGTH < EPOCH_ELEMENTS, "block number too big");
    int index = block_number / ETHASH_EPOCH_LENGTH;
    int array_num = index / EPOCH_SINGLE_ARRAY_SIZE;
    int index_in_array = index % EPOCH_SINGLE_ARRAY_SIZE;

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
  sha256(ret, conventional, MERKLE_CONVENTIONAL_LEN);
  return;
}

void merkle_hash_siblings(uint8_t *ret, uint8_t *a, uint8_t *b){
    uint8_t padded_pair[MERKLE_ELEMENT_LEN * 2] = {0};

    memcpy(padded_pair + MERKLE_ELEMENT_LEN, a, MERKLE_ELEMENT_LEN);
    memcpy(padded_pair + 0, b, MERKLE_ELEMENT_LEN);

    sha256(ret, padded_pair, MERKLE_ELEMENT_LEN * 2);
    return;
}

void merkle_apply_path(uint index,
                uint8_t *res, // 32B
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

void verify_header(struct header_info_struct* header_info,
                   const std::vector<unsigned char>& dag_vec,
                   const std::vector<unsigned char>& proof_vec,
                   uint proof_length) {

    uint8_t *header_hash = header_info->header_hash;
    uint64_t const nonce = header_info->nonce;
    uint64_t block_num = header_info->block_num;
    uint8_t *expected_root = header_info->expected_root;
    uint8_t *difficulty = header_info->difficulty;
    uint difficulty_len = header_info->difficulty_len;

    uint64_t full_size = ethash_get_datasize(block_num);
    eosio_assert(full_size % MIX_WORDS == 0, "wrong full size");

    // pack hash and nonce together into first 40 bytes of s_mix
    node s_mix[MIX_NODES + 1];

    memset(s_mix[0].bytes, 0, 64);
    memset(s_mix[1].bytes, 0, 64);
    memset(s_mix[2].bytes, 0, 64);

    memcpy(s_mix[0].bytes, header_hash, 32);
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

        if(VERIFY && i < 64) {
            uint8_t res[MERKLE_ELEMENT_LEN];
            uint8_t *current_item = (uint8_t *)(&dag_vec[i * NODE_BYTES * 2]);
            uint8_t *proofs_start = (uint8_t *)(&proof_vec[i * proof_length * MERKLE_ELEMENT_LEN]);

            merkle_apply_path(index, res, current_item, proofs_start, proof_length);
            eosio_assert(0 == memcmp(res, expected_root, MERKLE_ELEMENT_LEN),
                         "merkle verification failure");
        }
        for (unsigned n = 0; n != MIX_NODES; ++n) {

            uint32_t *current_item_word = (uint32_t *)(&dag_vec[NODE_BYTES * (i * 2 + n)]);
            for (unsigned w = 0; w != NODE_WORDS; ++w) {
                mix[n].words[w] = fnv_hash(mix[n].words[w], current_item_word[w]);
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

    // final Keccak hash
    unsigned char ethash[32] = {0};
    keccak256(ethash, s_mix->bytes, 64 + 32);

    print("ethash: ");
    print_uint8_array(ethash, 32);

    print("mixed_hash: ");
    print_uint8_array(mix->bytes, 32);

    //check ethash result meets the rquire difficulty
    eosio_assert(check_pow(difficulty, difficulty_len, ethash), "pow difficulty failure");

    return;
}

void hash_header_rlp(struct header_info_struct* header_info,
                     const std::vector<unsigned char>& header_rlp_vec,
                     rlp_item* items) {

    int trim_len = remove_last_field_from_rlp((unsigned char *)header_rlp_vec.data(),
                                              items[NONCE_FIELD].len);
    eosio_assert(trim_len == (header_rlp_vec.size() - 9), "wrong 1st trim length");

    trim_len = remove_last_field_from_rlp((unsigned char *)header_rlp_vec.data(),
                                          items[MIX_HASH_FIELD].len);
    eosio_assert(trim_len == (header_rlp_vec.size() - 42), "wrong 2nd trim length");

    keccak256(header_info->header_hash, (unsigned char *)header_rlp_vec.data(), trim_len);
}

void Bridge::parse_header(struct header_info_struct* header_info,
                          const std::vector<unsigned char>& header_rlp_vec) {
    rlp_item items[15];
    decode_list((unsigned char *)header_rlp_vec.data(), items);

    header_info->nonce = get_uint64(&items[NONCE_FIELD]);
    header_info->block_num = get_uint64(&items[NUMBER_FIELD]);
    header_info->difficulty = items[DIFFICULTY_FIELD].content;
    header_info->difficulty_len = items[DIFFICULTY_FIELD].len;

    hash_header_rlp(header_info, header_rlp_vec, items);

    // get pre-stored root
    roots_type roots_inst(_self, _self.value);
    auto root_entry = roots_inst.get(header_info->block_num / 30000U, "dag root not pre-stored");
    header_info->expected_root = root_entry.root.data();
}

void store_header(struct header_info_struct* header_info) {
    // TODO - implement with relevant data structure.
}

ACTION Bridge::storeroots(const std::vector<uint64_t>& epoch_num_vec,
                          const std::vector<unsigned char>& root_vec){

    require_auth(_self);

    eosio_assert(epoch_num_vec.size() * MERKLE_ELEMENT_LEN == root_vec.size(),
                 "block num and root vectors mismatch");
    roots_type roots_inst(_self, _self.value);

    for(uint i = 0; i < epoch_num_vec.size(); i++){
        auto itr = roots_inst.find(epoch_num_vec[i]);
        bool token_exists = (itr != roots_inst.end());
        if (!token_exists) {
            roots_inst.emplace(_self, [&](auto& s) {
                s.epoch_num = epoch_num_vec[i];

                // TODO: improve pushing to use memcpy or std::cpy
                for(uint j = 0; j < MERKLE_ELEMENT_LEN; j++ ){
                    s.root.push_back(root_vec[i * MERKLE_ELEMENT_LEN + j]);
                }
            });
        } else {
            roots_inst.modify(itr, _self, [&](auto& s) {
                s.epoch_num = epoch_num_vec[i];

                // TODO: improve pushing to use memcpy or std::cpy
                for(uint j = 0; j < MERKLE_ELEMENT_LEN; j++ ){
                    s.root.push_back(root_vec[i * MERKLE_ELEMENT_LEN + j]);
                }
            });
        }
    }
};

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
                EOSIO_DISPATCH_HELPER( Bridge, (verify)(storeroots))
            }
        }
        eosio_exit(0);
    }
}
