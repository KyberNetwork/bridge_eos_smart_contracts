#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <iostream>
#include <vector>
#include <assert.h>
#include <sstream>
#include <iomanip>

#include "data_sizes.h"
#include "fnv.h"
#include <SHA3/Keccak.h>

#include "input_block_4700000.cpp"
#include "input_block_4699999.cpp"
#include "input_block_5.cpp"
#include "input_block_0.cpp"
#include "proofs_block_4700000.cpp"

using std::endl;
using std::cout;

#define ETHASH_REVISION 23
#define ETHASH_DATASET_BYTES_INIT 1073741824U // 2**30
#define ETHASH_DATASET_BYTES_GROWTH 8388608U  // 2**23
#define ETHASH_CACHE_BYTES_INIT 1073741824U // 2**24
#define ETHASH_CACHE_BYTES_GROWTH 131072U  // 2**17
#define ETHASH_EPOCH_LENGTH 30000U
#define ETHASH_MIX_BYTES 128
#define ETHASH_HASH_BYTES 64
#define ETHASH_DATASET_PARENTS 256
#define ETHASH_CACHE_ROUNDS 3
#define ETHASH_ACCESSES 64
#define ETHASH_DAG_MAGIC_NUM_SIZE 8
#define ETHASH_DAG_MAGIC_NUM 0xFEE1DEADBADDCAFE

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

/////////////// helper functions //////////////////
// FOR DEBUG PURPOSES ONLY
std::string hexStr(uint8_t * data, int len)
{
    std::stringstream ss;
    ss << std::hex;
    for(int i=0;i<len;++i)
        ss << std::setw(2) << std::setfill('0') << (int)data[i];
    return ss.str();
}

static std::vector<unsigned char> HexToBytes(const std::string& hex) {
        std::vector<unsigned char> bytes;

        for (unsigned int i = 0; i < hex.length(); i += 2) {
                std::string byteString = hex.substr(i, 2);
                unsigned char byte = (unsigned char) strtol(byteString.c_str(), NULL, 16);
                bytes.push_back(byte);
        }

        return bytes;
}

void HexToArr(const std::string& hex, uint8_t *arr) {
    std::vector<unsigned char> bytes = HexToBytes(hex);
    std::copy(bytes.begin(), bytes.end(), arr);
}

void printBytes(uint8_t arr[], uint size) {
    for (int i = 0; i < size; i++) {
        printf("%02x", arr[i]);
    }
    printf("\n");
}
///////////////////////////////////////////////


uint8_t* keccak256(struct ethash_h256 const* ret, uint8_t* input, uint input_size) {
    keccakState *st = keccakCreate(256);
    keccakUpdate((uint8_t*)input, 0, input_size, st);
    memcpy((uint8_t*)ret, keccakDigest(st), 32);
}

uint8_t* keccak256(uint8_t* ret, uint8_t* input, uint input_size) {
    keccakState *st = keccakCreate(256);
    keccakUpdate((uint8_t*)input, 0, input_size, st);
    memcpy(ret, keccakDigest(st), 32);
}

uint8_t* keccak512(uint8_t* ret, uint8_t* input, uint input_size) {
    keccakState *st = keccakCreate(512);
    keccakUpdate((uint8_t*)input, 0, input_size, st);
    memcpy(ret, keccakDigest(st), 64);
}

typedef struct ethash_h256 { uint8_t b[32]; } ethash_h256_t;

uint64_t ethash_get_datasize(uint64_t const block_number)
{
    assert(block_number / ETHASH_EPOCH_LENGTH < 2048);
    return dag_sizes[block_number / ETHASH_EPOCH_LENGTH];
}

typedef struct ethash_return_value {
    ethash_h256_t result;
    ethash_h256_t mix_hash;
} ethash_return_value_t;

typedef union node {
    uint8_t bytes[NODE_WORDS * 4];
    uint32_t words[NODE_WORDS];
    uint64_t double_words[NODE_WORDS / 2];
} node;

static bool ethash_hash(
    ethash_return_value_t* ret,
    node const* full_nodes,
    uint64_t full_size,
    ethash_h256_t const header_hash,
    uint64_t const nonce
)
{
    if (full_size % MIX_WORDS != 0) {
        return false;
    }

    // pack hash and nonce together into first 40 bytes of s_mix
    assert(sizeof(node) * 8 == 512);
    node s_mix[MIX_NODES + 1];
    memcpy(s_mix[0].bytes, &header_hash, 32);
    fix_endian64(s_mix[0].double_words[4], nonce);

    // compute sha3-512 hash and replicate across mix
    keccak512(s_mix->bytes, s_mix->bytes, 40);

    fix_endian_arr32(s_mix[0].words, 16);

    node* const mix = s_mix + 1;
    for (uint32_t w = 0; w != MIX_WORDS; ++w) {
        mix->words[w] = s_mix[0].words[w % NODE_WORDS];
    }

    unsigned const page_size = sizeof(uint32_t) * MIX_WORDS;
    unsigned const num_full_pages = (unsigned) (full_size / page_size);

    for (unsigned i = 0; i != ETHASH_ACCESSES; ++i) {
        uint32_t const index = fnv_hash(s_mix->words[0] ^ i, mix->words[i % MIX_WORDS]) % num_full_pages;

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
    keccak256(&ret->result, s_mix->bytes, 64 + 32); // Keccak-256(s + compressed_mix)
    return true;
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
void conventional_encoding(uint8_t *ret, uint8_t *data) {
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
void element_hash(uint8_t *ret /* 16B */, uint8_t *data /* 128B */ ){
  uint8_t conventional[128];
  uint8_t full_hash_res[32];

  conventional_encoding(conventional, data);
  keccak256(full_hash_res, conventional, 128);
  memcpy(ret, full_hash_res + 16, 16); // last 16 bytes
  return;
}

/*
Hash function for 2 sibling nodes (hash) hash returns 16 bytes hash of 2 consecutive elements in a working level.
function hash(a, b) => 16bytes {
  h = keccak256(zeropadded(a), zeropadded(b)) // where zeropadded function prepend 16 bytes of 0 to its param
  return last16Bytes(h)
}
*/
void hash_siblings(uint8_t *ret, uint8_t *a, uint8_t *b){
    uint8_t padded_pair[64] = {0};
    uint8_t full_hash_res[32];

    memcpy(padded_pair + 48, a, 16);
    memcpy(padded_pair + 16, b, 16);

    keccak256(full_hash_res, padded_pair, 64);
    memcpy(ret, full_hash_res + 16, 16); // last 16 bytes
    return;
}

void apply_path(uint index,
                uint8_t *res, //16B
                uint8_t *full_element, //128B
                uint8_t proof[][16], /*does not include root and leaf */
                uint proof_size) {

   uint8_t leaf[16];
   uint8_t left[16];
   uint8_t right[16];

   element_hash(leaf, full_element);

   for(int i = 0; i < proof_size; i++) {
       uint side = index & 0x1;
       if( side ) {
           memcpy(left, leaf, 16);
           memcpy(right, proof[i], 16);
       } else {
           memcpy(right, leaf, 16);
           memcpy(left, proof[i], 16);
       }
       hash_siblings(leaf, left, right);
       index = index / 2;
   }

   memcpy(res, leaf, 16);
}

void hashimoto(ethash_h256_t header_hash, //TODO: get rlp header and not just hash
               uint64_t nonce,
               uint64_t block_number_for_epoch,
               node *full_nodes_arr, // 128 elements
               uint8_t witness_arr[][16]) {// size of MAX_PROOF_LENGTH

}

void test_ethash(std::string *dag_nodes,
                 uint64_t nonce,
                 uint64_t block_number_for_epoch,
                 std::string header_hash_st) {

    node full_nodes_arr[128];
    for(int i = 0; i < 128; i++) {
         HexToArr(dag_nodes[i], full_nodes_arr[i].bytes);
    }

    //////////////////////////
    uint8_t full_element[128];
    uint8_t res[16];
    uint8_t witness_arr[MAX_PROOF_DEPTH][16];
    uint8_t expected_root[16];
    HexToArr(expected_merkle_root_4700000, expected_root);
    uint proof_length = PROOF_LENGTH_4700000;

    for( int k= 0; k < 64; k++) {
        ////tmp until we figure out how to load 64 proofs
        if(k==2) break;
        ///

        for(int i = 0; i < proof_length; i++) {
             HexToArr(proofs_4700000[k][i], witness_arr[i]);
        }

        memcpy(full_element, full_nodes_arr[k*2].bytes, 64);
        memcpy(full_element + 64, full_nodes_arr[k*2 + 1].bytes, 64);

        apply_path(indices_4700000[k],
                   res,
                   full_element,
                   witness_arr, // does not include root and leaf
                   proof_length);

        assert(0==memcmp( res, expected_root, 16));
    }



    ethash_h256_t header_hash;
    HexToArr(header_hash_st, header_hash.b);

    uint64_t full_size = ethash_get_datasize(block_number_for_epoch);


    ethash_return_value_t r;
    bool ret = ethash_hash(&r, full_nodes_arr, full_size, header_hash, nonce);
    if (!ret) {
        cout << "ETHASH HASHING FAILED" << endl;
        exit(3);
    }

    // TODO: verify answer is below difficulty
    cout << "full_size: " << full_size << endl;
    cout << "RESULT: " << hexStr(r.result.b, 32) << endl;
    cout << "MIXHASH: " << hexStr(r.mix_hash.b, 32) << endl;
}



int main(int argc, char **argv) {
    //test_ethash(dag_nodes_0, 0x6d61f75f8e1ffecf, 0, "3c311878b188920ac1b95f96c7a18f81d08f1df1cb170d46140e76631f011172");
    test_ethash(dag_nodes_4700000, 9615896863360164237, 4700000, "de1e91c286c6b05d827e7ac983d3fc566e6139bed9384c711625f9cf1d77749c");
    //test_ethash(dag_nodes_4699999, 2130853672440268436, 4699999,"9bb20d3ef23a6b3cf2665e9779cf94c2de8b5d781c81cc455a1e3afdfd3aa954");
    //test_ethash(dag_nodes_5, 0x0cf446597e767586, 5, "8aa692f0a7bf0444c8e18f85d59f73f20c15e9c314dea0d3ff423b8043625a68");

    return 0;
}
