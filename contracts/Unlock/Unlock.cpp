#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/types.h>
#include <eosiolib/crypto.h>
#include <eosiolib/singleton.hpp>
typedef unsigned int uint;

#include <string.h>
#include <vector>

using byte = uint8_t;
using bytes = std::vector<byte>;

byte transfer_signature[] = {0xdd, 0xf2, 0x52, 0xad, 0x1b, 0xe2, 0xc8, 0x9b, 0x69, 0xc2, 0xb0, 0x68, 0xfc, 0x37, 0x8d, 0xaa, 0x95, 0x2b, 0xa7, 0xf1, 0x63, 0xc4, 0xa1, 0x16, 0x28, 0xf5, 0x5a, 0x4d, 0xf5, 0x23, 0xb3, 0xef};

using namespace eosio;
CONTRACT Unlock : public contract {

    public:
        using contract::contract;

        ACTION config(name token_contract,
                      symbol token_symbol,
                      name bridge_contract);

        ACTION unlock(const std::vector<unsigned char>& header_rlp_vec,
                      const std::vector<unsigned char>& encoded_path,
                      const std::vector<unsigned char>& rlp_receipt,
                      const std::vector<unsigned char>& all_parent_nodes_rlps,
                      const std::vector<uint>& all_parnet_rlp_sizes);

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
                      const std::vector<unsigned char>& encoded_path,
                      const std::vector<unsigned char>& rlp_receipt,
                      const std::vector<unsigned char>& all_parent_nodes_rlps,
                      const std::vector<uint>& all_parnet_rlp_sizes) {

    auto params_tuple = make_tuple(header_rlp_vec,
                                   encoded_path,
                                   rlp_receipt,
                                   all_parent_nodes_rlps,
                                   all_parnet_rlp_sizes);

    state_type state_inst(_self, _self.value);
    auto s = state_inst.get();

    action {permission_level{_self, "active"_n},
            s.bridge_contract,
            "checkreceipt"_n,
            params_tuple}.send();

    // 20 bytes
    uint8_t *address = get_address(rlp_receipt, 0);

    // each field size is 32
    uint8_t *function_signature = get_topic(rlp_receipt, 0, 0); // 1st topic
    uint8_t *from = get_topic(rlp_receipt, 0, 1); // 2nd topic
    uint8_t *to = get_topic(rlp_receipt, 0, 2); // 3rd topic
    uint8_t *amount = get_data(rlp_receipt, 0); // not indexed, so data field

    // parse data
    eosio_assert(memcmp(transfer_signature, function_signature, 20) == 0,
                 "function signature is not for knc transfer");

    //TODO - verify amount does not exceed 64/128 bit
    uint128_t *amount_uint = (uint128_t*)amount;
    // TODO - scale and send somewhere.
    // send eos tokens to someone
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
