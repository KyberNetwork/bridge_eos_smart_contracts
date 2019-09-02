//#include "../Common/common.hpp"
//#include "../Common/rlp.hpp"

#include "Bridge.hpp"

#define ANCHOR_SMALL_INTERVAL 50
#define ANCHOR_BIG_INTERVAL   1000

uint64_t get_tuple_key(uint64_t msg_sender, uint64_t anchor_block_num) {
    uint8_t input[16];
    memcpy(input, (uint8_t*)msg_sender, 8);
    memcpy(input + 8, (uint8_t*)anchor_block_num, 8);
    capi_checksum256 key_buffer = sha256(input, 16);
    uint64_t key = *((uint64_t *)key_buffer.hash);
    return key;
}

uint64_t round_up(uint64_t val, uint64_t denom) {
    return ((val + denom - 1) / denom) * denom;
}

uint64_t allocate_pointer(uint64_t header_hash) {
    return 0; // TODO - implement running pointer as first stage
}

// set genesis anchor!!! so can start..
void Bridge::setgenesis(uint64_t genesis_block_num,
                        uint64_t header_hash,
                        uint64_t difficulty) { // TODO: should difficulty be 0?

    // only authorized entity can set genesis
    require_auth(_self);

    // init state table
    newstate_type state_inst(_self, _self.value);

    eosio_assert(genesis_block_num % ANCHOR_BIG_INTERVAL == 0,
                 "bad genesis block num resolution");

    uint64_t current_pointer = allocate_pointer(header_hash);
    newstate initial_state = {
            current_pointer,   // last_issued_key
            difficulty,        // anchors_head_difficulty
            genesis_block_num, // anchors_head_block_num
            genesis_block_num  // genesis_block_num
    };
    state_inst.set(initial_state, _self);

    // init anchors table
    anchors_type anchors_inst(_self, _self.value);
    eosio_assert(anchors_inst.find(0) != anchors_inst.end(),
                 "scratchpad already for this anchor");

    anchors_inst.emplace(_self, [&](auto& s) {
        s.current = current_pointer;
        s.previous_small = 0;
        s.previous_large = 0;
        s.small_interval_list_hash = 0;
        s.header_hash = header_hash; // sha3(rlp{header}), for verifying previous hash
        s.total_difficulty = difficulty;
        s.block_num = genesis_block_num;
    });
}

void Bridge::initscratch(uint64_t msg_sender,
                         uint64_t anchor_block_num,
                         uint64_t previous_anchor_pointer) {

    // make sure anchor_block_num is in ANCHOR_SMALL_INTERVAL resolution
    eosio_assert(anchor_block_num % ANCHOR_SMALL_INTERVAL == 0, "bad anchor resolution");

    // get previous anchor
    anchors_type anchors_inst(_self, _self.value);
    auto previous_anchor_itr = anchors_inst.find(previous_anchor_pointer);
    eosio_assert(previous_anchor_itr != anchors_inst.end(),
                 "wrong previous anchor pointer");

    // assert previous anchor pointer points to the correct block num
    uint64_t previous_anchor_block_num = anchor_block_num - ANCHOR_SMALL_INTERVAL;
    eosio_assert(previous_anchor_itr->block_num == previous_anchor_block_num,
                 "wrong previous anchor block num");

    // make sure scratchpad is not allocated and allocate one
    uint64_t tuple_key = get_tuple_key(msg_sender, anchor_block_num);
    scratchdata_type scratch_inst(_self, _self.value);
    eosio_assert(scratch_inst.find(tuple_key) != scratch_inst.end(),
                 "scratchpad already for this anchor");

    scratch_inst.emplace(_self, [&](auto& s) {
        s.anchor_sender_hash = tuple_key;
        s.last_block_hash = previous_anchor_itr->header_hash;
        s.total_difficulty = previous_anchor_itr->total_difficulty;
        s.previous_anchor_pointer = previous_anchor_pointer;

        for( int i = 0; i < ANCHOR_SMALL_INTERVAL; i++) {
            s.small_interval_list.push_back(0);
        }
    });
}

void Bridge::storeheader(uint64_t msg_sender,
                         uint64_t block_num,
                         uint128_t difficulty,
                         uint64_t header_hash,
                         uint64_t previous_hash,
                         const vector<uint8_t>& header_rlp) {

    // load scratchpad data for the tuple
    uint64_t next_anchor = round_up(block_num, ANCHOR_SMALL_INTERVAL);
    uint64_t tuple_key = get_tuple_key(msg_sender, next_anchor);

    scratchdata_type scratch_inst(_self, _self.value);
    auto itr = scratch_inst.find(tuple_key);
    eosio_assert(itr != scratch_inst.end(), "scratchpad not initialized");

    // check new block is based on previous one
    eosio_assert(previous_hash == itr->last_block_hash, "wrong previous hash");

    // add difficulty to total_difficulty
    uint128_t new_total_difficulty = itr->total_difficulty + difficulty;

    // append sha256(rlp{y}) to scratchpad.list
    capi_checksum256 rlp_sha_buffer = sha256(header_rlp.data(), header_rlp.size());
    uint64_t rlp_sha = *((uint64_t *)rlp_sha_buffer.hash);

    // store in scratchpad
    scratch_inst.modify(itr, _self, [&](auto& s) {
        s.last_block_hash = header_hash;
        s.small_interval_list.push_back(rlp_sha);
        s.total_difficulty = new_total_difficulty;
    });
}

void Bridge::finalize(uint64_t msg_sender,
                      uint64_t anchor_block_num) {
    eosio_assert(anchor_block_num % ANCHOR_SMALL_INTERVAL == 0,
                 "wrong block number resolution");

    // load scratchpad (make sure it exists)
    scratchdata_type scratch_inst(_self, _self.value);
    uint64_t tuple_key = get_tuple_key(msg_sender, anchor_block_num);

    // get from scratchpad pointer to previous anchor
    auto scratch_itr = scratch_inst.find(tuple_key);
    eosio_assert(scratch_itr != scratch_inst.end(), "scratchpad not initialized");

    // load previous anchor in order to extend it
    anchors_type anchors_inst(_self, _self.value);
    auto previous_anchor_itr = anchors_inst.find(scratch_itr->previous_anchor_pointer);
    eosio_assert(previous_anchor_itr != anchors_inst.end(),
                 "internal error, wrong previous anchor pointer");

    // TODO: traverse back on anchor list and set previous_large
    // TODO: calculate small_interval_list_hash from list

    anchors_inst.emplace(_self, [&](auto& s) {
        s.current = allocate_pointer(scratch_itr->last_block_hash);
        s.previous_small = scratch_itr->previous_anchor_pointer;
        s.previous_large = 0; // TODO
        s.small_interval_list_hash = 0; // TODO
        s.header_hash = scratch_itr->last_block_hash;
        s.total_difficulty = scratch_itr->total_difficulty;
        s.block_num = anchor_block_num;
    });
}
