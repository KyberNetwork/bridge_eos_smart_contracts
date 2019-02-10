#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <iostream>
#include <vector>
#include <assert.h>

#include "data_sizes.h"
#include "sha3.h"
#include "fnv.h"


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

#define fix_endian32(dst_ ,src_) dst_ = src_
#define fix_endian32_same(val_)
#define fix_endian64(dst_, src_) dst_ = src_
#define fix_endian64_same(val_)
#define fix_endian_arr32(arr_, size_)
#define fix_endian_arr64(arr_, size_)

/////////////// helper functions //////////////////
static std::vector<unsigned char> HexToBytes(const std::string& hex) {
        std::vector<unsigned char> bytes;

        for (unsigned int i = 0; i < hex.length(); i += 2) {
                std::string byteString = hex.substr(i, 2);
                unsigned char byte = (unsigned char) strtol(byteString.c_str(), NULL, 16);
                bytes.push_back(byte);
        }

        return bytes;
}

void printBytes(uint8_t arr[], uint size) {
    for (int i = 0; i < size; i++) {
        printf("%02x", arr[i]);
    }
    printf("\n");
}
///////////////////////////////////////////////////

typedef struct ethash_h256 { uint8_t b[32]; } ethash_h256_t;

uint64_t ethash_get_datasize(uint64_t const block_number)
{
    // TODO: return this assert if needed: assert(block_number / ETHASH_EPOCH_LENGTH < 2048);
    return dag_sizes[block_number / ETHASH_EPOCH_LENGTH];
}

typedef struct ethash_return_value {
    ethash_h256_t result;
    ethash_h256_t mix_hash;
    bool success;
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
    SHA3_512(s_mix->bytes, s_mix->bytes, 40);

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
            node const* dag_node;
            node tmp_node;
            if (full_nodes) {
                dag_node = &full_nodes[MIX_NODES * index + n];
            } else {
                //ethash_calculate_dag_item(&tmp_node, index * MIX_NODES + n, light);
                //dag_node = &tmp_node;
                assert(0);
            }

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
    SHA3_256(&ret->result, s_mix->bytes, 64 + 32); // Keccak-256(s + compressed_mix)
    return true;
}




void verify_block_header(/* uint64_t blockno, std::string BLOCK_HEADER_RLP, std::string PARENT_HEADER_RLP */) {
    /*
    RLP blockRLP = RLP(new bytes(HexToBytes(BLOCK_HEADER_RLP)));
    RLP parentRLP = RLP(new bytes(HexToBytes(PARENT_HEADER_RLP)));
    BlockHeader header = BlockHeader();
    BlockHeader parentHeader = BlockHeader();
    header.populate(blockRLP);
    parentHeader.populate(parentRLP);

    // get header info
    Ethash ethash;
    h256 _headerHash = header.hash(WithoutSeal);
    h64 _nonce = ethash.nonce(header);

    // compute answer
    ethash_light_t light = compute_cache(blockno);
    */

    uint64_t nonce = 42;
    uint64_t block_number = 42;
    ethash_h256_t header_hash;
    std::vector<unsigned char> header_hash_bytes = HexToBytes("de1e91c286c6b05d827e7ac983d3fc566e6139bed9384c711625f9cf1d77749c");
    std::copy(header_hash_bytes.begin(), header_hash_bytes.end(), header_hash.b);
    printBytes(header_hash.b, 32);

    uint64_t full_size = ethash_get_datasize(block_number);
    ethash_return_value_t ret;
    node const full_nodes_arr[128] = {0};
    ethash_hash(&ret, full_nodes_arr, full_size, header_hash, nonce);

    //return ethash_light_compute_internal(light, full_size, header_hash, nonce);
/*
    ethash_return_value_t r = ethash_light_compute(light, *(ethash_h256_t*)_headerHash.data(), (uint64_t)(u64)_nonce);
    if (!r.success) {
        cout << "ETHASH HASHING FAILED" << endl;
        exit(3);
    }

    // TODO: verify answer is below difficulty

    cout << "RESULT: " << hexStr(r.result.b, 32) << endl;
    cout << "MIXHASH: " << hexStr(r.mix_hash.b, 32) << endl;
    cout << "EXPECTED MIXHASH: 5247691ab0953fa5c5c2c84b0b142b6d62e9dc5f35a865ed197b9cd3736af6f1" << endl;
*/
}

int main(int argc, char **argv) {
    cout << "Starting" << endl;
    verify_block_header();
    return 0;
}
