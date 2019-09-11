//#include "../Common/common.hpp"
//#include "../Common/rlp.hpp"

#include "Bridge.hpp"

#define ANCHOR_SMALL_INTERVAL 5
#define ANCHOR_BIG_INTERVAL   10
#define INVALID (1LL << 62) - 1

uint64_t sha_and_crop(const uint8_t *input, uint size) {
    capi_checksum256 sha_csum = sha256(input, size);
    return crop(sha_csum.hash);
}

uint64_t sha256_of_list(const vector<uint64_t> &list) {
    return sha_and_crop((uint8_t *)(list.data()), sizeof(uint64_t) * list.size());
}

uint64_t get_tuple_key(uint64_t msg_sender, uint64_t anchor_block_num) {
    uint8_t input[16];

    // TODO - for some reason this doesn't work - memcpy(input, (uint8_t *)anchor_block_num, 8);
    for (uint i = 0; i < 8; i++) {
        input[i] = (msg_sender >> (i * 8)) & 0xFF;
    }
    for (uint i = 0; i < 8; i++) {
        input[i + 8] = (anchor_block_num >> (i * 8)) & 0xFF;
    }

    return sha_and_crop(input, 16);
}

uint64_t round_up(uint64_t val, uint64_t denom) {
    return ((val + denom - 1) / denom) * denom;
}

uint64_t round_down(uint64_t val, uint64_t denom) {
    return (val / denom) * denom;
}

uint64_t parseBuff(uint8_t buff[]) {
    uint64_t result = 0;
    for(int i = 0; i < 8; i++){
        result |= (((uint64_t)buff[i]) << (56 - i * 8));
    }
    return result;
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


void Bridge::setgenesis(uint64_t genesis_block_num,
                        uint64_t header_hash,
                        uint64_t difficulty) { // TODO: should difficulty be 0?
    print("setgenesis with genesis block num ", genesis_block_num);

    require_auth(_self); // only authorized entity can set genesis
    eosio_assert(genesis_block_num % ANCHOR_BIG_INTERVAL == 1, "bad genesis block resolution");

    newstate_type state_inst(_self, _self.value);
    uint64_t current_pointer = 0; // can not allocate if state is not initialized
    newstate initial_state = {
            current_pointer,   // last_issued_key
            difficulty,        // anchors_head_difficulty
            genesis_block_num, // anchors_head_block_num
            current_pointer,   // anchors_head_pointer
            genesis_block_num  // genesis_block_num
    };
    state_inst.set(initial_state, _self);

    // init anchors table
    anchors_type anchors_inst(_self, _self.value);
    eosio_assert(anchors_inst.find(genesis_block_num) == anchors_inst.end(), "anchor exists");

    anchors_inst.emplace(_self, [&](auto& s) {
        s.current = current_pointer;
        s.previous_small = INVALID;
        s.previous_large = INVALID;
        s.list_hash = 0;
        s.header_hash = header_hash; // sha3(rlp{header}), for verifying previous hash
        s.total_difficulty = difficulty;
        s.block_num = genesis_block_num - 1;
    });
}

void Bridge::initscratch(uint64_t msg_sender,
                         uint64_t anchor_block_num, // anchor to connect to + ANCHOR_SMALL_INTERVAL
                         uint64_t previous_anchor_pointer) {
    print("initscratch for anchor block num ", anchor_block_num);

    // make sure anchor_block_num is in small interval resolution
    eosio_assert(anchor_block_num % ANCHOR_SMALL_INTERVAL == 0, "bad anchor resolution");

    // get previous anchor
    anchors_type anchors_inst(_self, _self.value);
    auto itr = anchors_inst.find(previous_anchor_pointer);
    eosio_assert(itr != anchors_inst.end(), "wrong previous anchor pointer");

    // assert previous anchor pointer points to the correct block num
    uint64_t previous_anchor_block_num = anchor_block_num - ANCHOR_SMALL_INTERVAL;
    eosio_assert(itr->block_num == previous_anchor_block_num, "wrong previous anchor block num");

    // make sure scratchpad is not allocated and allocate one
    uint64_t tuple_key = get_tuple_key(msg_sender, anchor_block_num);
    scratchdata_type scratch_inst(_self, _self.value);
    eosio_assert(scratch_inst.find(tuple_key) == scratch_inst.end(), "scratchpad exists");

    scratch_inst.emplace(_self, [&](auto& s) {
        s.anchor_sender_hash = tuple_key;
        s.last_block_hash = itr->header_hash;
        s.total_difficulty = itr->total_difficulty;
        s.previous_anchor_pointer = previous_anchor_pointer;
    });
}

void Bridge::storeheader(uint64_t msg_sender,
                         uint64_t block_num,
                         uint128_t difficulty,
                         uint64_t header_hash,
                         uint64_t previous_hash,
                         const vector<uint8_t>& header_rlp) {
    print("storeheader for block num ", block_num);

    // load scratchpad data for the tuple
    uint64_t next_anchor = round_up(block_num, ANCHOR_SMALL_INTERVAL);
    uint64_t tuple_key = get_tuple_key(msg_sender, next_anchor);
    scratchdata_type scratch_inst(_self, _self.value);
    auto itr = scratch_inst.find(tuple_key);
    eosio_assert(itr != scratch_inst.end(), "scratchpad not initialized");

    // check new block is based on previous one
    newstate_type state_inst(_self, _self.value);
    if (block_num != state_inst.get().genesis_block_num) {
        eosio_assert(previous_hash == itr->last_block_hash, "wrong previous hash");
    }

    // add difficulty
    uint128_t new_total_difficulty = itr->total_difficulty + difficulty;

    // append sha256 of header rlp to scratchpad's list
    uint64_t rlp_sha = sha_and_crop(header_rlp.data(), header_rlp.size());

    // store in scratchpad
    scratch_inst.modify(itr, _self, [&](auto& s) {
        s.last_block_hash = header_hash;
        s.small_interval_list.push_back(rlp_sha);
        s.total_difficulty = new_total_difficulty;
    });
}

void Bridge::finalize(uint64_t msg_sender,
                      uint64_t anchor_block_num) {
    print("finalize anchor block num ", anchor_block_num);

    eosio_assert(anchor_block_num % ANCHOR_SMALL_INTERVAL == 0, "wrong anchor resolution");

    // load scratchpad
    scratchdata_type scratch_inst(_self, _self.value);
    uint64_t tuple_key = get_tuple_key(msg_sender, anchor_block_num);
    auto scratch_itr = scratch_inst.find(tuple_key);
    eosio_assert(scratch_itr != scratch_inst.end(), "scratchpad not initialized");

    // calculate small_interval list hash
    eosio_assert(scratch_itr->small_interval_list.size() == ANCHOR_SMALL_INTERVAL, "wrong list size");
    uint64_t list_hash = sha256_of_list(scratch_itr->small_interval_list);

    // traverse back on anchor list and set previous_large
    uint blocks_to_traverse = anchor_block_num % ANCHOR_BIG_INTERVAL;
    if (blocks_to_traverse == 0) blocks_to_traverse = ANCHOR_BIG_INTERVAL;

    // first traverse back to the last anchor
    anchors_type anchors_inst(_self, _self.value);
    auto traverse_itr = anchors_inst.find(scratch_itr->previous_anchor_pointer);
    blocks_to_traverse -= ANCHOR_SMALL_INTERVAL;

    // continue traversing to last anchor that is in big interval resolution
    while (blocks_to_traverse) {
        auto tmp_itr = anchors_inst.find(traverse_itr->previous_small);
        traverse_itr = tmp_itr; // to avoid compilation error
        blocks_to_traverse -= ANCHOR_SMALL_INTERVAL;
    }

    // TODO - remove tmp code:
    /*
    if (anchor_block_num == 8123010) {
        for(int i = 0; i < scratch_itr->small_interval_list.size(); i++) {
            print(i);
            print(":");
            print(scratch_itr->small_interval_list[i]);
            print("\n");
        }
        print("list_hash: ", list_hash);
    }
    */

    // store new anchor
    uint current_anchor_pointer = allocate_pointer(scratch_itr->last_block_hash);
    print("current_anchor_pointer ", current_anchor_pointer);
    anchors_inst.emplace(_self, [&](auto& s) {
        s.current = current_anchor_pointer;
        s.previous_small = scratch_itr->previous_anchor_pointer;
        s.previous_large = traverse_itr->current;
        s.list_hash = list_hash;
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

    // clean up scratchpad
    scratch_inst.erase(scratch_itr);
}

// NOTE: byte vectors are used instead of uint64_t since actually not all 2^64 range is supported.
ACTION Bridge::veriflongest(vector<uint8_t>& header_rlp_sha256,
                            uint64_t block_num,
                            vector<uint8_t>& interval_list_proof) {
    print("veriflongest for block num ", block_num);

    // TODO: remove the need for parsing buffer on-chain by arranging input conveniently for the contract.
    auto header_sha256 = parseBuff(header_rlp_sha256.data());

    // TODO: remove here as well, this is very cpu consuming
    vector<uint64_t> proof(ANCHOR_SMALL_INTERVAL);
    for(int k = 0; k < ANCHOR_SMALL_INTERVAL; k++) {
        proof[k] = parseBuff((uint8_t *)(&interval_list_proof[k * 8]));
    }

    // verify header_rlp_sha256 in list
    uint index = (block_num - 1) % ANCHOR_SMALL_INTERVAL;
    eosio_assert(header_sha256 == proof[index], "header sha256 not found");

    // start process of finding adjacent node that is a multiplication of small interval
    newstate_type state_inst(_self, _self.value);
    auto state = state_inst.get();
    uint64_t running_pointer = state.anchors_head_pointer;
    uint64_t running_block_num = state.anchors_head_block_num;

    // traverse to closest larger anchor in large interval resolution
    anchors_type anchors_inst(_self, _self.value);
    auto itr = anchors_inst.find(running_pointer);

    while (running_block_num > round_up(block_num, ANCHOR_BIG_INTERVAL)) {
        auto tmp_itr = anchors_inst.find(itr->previous_large);
        itr = tmp_itr; // to avoid compilation error
        running_pointer = itr->previous_large;
        running_block_num = itr->block_num;
    }

    // traverse to closest larger anchor in small interval resolution
    while (running_block_num > round_up(block_num, ANCHOR_SMALL_INTERVAL)) {
        auto tmp_itr = anchors_inst.find(itr->previous_small);
        itr = tmp_itr; // to avoid compilation error
        running_pointer = itr->previous_small;
        running_block_num = itr->block_num;
    }

    // verify stored sha256(list) is equal to given sha256 list proof
    uint64_t list_proof_sha256 = sha256_of_list(proof);
    eosio_assert(list_proof_sha256 == itr->list_hash, "given list hash diff with stored list hash");

}
