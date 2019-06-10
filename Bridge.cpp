#define VERIFY 1
#define EOSIO
#ifdef EOSIO
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
typedef unsigned int uint;
#else
#include <iostream>
#include <sstream>
using std::cout;
#include <iomanip>
#endif

#include <string.h>
#include <vector>
#include <assert.h>


#include "data_sizes.h"
//#include "sha3/Keccak.h"
#include "sha3/Keccak.hpp" //TODO: remove when organizing includes.

#include "input/input_block_4700000.hpp"
//#include "input/input_block_4699999.hpp"
//#include "input/input_block_5.hpp"
//#include "input/input_block_0.hpp"
#include "input/proofs_block_4700000.hpp"

using std::endl;


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


struct test_info_struct {
    std::string dag_nodes[128];
    uint64_t nonce;
    uint64_t block_num;
    std::string header_hash_st;
    uint proof_length;
    std::string proofs[2][MAX_PROOF_DEPTH];
    std::string expected_merkle_root;
};




///////////////
#ifdef EOSIO
using namespace eosio;
CONTRACT Bridge : public contract {

    public:
        using contract::contract;

        ACTION start(const std::vector<unsigned char>& dag_vec);
    private:
};
#endif

/////////////// helper functions //////////////////
// FOR DEBUG PURPOSES ONLY
#ifndef EOSIO
std::string hexStr(uint8_t * data, int len)
{
    std::stringstream ss;
    ss << std::hex;
    for(int i=0;i<len;++i)
        ss << std::setw(2) << std::setfill('0') << (int)data[i];
    return ss.str();
}
#endif

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

void HexToArr2(const std::string& hex, uint8_t *arr) {
    for (unsigned int i = 0; i < hex.length(); i += 2) {
            std::string byteString = hex.substr(i, 2);
            unsigned char byte = (unsigned char) strtol(byteString.c_str(), NULL, 16);
            arr[i] = byte;
    }
}

void my_memcpy16B(uint8_t *dst, uint8_t *src) {
    memcpy(dst, src, 16);
    //uint64_t *s = (uint64_t *)src;
    //uint64_t *d = (uint64_t *)dst;

    //dst[0] = s[0];
    //dst[1] = src[1];
}

int my_memcmp16B(uint8_t *a, uint8_t *b) {
    for(int i = 0; i < 16; i++) {
        print("a[i]:", uint(a[i]));
        print("b[i]:", uint(b[i]));
        if (a[i] != b[i]) {
            return 1;
        }
    }
    return 0;
    //uint64_t *s = (uint64_t *)src;
    //uint64_t *d = (uint64_t *)dst;

    //dst[0] = s[0];
    //dst[1] = src[1];
}
#ifndef EOSIO
void printBytes(uint8_t arr[], uint size) {
    for (int i = 0; i < size; i++) {
        printf("%02x", arr[i]);
    }
    printf("\n");
}
#endif

///////////////////////////////////////////////

#define FNV_PRIME 0x01000193
uint32_t fnv_hash(uint32_t const x, uint32_t const y)
{
    return x * FNV_PRIME ^ y;
}

uint8_t* keccak256(struct ethash_h256 const* ret, uint8_t* input, uint input_size) {
    keccakState *st = keccakCreate(256);
    keccakUpdate((uint8_t*)input, 0, input_size, st);
    memcpy((uint8_t*)ret, keccakDigest(st), 32);
}

uint8_t* keccak256(uint8_t* ret, uint8_t* input, uint input_size) {
    keccakState *st = keccakCreate(256);
    keccakUpdate((uint8_t*)input, 0, input_size, st);
    unsigned char *tmp = keccakDigest(st);
    return tmp;
    //memcpy(ret, keccakDigest(st), 32);
}

uint8_t* keccak512(uint8_t* ret, uint8_t* input, uint input_size) {
    keccakState *st = keccakCreate(512);
    keccakUpdate((uint8_t*)input, 0, input_size, st);
    unsigned char *tmp = keccakDigest(st);
    return tmp;
    //print("gere1");
    //memcpy(ret, tmp, 64);
    //print("gere2");
}

typedef struct ethash_h256 { uint8_t b[32]; } ethash_h256_t;

uint64_t ethash_get_datasize(uint64_t const block_number)
{
    assert(block_number / ETHASH_EPOCH_LENGTH < 1023);
    int index = block_number / ETHASH_EPOCH_LENGTH;
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

typedef struct proof_struct {
    uint8_t leaves[MAX_PROOF_DEPTH][16];
} proof;

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

  unsigned char* tmp = keccak256(full_hash_res, conventional, 128);

  my_memcpy16B(ret, tmp + 16); // last 16 bytes
  //memcpy(ret, full_hash_res + 16, 16); // last 16 bytes

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

    my_memcpy16B(padded_pair + 48, a);
    my_memcpy16B(padded_pair + 16, b);

    unsigned char* tmp = keccak256(full_hash_res, padded_pair, 64);
    my_memcpy16B(ret, tmp + 16); // last 16 bytes
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

   print("proof_size: ",proof_size);
   for(int i = 0; i < proof_size; i++) {
       uint side = index & 0x1;
       if( side ) {
           my_memcpy16B(left, leaf);
           my_memcpy16B(right, proof[i]);
       } else {
           my_memcpy16B(right, leaf);
           my_memcpy16B(left, proof[i]);
       }
       hash_siblings(leaf, left, right);
       index = index / 2;
   }

   //print("            leaf:");
   //print("x",(uint)leaf[0]);
   //print("x",(uint)leaf[1]);
   //print("x",(uint)leaf[2]);
   //print("x",(uint)leaf[3]);
   my_memcpy16B(res, leaf);
}

static bool hashimoto(
    ethash_return_value_t* ret,
    node const* full_nodes,
    ethash_h256_t const header_hash,
    uint64_t const nonce,
    uint64_t block_num,
    proof witnesses[],
    uint proof_length,
    uint8_t *expected_root
)
{
    uint64_t full_size = ethash_get_datasize(block_num);
    if (full_size % MIX_WORDS != 0) {
        return false;
    }

    // pack hash and nonce together into first 40 bytes of s_mix
    assert(sizeof(node) * 8 == 512);
    node s_mix[MIX_NODES + 1];

    memcpy(s_mix[0].bytes, &header_hash, 32);
    fix_endian64(s_mix[0].double_words[4], nonce);

    // compute sha3-512 hash and replicate across mix
    //print("s_mix->bytes: ");
    //for (int i = 0; i < 64; i++){
    //    print_f("," ,s_mix->bytes[i]);
    //}

    unsigned char *tmp = keccak512(s_mix->bytes, s_mix->bytes, 40);
    memcpy(s_mix->bytes, tmp, 64);

    //return true;
    fix_endian_arr32(s_mix[0].words, 16);
    node* const mix = s_mix + 1;
    for (uint32_t w = 0; w != MIX_WORDS; ++w) {
        mix->words[w] = s_mix[0].words[w % NODE_WORDS];
    }

    unsigned const page_size = sizeof(uint32_t) * MIX_WORDS;
    unsigned const num_full_pages = (unsigned) (full_size / page_size);

    /*
    print("\n");
    print("num_full_pages: ", num_full_pages);
    print("\n");
    */

    // smix
    /*
    for (int i =0; i < MIX_NODES + 1; i++) {
        print("i=", i); print(":");
        for(int j =0; j <(NODE_WORDS * 4); j++) {
            uint8_t* a = &s_mix->bytes[j];
            unsigned int y = (unsigned int)a[0];
            print(y);
            print(",");
        }
        print("\n");
    }
    */

    //mix
    /*
    for (uint32_t w = 0; w != MIX_WORDS; ++w) {
        print(mix->words[w]);
        print(",");
    }
    */

    print("\n");
    for (unsigned i = 0; i != ETHASH_ACCESSES; ++i) {

        // here is the problem:
        uint32_t const index = fnv_hash(s_mix->words[0] ^ i, mix->words[i % MIX_WORDS]) % num_full_pages;
        /*
        print("\n");
        print("index: ", index);
        print("\n");
        */

        ///
        if(i < 2 && VERIFY) { //TODO: remove check once we figure out how to load 64 proofs
            uint8_t full_element[128];
            uint8_t res[16];
            //uint8_t *res = new uint8_t[16];

            memcpy(full_element, full_nodes[i*2].bytes, 64);
            memcpy(full_element + 64, full_nodes[i*2 + 1].bytes, 64);
            apply_path(index, res, full_element, witnesses[i].leaves, proof_length);
            /*
            print("expected_root[0]:",(uint)expected_root[0]);
            print("res[0]:",(uint)res[0]);
            */

            // failing here
            assert(0==my_memcmp16B(res,expected_root));
            //assert(0==memcmp(res, expected_root, 16));
            print("here5.6");

        }
        for (unsigned n = 0; n != MIX_NODES; ++n) {
            node const* dag_node = &full_nodes[i*2 + n];
/*
            for(int j =0; j <64; j++) {
                const uint8_t* a = &dag_node->bytes[j];
                unsigned int y = (unsigned int)a[0];
                print(y);
                print(",");
            }
            print("\n");
*/
            for (unsigned w = 0; w != NODE_WORDS; ++w) {
                mix[n].words[w] = fnv_hash(mix[n].words[w], dag_node->words[w]);
            }
        }
    }
    print("here6");

    // compress mix
    for (uint32_t w = 0; w != MIX_WORDS; w += 4) {
        print("here6.1: ");
        print(w);
        print("\n");
        uint32_t reduction = mix->words[w + 0];
        reduction = reduction * FNV_PRIME ^ mix->words[w + 1];
        reduction = reduction * FNV_PRIME ^ mix->words[w + 2];
        reduction = reduction * FNV_PRIME ^ mix->words[w + 3];
        mix->words[w / 4] = reduction;
    }

    print("here6.2");
    fix_endian_arr32(mix->words, MIX_WORDS / 4);
    print("here6.3");
    memcpy(&ret->mix_hash, mix->bytes, 32);
    print("here6.4");
    // final Keccak hash

    //keccak256(&ret->result, s_mix->bytes, 64 + 32); // Keccak-256(s + compressed_mix)
    unsigned char* tmp2 = keccak256(&ret->result.b[0], s_mix->bytes, 64 + 32);
    my_memcpy16B(&ret->result.b[0], tmp2); // first 16 bytes
    my_memcpy16B(&ret->result.b[0] + 16, tmp2 + 16); // last 16 bytes

    print("here6.5");
    print("\n");
    // print result
    print("result: ");
    for(int j =0; j < 32; j++) {
        const uint8_t* a = &ret->result.b[j];
        unsigned int y = (unsigned int)a[0];
        print(y);
        print(",");
    }
    print("\n");

    // print mixed hash
    print("mixed_hash: ");
    for(int j =0; j < 32; j++) {
        const uint8_t* a = &ret->mix_hash.b[j];
        unsigned int y = (unsigned int)a[0];
        print(y);
        print(",");
    }
    print("\n");

    // TODO: verify answer is below difficulty

    return true;
}

/*
void test_hashimoto(std::string *dag_nodes,
                 uint64_t nonce,
                 uint64_t block_num,
                 std::string header_hash_st,
                 uint proof_length, //PROOF_LENGTH_4700000
                 std::string proofs[][MAX_PROOF_DEPTH], //proofs_4700000
                 std::string expected_merkle_root //expected_merkle_root_4700000
*/

//#include "scripts/try.h"
void test_hashimoto(struct test_info_struct* test_info, const std::vector<unsigned char>& dag_vec) {

    std::vector<unsigned char> bytes;

    // return; 1164 ms
    node* full_nodes_arr = new node[128];

    //return; 1246

    //print(dag_vec.size());
    for(int i = 0; i < 128; i++) {
        for(int j = 0; j < 64; j++){
            full_nodes_arr[i].bytes[j] = dag_vec[i * 64 + j];
        }
        //HexToArr2(test_info->dag_nodes[i], full_nodes_arr[i].bytes);
    }

    // was 25280 ms, now 6278 ms only
    //return; 26956 ms

    proof *witnesses = new proof[64];
    for(int i = 0; i < 64; i++) {
        if(i>=2) break; //TODO: remove check once we figure out how to load 64 proofs
        for(int m = 0; m < test_info->proof_length; m++) {
            //HexToArr2(test_info->proofs[i][m], (witnesses[i]).leaves[m]);
            HexToArr(test_info->proofs[i][m], (witnesses[i]).leaves[m]);
        }
    }

    uint8_t expected_root[16];
    //HexToArr2(test_info->expected_merkle_root, expected_root);
    HexToArr(test_info->expected_merkle_root, expected_root);

    ethash_h256_t header_hash;
    HexToArr(test_info->header_hash_st, header_hash.b);

    ethash_return_value_t r;
    bool ret = hashimoto(&r,
                         full_nodes_arr,
                         header_hash,
                         test_info->nonce,
                         test_info->block_num,
                         witnesses,
                         test_info->proof_length,
                         expected_root);
    if (!ret) {
#ifndef EOSIO
        cout << "ETHASH HASHING FAILED" << endl;
        exit(3);
#endif
    }
#ifndef EOSIO
    cout << "RESULT: " << hexStr(r.result.b, 32) << endl;
    cout << "MIXHASH: " << hexStr(r.mix_hash.b, 32) << endl;
#endif

}

#ifdef EOSIO
ACTION Bridge::start(const std::vector<unsigned char>& dag_vec) {
#else
int main() {
#endif

    //print("here:", (unsigned int)dag_nodes[0]);

    //print((const char *)dag_nodes[1]);

    struct test_info_struct* test_info = new struct test_info_struct;
    memcpy(test_info->dag_nodes, dag_nodes_4700000, 128 * sizeof(node));
    test_info->nonce = 9615896863360164237;
    test_info->block_num = 4700000;
    test_info->header_hash_st = "de1e91c286c6b05d827e7ac983d3fc566e6139bed9384c711625f9cf1d77749c";
    test_info->proof_length = PROOF_LENGTH_4700000;

    //return; 1426 ms
    for(int i = 0; i < 25; i++ ){
        test_info->proofs[0][i] = proofs_4700000[0][i];
    }
    //return; 1940 ms
    for(int i = 0; i < 25; i++ ){
        test_info->proofs[1][i] = proofs_4700000[1][i];
    }
    test_info->expected_merkle_root = expected_merkle_root_4700000;

    //return; 1777 ms
    //test_hashimoto(dag_nodes_0, 0x6d61f75f8e1ffecf, 0, "3c311878b188920ac1b95f96c7a18f81d08f1df1cb170d46140e76631f011172");
    test_hashimoto(test_info, dag_vec);
    //test_hashimoto(dag_nodes_4699999, 2130853672440268436, 4699999,"9bb20d3ef23a6b3cf2665e9779cf94c2de8b5d781c81cc455a1e3afdfd3aa954");
    //test_hashimoto(dag_nodes_5, 0x0cf446597e767586, 5, "8aa692f0a7bf0444c8e18f85d59f73f20c15e9c314dea0d3ff423b8043625a68");

    return;
}

#ifdef EOSIO
extern "C" {

    void apply(uint64_t receiver, uint64_t code, uint64_t action) {

        if (code == receiver){
            switch( action ) {
                EOSIO_DISPATCH_HELPER( Bridge, (start))
            }
        }
        eosio_exit(0);
    }
}
#endif
