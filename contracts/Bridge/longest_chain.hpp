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
    uint64_t key = crop(key_buffer.hash);
    return key;
}

uint64_t round_up(uint64_t val, uint64_t denom) {
    return ((val + denom - 1) / denom) * denom;
}

uint64_t round_down(uint64_t val, uint64_t denom) {
    return (val / denom) * denom;
}

uint64_t Bridge::allocate_pointer(uint64_t header_hash) {
    newstate_type state_inst(_self, _self.value);
    auto s = state_inst.get();
    auto issued_pointer = ++s.last_issued_key;
    state_inst.set(s, _self);

    /* TODO - since on finalize() the user needs to input the anchor's pointer, need to:
        * Hold an on chain mapping of 8B of hash to list of indexes
        * Off chain client can traverse/check what is the right index.
    */

    return issued_pointer;
}

uint64_t sha256_of_list(const vector<uint64_t> &list) {
    capi_checksum256 sha_buffer = sha256((uint8_t *)(list.data()),
                                         sizeof(uint64_t) * list.size());
    return crop(sha_buffer.hash);;
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
            current_pointer, // anchors_head_pointer
            genesis_block_num  // genesis_block_num
    };
    state_inst.set(initial_state, _self);

    // init anchors table
    anchors_type anchors_inst(_self, _self.value);
    eosio_assert(anchors_inst.find(genesis_block_num) != anchors_inst.end(),
                 "anchor already exists");

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
                         uint64_t anchor_block_num, // anchor to connect to + ANCHOR_SMALL_INTERVAL
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
                 "scratchpad already exists for this anchor");

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

    // append sha256 of header rlp to scratchpad's list
    capi_checksum256 rlp_sha_buffer = sha256(header_rlp.data(), header_rlp.size());
    uint64_t rlp_sha = crop(rlp_sha_buffer.hash);

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

    // traverse back on anchor list and set previous_large
    uint blocks_to_traverse = ANCHOR_BIG_INTERVAL - ANCHOR_SMALL_INTERVAL; // already went backwards once
    if (anchor_block_num > blocks_to_traverse) {
        while (blocks_to_traverse > 0) {
            auto itr = anchors_inst.find(previous_anchor_itr->previous_small);
            auto previous_anchor_itr = itr;
            eosio_assert(previous_anchor_itr != anchors_inst.end(),
                         "internal error on traversing backwards");
            blocks_to_traverse -= ANCHOR_SMALL_INTERVAL;
        }
    }
    uint previous_large = previous_anchor_itr->current;

    // calculate small_interval_list_hash from list
    eosio_assert(scratch_itr->small_interval_list.size() == ANCHOR_SMALL_INTERVAL,
                 "wrong number of headers in scratchpad");
    uint64_t small_interval_list_hash = sha256_of_list(scratch_itr->small_interval_list);

    uint current_anchor_pointer = allocate_pointer(scratch_itr->last_block_hash);

    // store new anchor
    anchors_inst.emplace(_self, [&](auto& s) {
        s.current = current_anchor_pointer;
        s.previous_small = scratch_itr->previous_anchor_pointer;
        s.previous_large = previous_large;
        s.small_interval_list_hash = small_interval_list_hash;
        s.header_hash = scratch_itr->last_block_hash;
        s.total_difficulty = scratch_itr->total_difficulty;
        s.block_num = anchor_block_num;
    });

    // update pointer to list head if needed
    newstate_type state_inst(_self, _self.value);
    auto s = state_inst.get();
    if( scratch_itr->total_difficulty > s.anchors_head_difficulty){
        s.anchors_head_difficulty = scratch_itr->total_difficulty;
        s.anchors_head_block_num = anchor_block_num;
        s.anchors_head_pointer = current_anchor_pointer;
        state_inst.set(s, _self);
    }
}

void Bridge::veriflongest(uint64_t header_rlp_sha256,
                          uint  block_num,
                          vector<uint64_t> interval_list_proof) {

    //verify header_rlp_sha256 in list
    uint place_in_list = block_num % ANCHOR_SMALL_INTERVAL;
    eosio_assert(header_rlp_sha256 == interval_list_proof[block_num],
                 "given sha256 of header not in given proof in place deducted from block num");

    // start process of finding adjacent node that is a multiplication of small interval
    newstate_type state_inst(_self, _self.value);
    auto state = state_inst.get();
    uint64_t running_pointer = state.anchors_head_pointer;
    uint64_t running_block_num = state.anchors_head_block_num;

    // traverse to closest larger anchor in large interval resolution
    anchors_type anchors_inst(_self, _self.value);
    auto itr = anchors_inst.find(running_pointer);
    while (running_block_num > round_up(block_num, ANCHOR_BIG_INTERVAL)) {
        auto previous_anchor_itr = anchors_inst.find(itr->previous_large);
        auto itr = previous_anchor_itr; // to avoid compilation error
        running_pointer = itr->previous_large;
    }

    // traverse to closest smallest anchor in small interval resolution
    itr = anchors_inst.find(running_pointer);
    while (running_block_num > round_down(block_num, ANCHOR_SMALL_INTERVAL)) {
        auto previous_anchor_itr = anchors_inst.find(itr->previous_small);
        auto itr = previous_anchor_itr; // to avoid compilation error
        running_pointer = itr->previous_small;
    }

    itr = anchors_inst.find(running_pointer);

    // verify stored sha256(list) is equal to given sha256 list proof
    uint64_t list_proof_sha256 = sha256_of_list(interval_list_proof);
    eosio_assert(list_proof_sha256 == itr->small_interval_list_hash,
                 "given list does not hash to stored hash value");

}
