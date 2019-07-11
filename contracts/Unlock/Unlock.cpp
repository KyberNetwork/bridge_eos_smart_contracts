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

byte lock_signature[] = {0xff, 0x0e, 0x8e, 0xf5, 0x8d, 0x4e, 0x45, 0x21, 0x66, 0xe4, 0x15, 0xcf, 0x3e, 0x96, 0xe0, 0x6c, 0xac, 0x94, 0x63, 0x62, 0xd8, 0xda, 0x31, 0x74, 0xea, 0xcf, 0xa8, 0xa3, 0x53, 0x59, 0xa5, 0x99};
byte lock_contract_address[] = {0x98, 0x03, 0x58, 0x36, 0x04, 0x09, 0xb1, 0xcc, 0x91, 0x3a, 0x91, 0x6b, 0xc0, 0xbf, 0x6f, 0x52, 0xf7, 0x75, 0x24, 0x2a};

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
    rlp_elem* event = get_n_elem(logs, 1); // second event

    rlp_elem* address = get_n_elem(event, 0);

    rlp_elem* topics = get_n_elem(event, 1);
    rlp_elem* function_signature = get_n_elem(topics, 0); // 1st topic
    rlp_elem *amount = get_n_elem(topics, 1); // 2nd topic
    rlp_elem *eos_recipient_name = get_n_elem(topics, 2); // 3rd topic
    rlp_elem *lock_id = get_n_elem(topics, 4); // 4th topic

    eosio_assert(memcmp(lock_contract_address, address->content, 20) == 0,
                 "adress is not for knc contract address");
    eosio_assert(memcmp(lock_signature, function_signature->content, 32) == 0,
                 "function signature is not for knc transfer");

    // fix endianess
    uint128_t amount_128 = 0;
    for(int i = 0; i < 16; i++){
        amount_128 = amount_128 << 8;
        amount_128 = amount_128 + (uint8_t)amount->content[16 + i];
    }

    uint64_t eos_address_64 = 0;
    for(int i = 0; i < 8; i++){
        eos_address_64 = eos_address_64 << 8;
        eos_address_64 = eos_address_64 + (uint8_t)eos_recipient_name->content[24 + i];
    }
    name recipient(eos_address_64);
    eosio_assert(is_account(recipient), "recipient account does not exist");
    print("recipient:", recipient);
    print("\n");

    //verify amount does not exceed 64 bit
    uint128_t amount_128_scaled =
        amount_128 / (uint128_t)(pow(10, 18 - s.token_symbol.precision()));
    eosio_assert(amount_128_scaled < asset::max_amount, "amount too big");
    uint64_t asset_amount = (uint64_t)amount_128_scaled;

    asset to_issue = asset(asset_amount, s.token_symbol);

    action {
        permission_level{_self, "active"_n},
        s.token_contract,
        "issue"_n,
        std::make_tuple(recipient, to_issue, string("issued tokens from waterloo"))
    }.send();

    print("done issuing of: ", to_issue);
    print("\n");

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
