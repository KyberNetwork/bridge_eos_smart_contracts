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
#include "../Common/NestedRlp.hpp"

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

    rlp_elem elem[100] = {0};
    rlp_elem* list = elem;
    rlp_elem* elem_ptr = elem + 1;
    uint num_elem, i;
    decode_list((uint8_t *)&rlp_receipt[0], list, &elem_ptr);

    rlp_elem* logs = get_n_elem(list, 3);
    rlp_elem* first_event = get_n_elem(logs, 0);

    rlp_elem* address = get_n_elem(first_event, 0);

    rlp_elem* topics = get_n_elem(first_event, 1);
    rlp_elem* function_signature = get_n_elem(topics, 0); // 1st topic
    rlp_elem *from = get_n_elem(topics, 1); // 2nd topic
    rlp_elem *to = get_n_elem(topics, 2); // 3rd topic

    // amount not indexed, so in data field
    rlp_elem *amount = get_n_elem(first_event, 2);

    //uint8_t *address = get_address(rlp_receipt, 0);
    eosio_assert(memcmp(knc_address, address->content, 20) == 0,
                 "adress is not for knc contract address");
    // parse data
    eosio_assert(memcmp(transfer_signature, function_signature->content, 32) == 0,
                 "function signature is not for knc transfer");

    // fix endianess
    uint128_t amount_128 = 0;
    for(int i = 0; i < 16; i++){
        amount_128 = amount_128 << 8;
        amount_128 = amount_128 + (uint8_t)amount->content[16 + i];
    }

    //verify amount does not exceed 64 bit
    uint128_t amount_128_scaled =
        amount_128 / (uint128_t)(pow(10, 18 - s.token_symbol.precision()));
    eosio_assert(amount_128_scaled < asset::max_amount, "amount too big");
    uint64_t asset_amount = (uint64_t)amount_128_scaled;

    // TODO - change according to recepient name in event
    asset to_pay = asset(asset_amount, s.token_symbol);
    async_pay(_self, "user"_n, to_pay, s.token_contract, "unlock");
    print("done parment of: ", to_pay);

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
