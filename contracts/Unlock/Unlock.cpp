#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/types.h>
#include <eosiolib/crypto.h>
#include <eosiolib/singleton.hpp>
typedef unsigned int uint;

#include <math.h>
#include <string.h>
#include <vector>

#include "../Common/sha3/sha3.hpp"
#include "../Common/Common.hpp"

using byte = uint8_t;
using bytes = std::vector<byte>;

byte transfer_signature[] = {0xdd, 0xf2, 0x52, 0xad, 0x1b, 0xe2, 0xc8, 0x9b, 0x69, 0xc2, 0xb0, 0x68, 0xfc, 0x37, 0x8d, 0xaa, 0x95, 0x2b, 0xa7, 0xf1, 0x63, 0xc4, 0xa1, 0x16, 0x28, 0xf5, 0x5a, 0x4d, 0xf5, 0x23, 0xb3, 0xef};
byte knc_address[] = {0xdd, 0x97, 0x4d, 0x5c, 0x2e, 0x29, 0x28, 0xde, 0xa5, 0xf7, 0x1b, 0x98, 0x25, 0xb8, 0xb6, 0x46, 0x68, 0x6b, 0xd2, 0x00};

// TODO - duplicated with Bridge contract, see if can moved to common place.
struct receipts {
    // TODO - only 8B out of the hash, in production add 32B to result and compare
    uint64_t receipt_header_hash;
    uint64_t primary_key() const { return receipt_header_hash; }
};
typedef eosio::multi_index<"receipts"_n, receipts> receipts_type;

using namespace eosio;
CONTRACT Unlock : public contract {

    public:
        using contract::contract;

        ACTION config(name token_contract,
                      symbol token_symbol,
                      name bridge_contract);

        ACTION unlock(const std::vector<unsigned char>& header_rlp_vec,
                      const std::vector<unsigned char>& rlp_receipt);

        TABLE state {
            name            token_contract;
            symbol          token_symbol;
            name            bridge_contract;
        };

        typedef eosio::singleton<"state"_n, state> state_type;

    private:

};

//////// tmp functions untill we have nested lists rlp decoding ///////
uint8_t* get_address(const std::vector<unsigned char> &value, uint event_num) {
    return (uint8_t*)&value[272];
}

uint8_t* get_topic(const std::vector<unsigned char> &value, uint event_num, uint topic_num) {
    if (topic_num == 0) {
        return (uint8_t*)&value[295];
    } else if (topic_num == 1) {
        return (uint8_t*)&value[328];
    } else if (topic_num == 2) {
        return (uint8_t*)&value[361];
    } else {
        return 0;
    }
}

uint8_t* get_data(const std::vector<unsigned char> &value, uint event_num) {
    return (uint8_t*)&value[394];
}
/////////

ACTION Unlock::config(name token_contract,
                      symbol token_symbol,
                      name bridge_contract) {
    require_auth(_self);
    state_type state_inst(_self, _self.value);

    state s = {token_contract, token_symbol, bridge_contract};
    state_inst.set(s, _self);
}

ACTION Unlock::unlock(const std::vector<unsigned char>& header_rlp_vec,
                      const std::vector<unsigned char>& rlp_receipt) {

    state_type state_inst(_self, _self.value);
    auto s = state_inst.get();

    // calculate sealed header hash
    uint8_t header_hash_arr[32];
    keccak256(header_hash_arr, (unsigned char *)header_rlp_vec.data(), header_rlp_vec.size());
    uint64_t header_hash = *((uint64_t*)header_hash_arr);

    // create vector from header_hash_arr
    std::vector<uint8_t> header_hash_vec(header_hash_arr, header_hash_arr + 32);

    // verify longest path
    action {permission_level{_self, "active"_n},
            s.bridge_contract,
            "veriflongest"_n,
            make_tuple(header_hash_vec)}.send();

    // get combined sha256 of (sha256(receipt rlp),header hash)
    // TODO: this is duplicated with Bridge.cpp, move to common place
    uint8_t rlp_receipt_hash[32];
    sha256(rlp_receipt_hash, (uint8_t *)&rlp_receipt[0], rlp_receipt.size());

    uint8_t combined_hash_input[64];
    memcpy(combined_hash_input, header_hash_arr, 32);
    memcpy(combined_hash_input + 32, rlp_receipt_hash, 32);

    uint8_t combined_hash_output[32];
    sha256(combined_hash_output, combined_hash_input, 64);
    uint64_t *receipt_header_hash = (uint64_t *)combined_hash_output;

    // make sure the reciept exists in bridge contract
    receipts_type receipt_table(s.bridge_contract, s.bridge_contract.value);
    eosio_assert(receipt_table.find(*receipt_header_hash) != receipt_table.end(),
                 "reciept not verified in bridge contract");

    // 20 bytes
    uint8_t *address = get_address(rlp_receipt, 0);
    eosio_assert(memcmp(knc_address, address, 20) == 0,
                 "adress is not for knc contract address");

    // each field size is 32
    uint8_t *function_signature = get_topic(rlp_receipt, 0, 0); // 1st topic
    uint8_t *from = get_topic(rlp_receipt, 0, 1); // 2nd topic
    uint8_t *to = get_topic(rlp_receipt, 0, 2); // 3rd topic
    uint8_t *amount = get_data(rlp_receipt, 0); // not indexed, so data field

    // parse data
    eosio_assert(memcmp(transfer_signature, function_signature, 32) == 0,
                 "function signature is not for knc transfer");

    //TODO - verify amount does not exceed 64/128 bit
    print_uint8_array(amount, 32);
    //uint128_t *amount_uint = (uint128_t*)(amount + 16);

    uint128_t t = 0;
    for(int i = 0; i < 16; i++){
        t = t << 8;
        t = t + (uint8_t)amount[16 + i];
    }
    print("t: ", t / pow(10, 18 - s.token_symbol.precision())); //TODO: make constant/configureable

    // TODO - scale and send somewhere.
    // send eos tokens to someone
);


}

extern "C" {

    void apply(uint64_t receiver, uint64_t code, uint64_t action) {

        if (code == receiver){
            switch( action ) {
                EOSIO_DISPATCH_HELPER( Unlock, (config)(unlock))
            }
        }
        eosio_exit(0);
    }
}
