#include "../Common/common.hpp"
#include "../Common/rlp.hpp"

using namespace eosio;

#define ANCHOR_SMALL_INTERVAL 50
#define ANCHOR_BIG_INTERVAL   1000

CONTRACT LongestChain : public contract {

    public:
        using contract::contract;

        ACTION initscratch(uint64_t msg_sender,
                           uint64_t anchor_block_num);

        ACTION storeheader(uint64_t msg_sender,
                           uint64_t block_num,
                           uint128_t difficulty,
                           uint64_t previous_hash,
                           const vector<uint8_t>& header_rlp);

        ACTION finalize(uint64_t msg_sender,
                        uint64_t anchor_block_num);

        ACTION veriflongest(const vector<uint8_t>& header_hash,
                            uint  block_num,
                            vector<uint64_t> small_interval_list);

        TABLE state {
            uint64_t   last_issued_key;
            uint128_t  anchors_head_difficulty;
            uint64_t   anchors_head_block_num;
            uint64_t   genesis_block_num;
        };

        // anchors are the headers that are maintained in permanent storage
        TABLE anchors {
            // related to data structure
            uint64_t current; // running index
            uint64_t previous_small;
            uint64_t previous_large;
            uint64_t small_interval_list_hash; // sha256([sha256(rlp{z - 1}), sha256(rlp{z - 2}), …, sha256(rlp{z - 50})])

            // related to verifying ethash
            uint64_t  header_hash; // sha3(rlp{header}), for verifying previous hash
            uint128_t total_difficulty;
            uint64_t  block_num;
            uint64_t primary_key() const { return current; }
        };

        TABLE scratchdata {
            // TODO - only 8B out of the hash, in production add 32B to result and compare
            uint64_t         anchor_sender_hash;
            uint64_t         previous_block_hash;
            uint128_t        total_difficulty;
            vector<uint64_t> small_interval_list;
            uint64_t primary_key() const { return anchor_sender_hash; }
        };

        typedef eosio::singleton<"state"_n, state> state_type;
        typedef eosio::multi_index<"anchors"_n, anchors> anchors_type;
        typedef eosio::multi_index<"scratchdata"_n, scratchdata> scratchdata_type;

    private:
};

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

// set genesis anchor!!! so can start..
ACTION LongestChain::setgenesis(uint64_t genesis_block_num,
                                uint64_t header_hash,
                                uint64_t difficulty) {

    // only authorized entity can set genesis
    require_auth(_self);

    // init state
    state_type state_inst(_self, _self.value);

    eosio_assert(genesis_block_num % ANCHOR_BIG_INTERVAL == 0,
                 "bad genesis block num resolution");

    state initial_state = {
            0,                 // last_issued_key
            0,                 // anchors_head_difficultyc
            genesis_block_num, // anchors_head_block_num
            genesis_block_num  // genesis_block_num
    };
    state_inst.set(initial_state, _self);

    // init anchors table
    anchors_type anchors_inst(_self, _self.value);
    eosio_assert(anchors_inst.find(0) != anchors_inst.end(),
                 "scratchpad already for this anchor");

    anchors_inst.emplace(_self, [&](auto& s) {
        s.current = 0; // running index
        s.previous_small = 0;
        s.previous_large = 0;
        s.small_interval_list_hash = 0;
        s.header_hash = header_hash; // sha3(rlp{header}), for verifying previous hash
        s.total_difficulty = difficulty;
        s.block_num = genesis_block_num;
    });
}

ACTION LongestChain::initscratch(uint64_t msg_sender,
                                 uint64_t anchor_block_num) {

    // make sure anchor_block_num is in ANCHOR_SMALL_INTERVAL resolution
    eosio_assert(anchor_block_num % ANCHOR_SMALL_INTERVAL == 0, "bad anchor resolution");

    // make sure previous_anchor = anchor_block_num - ANCHOR_SMALL_INTERVAL is in anchors table
    uint64_t previous_anchor_block_num = anchor_block_num - ANCHOR_SMALL_INTERVAL;
    // TODO - actually make sure

    // make sure scratchpad is not allocated and allocate one
    uint64_t key = get_tuple_key(msg_sender, anchor_block_num);
    scratchdata_type scratch_inst(_self, _self.value);
    eosio_assert(scratch_inst.find(key) != scratch_inst.end(),
                 "scratchpad already for this anchor");

    scratch_inst.emplace(_self, [&](auto& s) {
        s.anchor_sender_hash = key;
        s.previous_block_hash = 0; // TODO - set previous/last_block_hash to previous_anchor's sha3
        s.total_difficulty = 0; // TODO - get from previous anchor.

        for( int i = 0; i < ANCHOR_SMALL_INTERVAL; i++) {
            s.small_interval_list.push_back(0);
        }
    });
}

// this assumes header has been verified already
ACTION LongestChain::storeheader(uint64_t msg_sender,
                                 uint64_t block_num,
                                 uint128_t difficulty,
                                 uint64_t previous_hash,
                                 const vector<uint8_t>& header_rlp) {

    // load scratchpad data for the tuple
    // round up block num to multiple of small interval
    uint64_t next_anchor = round_up(block_num, ANCHOR_SMALL_INTERVAL)
    uint64_t key = get_tuple_key(msg_sender, anchor_block_num);
    scratchdata_type scratch_inst(_self, _self.value);
    auto itr = scratch_inst.find(key);
    eosio_assert(itr != scratch_inst.end(), "scratchpad not initialized");

    // TODO: verify header.previous_hash == scratchpad.previous_block_hash

    // add difficulty to total_difficulty
    uint new_total_difficulty = itr->total_difficulty + difficulty;

    // append sha256(rlp{y}) to scratchpad.list
    capi_checksum256 rlp_sha_buffer = sha256(header_rlp.data(), header_rlp.size());
    uint64_t rlp_sha = *((uint64_t *)rlp_sha.hash);

    eosio.modify(itr, _self, [&](auto& s) {
        s.previous_block_hash = 0; // TODO -  update to new hash
        s.small_interval_list.push(rlp_sha);
    }
}

ACTION LongestChain::finalize(uint64_t msg_sender,
                              uint64_t anchor_block_num) {
    eosio_assert(anchor_block_num % ANCHOR_SMALL_INTERVAL == 0,
                 "wrong block number resolution");

    // load scratchpad (make sure it exists)
    scratchdata_type scratch_inst(_self, _self.value);
    uint64_t key = get_tuple_key(msg_sender, anchor_block_num);

    // get from scratchpad pointer to previous anchor
    auto itr = scratch_inst.find(key);
    eosio_assert(itr != scratch_inst.end(), "scratchpad not initialized");

    // TODO: continue implementation...


    // traverse back on anchor list and set anchor for ANCHOR_BIG_INTERVAL
}

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        if (code == receiver){
            switch( action ) {
                EOSIO_DISPATCH_HELPER( LongestChain, (initscratch)(storeheader)(finalize)(setgenesis)/*(veriflongest)*/)
            }
        }
        eosio_exit(0);
    }
}
