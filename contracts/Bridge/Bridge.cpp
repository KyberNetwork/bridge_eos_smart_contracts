#include "Bridge.hpp"

#include "../Common/common.hpp"
#include "../Common/rlp.hpp"
#include "dag_merkle.hpp"
#include "dag_sizes.h"
#include "long_mult.hpp"
#include "patricia_merkle.hpp"
#include "longest_chain.hpp"

#define ETHASH_EPOCH_LENGTH 30000U
#define ETHASH_MIX_BYTES 128
#define ETHASH_ACCESSES 64

#define NODE_BYTES 64
#define NODE_WORDS (64/4)
#define MIX_WORDS (ETHASH_MIX_BYTES/4)
#define MIX_NODES (MIX_WORDS / NODE_WORDS)

#define FNV_PRIME 0x01000193

using namespace eosio;

typedef union node {
    uint8_t bytes[NODE_WORDS * 4];
    uint32_t words[NODE_WORDS];
    uint64_t double_words[NODE_WORDS / 2];
} node;

struct header_info_struct {
    uint64_t nonce;
    uint64_t block_num;
    uint8_t *previous_hash;
    uint8_t *expected_root;
    uint8_t *difficulty;
    uint difficulty_len;
    uint8_t *receipt_root;
    capi_checksum256 unsealed_header_hash;
    capi_checksum256 header_hash;
};

static uint64_t* dag_sizes_array[] = {dag_sizes_0, dag_sizes_1, dag_sizes_2,
                                      dag_sizes_3, dag_sizes_4};



uint32_t fnv_hash(uint32_t const x, uint32_t const y)
{
    return x * FNV_PRIME ^ y;
}

uint64_t ethash_get_datasize(uint64_t const block_num)
{
    eosio_assert(block_num / ETHASH_EPOCH_LENGTH < EPOCH_ELEMENTS, "block number too big");
    uint index = block_num / ETHASH_EPOCH_LENGTH;
    uint array_num = index / EPOCH_SINGLE_ARRAY_SIZE;
    uint index_in_array = index % EPOCH_SINGLE_ARRAY_SIZE;

    return dag_sizes_array[array_num][index_in_array];
}

void verify_header(struct header_info_struct* header_info,
                   const bytes& dags,
                   const bytes& proofs,
                   uint proof_length) {

    uint8_t *header_hash = header_info->unsealed_header_hash.hash;
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
    s_mix[0].double_words[4] = nonce;

    // compute sha3-512 hash and replicate across mix

    uint8_t res[64];
    keccak512(res, s_mix->bytes, 40);
    memcpy(s_mix->bytes, res, 64);

    node* const mix = s_mix + 1;
    for (uint32_t w = 0; w != MIX_WORDS; ++w) {
        mix->words[w] = s_mix[0].words[w % NODE_WORDS];
    }

    unsigned const page_size = sizeof(uint32_t) * MIX_WORDS;
    unsigned const num_full_pages = (unsigned) (full_size / page_size);

    for (unsigned i = 0; i != ETHASH_ACCESSES; ++i) {

        uint32_t const index = fnv_hash(s_mix->words[0] ^ i, mix->words[i % MIX_WORDS]) % num_full_pages;

        // dag elements verification
        uint8_t res[MERKLE_ELEMENT_LEN];
        uint8_t *current_item = (uint8_t *)(&dags[i * NODE_BYTES * 2]);
        uint8_t *proofs_start = (uint8_t *)(&proofs[i * proof_length * MERKLE_ELEMENT_LEN]);

        merkle_apply_path(index, res, current_item, proofs_start, proof_length);
        eosio_assert(0 == memcmp(res, expected_root, MERKLE_ELEMENT_LEN),
                     "dag elements merkle verification failure");

        for (unsigned n = 0; n != MIX_NODES; ++n) {

            uint32_t *current_item_word = (uint32_t *)(&dags[NODE_BYTES * (i * 2 + n)]);
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

    // final Keccak hash
    capi_checksum256 ethash= keccak256(s_mix->bytes, 64 + 32);

    print("ethash: ");
    print_uint8_array(ethash.hash, 32);

    print("mixed_hash: ");
    print_uint8_array(mix->bytes, 32);

    //check ethash result meets the required difficulty
    eosio_assert(check_pow(difficulty, difficulty_len, ethash.hash), "pow difficulty failure");

    return;
}

void hash_header_rlp(struct header_info_struct* header_info,
                     const bytes& header_rlp,
                     rlp_item* items) {

    // calculate sealed header hash
    header_info->header_hash = keccak256(header_rlp.data(), header_rlp.size());

    // calculate unsealed header hash (w/o nonce and mixed fields).
    int trim_len = remove_last_field_from_rlp((uint8_t *)header_rlp.data(),
                                              items[NONCE_FIELD].len);
    eosio_assert(trim_len == (header_rlp.size() - 9), "wrong 1st trim length");

    trim_len = remove_last_field_from_rlp((uint8_t *)header_rlp.data(),
                                          items[MIX_HASH_FIELD].len);
    eosio_assert(trim_len == (header_rlp.size() - 42), "wrong 2nd trim length");

    header_info->unsealed_header_hash = keccak256(header_rlp.data(), trim_len);
}

void Bridge::parse_header(struct header_info_struct* header_info, const bytes& header_rlp) {
    rlp_item items[15];
    uint num_items;
    decode_list((uint8_t *)header_rlp.data(), items, &num_items);

    header_info->nonce = get_uint64(&items[NONCE_FIELD]);
    header_info->block_num = get_uint64(&items[NUMBER_FIELD]);
    header_info->difficulty = items[DIFFICULTY_FIELD].content;
    header_info->difficulty_len = items[DIFFICULTY_FIELD].len;
    header_info->previous_hash = items[PARENT_HASH_FIELD].content;
    header_info->receipt_root = items[RECIEPT_ROOT_FIELD].content;

    hash_header_rlp(header_info, header_rlp, items);

    // get pre-stored root
    roots_type roots_inst(_self, _self.value);
    auto root_entry = roots_inst.get(header_info->block_num / 30000U, "dag root not pre-stored");
    header_info->expected_root = root_entry.root.data();
}

void Bridge::store_header(struct header_info_struct* header_info,
                          const vector<uint8_t>& header_rlp) {
    state_type state_inst(_self, _self.value);
    headers_type headers_inst(_self, _self.value);

    uint64_t header_hash = crop(header_info->header_hash.hash);
    uint64_t previous_hash = crop(header_info->previous_hash);
    uint64_t block_num = header_info->block_num;

    uint64_t msg_sender = 0; // TODO - actually get message sender and authenticate it

    uint128_t difficulty_value = decode_number128(header_info->difficulty, header_info->difficulty_len);
    storeheader(msg_sender,
                block_num,
                difficulty_value,
                header_hash,
                previous_hash,
                header_rlp);

/* old implementation:
    // calculate total difficulty
    uint128_t previous_total_difficulty;

    if (block_num == state_inst.get().genesis_block_num) {
        // genesis can only be input by the contract's owner
        require_auth(_self);
        previous_total_difficulty = 0;
    } else {
        auto prev_itr = headers_inst.find(previous_hash);
        bool prev_exists = (prev_itr != headers_inst.end());
        eosio_assert(prev_exists, "previous header hash does not exist");
        previous_total_difficulty = prev_itr->total_difficulty;
    }
    uint128_t difficulty_value = decode_number128(header_info->difficulty, header_info->difficulty_len);
    uint128_t total_difficulty = previous_total_difficulty + difficulty_value;

    // update pointer to list head if needed
    auto s = state_inst.get();
    if( total_difficulty > s.headers_head_difficulty){
        s.headers_head = header_hash;
        s.headers_head_difficulty = total_difficulty;
        s.headers_head_block_num = block_num;
        state_inst.set(s, _self);
    }

      // store in table
    auto itr = headers_inst.find(header_hash);
    bool header_exists = (itr != headers_inst.end());
    if (!header_exists) {
        headers_inst.emplace(_self, [&](auto& s) {
            s.header_hash = header_hash;
            s.previous_hash = previous_hash;
            s.total_difficulty = total_difficulty;
            s.block_num = block_num;
        });
    } else {
        headers_inst.modify(itr, _self, [&](auto& s) {
            s.header_hash = header_hash;
            s.previous_hash = previous_hash;
            s.total_difficulty = total_difficulty;
            s.block_num = block_num;
        });
    }
 */
}

ACTION Bridge::veriflongest(const bytes& header_hash) {
    state_type state_inst(_self, _self.value);
    headers_type headers_inst(_self, _self.value);

    uint64_t header_hash_key = crop(header_hash.data());

    // go backwards from list head and find given header
    auto s = state_inst.get();
    if (header_hash_key == s.headers_head) return; // action is in head - return successfully

    // get previous hash
    uint64_t current_hash = s.headers_head;
    while (true) {
        auto s = headers_inst.get(current_hash, "header is not on longest path");
        if (s.previous_hash == header_hash_key) {
            print("found in block before"); print(s.block_num);
            return; // found - return successfully
        }
        current_hash = s.previous_hash;
    }

    eosio_assert(false, "not on longest path");

}

ACTION Bridge::storeroots(uint64_t genesis_block_num,
                          const vector<uint64_t>& epoch_nums,
                          const bytes& dag_roots){

    require_auth(_self);
    state_type state_inst(_self, _self.value);
    state initial_state = {0, 0, 0, genesis_block_num};
    state_inst.set(initial_state, _self);

    eosio_assert(epoch_nums.size() * MERKLE_ELEMENT_LEN == dag_roots.size(),
                 "epochs and roots vectors mismatch");
    roots_type roots_inst(_self, _self.value);

    for(uint i = 0; i < epoch_nums.size(); i++){
        auto itr = roots_inst.find(epoch_nums[i]);
        bool exists = (itr != roots_inst.end());
        if (!exists) {
            roots_inst.emplace(_self, [&](auto& s) {
                s.epoch_num = epoch_nums[i];

                // TODO: improve pushing to use memcpy or std::cpy
                for(uint j = 0; j < MERKLE_ELEMENT_LEN; j++ ){
                    s.root.push_back(dag_roots[i * MERKLE_ELEMENT_LEN + j]);
                }
            });
        } else {
            roots_inst.modify(itr, _self, [&](auto& s) {
                s.epoch_num = epoch_nums[i];

                // TODO: improve pushing to use memcpy or std::cpy
                for(uint j = 0; j < MERKLE_ELEMENT_LEN; j++ ){
                    s.root.push_back(dag_roots[i * MERKLE_ELEMENT_LEN + j]);
                }
            });
        }
    }
};

ACTION Bridge::relay(const bytes& header_rlp,
                     const bytes& dags,
                     const bytes& proofs,
                     uint proof_length) {

    struct header_info_struct header_info;
    parse_header(&header_info, header_rlp);
    verify_header(&header_info, dags, proofs, proof_length);
    store_header(&header_info, header_rlp);

    return;
}

ACTION Bridge::checkreceipt(const bytes& header_rlp,
                            const bytes& encoded_path,
                            const bytes& receipt_rlp,
                            const bytes& all_parent_nodes_rlps,
                            const vector<uint>& all_parnet_rlp_sizes) {
    struct header_info_struct header_info;
    parse_header(&header_info, header_rlp);

    eosio_assert(trieValue(encoded_path,
                           receipt_rlp,
                           all_parent_nodes_rlps,
                           all_parnet_rlp_sizes,
                           header_info.receipt_root),
                 "failed receipt patricia trie verification"
    );

    uint64_t receipt_header_hash = get_reciept_header_hash(receipt_rlp, header_info.header_hash);

    receipts_type receipts_inst(_self, _self.value);
    auto itr = receipts_inst.find(receipt_header_hash);
    bool exists = (itr != receipts_inst.end());
    if (!exists) {
        receipts_inst.emplace(_self, [&](auto& s) {
            s.receipt_header_hash = receipt_header_hash;
        });
    } else {
        receipts_inst.modify(itr, _self, [&](auto& s) {
            s.receipt_header_hash = receipt_header_hash;
        });
    }

    return;
}

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        if (code == receiver){
            switch( action ) {
                EOSIO_DISPATCH_HELPER( Bridge, (relay)(checkreceipt)(storeroots))
            }
        }
        eosio_exit(0);
    }
}
