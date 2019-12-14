#include <math.h>

#include "../Common/common.hpp"
#include "../Common/rlp/nested_rlp.hpp"

// signature for "Lock(uint256,uint64,uint256)"
const bytes lock_signature{0xff, 0x0e, 0x8e, 0xf5, 0x8d, 0x4e, 0x45, 0x21, 0x66, 0xe4,
                           0x15, 0xcf, 0x3e, 0x96, 0xe0, 0x6c, 0xac, 0x94, 0x63, 0x62,
                           0xd8, 0xda, 0x31, 0x74, 0xea, 0xcf, 0xa8, 0xa3, 0x53, 0x59,
                           0xa5, 0x99};

const bytes lock_contract_address{0x98, 0x03, 0x58, 0x36, 0x04, 0x09, 0xb1, 0xcc, 0x91,
                                  0x3a, 0x91, 0x6b, 0xc0, 0xbf, 0x6f, 0x52, 0xf7, 0x75,
                                  0x24, 0x2a};

// TODO - tables duplicated with Bridge contract, see if can moved to common place.
struct receipts {
    // TODO - only 8B out of the hash, in production add 32B to result and compare
    uint64_t receipt_header_hash;
    uint64_t primary_key() const { return receipt_header_hash; }
};

TABLE onlongest {
    // TODO - only 8B out of the hash, in production add 32B to result and compare
    uint64_t header_sha256;
    uint64_t accumulated_work;
    // TODO - add sender so can support deleting of entries
    uint64_t primary_key() const { return header_sha256; }
};

typedef eosio::multi_index<"receipts"_n, receipts> receipts_type;
typedef eosio::multi_index<"onlongest"_n, onlongest> onlongest_type;

CONTRACT Issue : public contract {

    public:
        using contract::contract;

        ACTION config(name token_contract, symbol token_symbol, name bridge_contract);
        ACTION issue(const vector<uint8_t>& header_rlp,
                     const vector<uint8_t>& receipt_rlp,
                     uint event_num_in_tx);

        TABLE state {
            name   token_contract;
            symbol token_symbol;
            name   bridge_contract;
        };

        TABLE lockid {
            uint64_t lock_id;
            uint64_t primary_key() const { return lock_id; }
        };

        typedef eosio::singleton<"state"_n, state> state_type;
        typedef eosio::multi_index<"lockid"_n, lockid> lockid_type;

    private:

};

void parse_reciept(name *recipient,
                   asset *to_issue,
                   uint64_t *lock,
                   const bytes &receipt_rlp,
                   symbol token_symbol,
                   uint event_num_in_tx) {
    rlp_elem elem[100] = {0};
    rlp_elem* list = elem;
    rlp_elem* elem_ptr = elem + 1;
    uint num_elem, i;
    decode_list((uint8_t *)receipt_rlp.data(), list, &elem_ptr);

    rlp_elem* logs = get_n_elem(list, 3);
    rlp_elem* event = get_n_elem(logs, event_num_in_tx);

    rlp_elem* address = get_n_elem(event, 0);
    rlp_elem* topics = get_n_elem(event, 1);
    rlp_elem* function_signature = get_n_elem(topics, 0);   // 1st topic
    rlp_elem *amount = get_n_elem(topics, 1);               // 2nd topic
    rlp_elem *eos_recipient_name = get_n_elem(topics, 2);   // 3rd topic
    rlp_elem *lock_id = get_n_elem(topics, 4);              // 4th topic

    eosio_assert(memcmp(&lock_contract_address[0], address->content, 20) == 0,
                 "adress is not for knc contract address");
    eosio_assert(memcmp(&lock_signature[0], function_signature->content, 32) == 0,
                 "function signature is not for knc transfer");

    // further parse amount
    uint128_t amount_128 = 0;
    for (int i = 0; i < 16; i++){
        amount_128 = (amount_128 << 8) + (uint8_t)amount->content[16 + i];
    }
    uint128_t amount_128_scaled =
        amount_128 / (uint128_t)(pow(10, 18 - token_symbol.precision()));
    eosio_assert(amount_128_scaled < asset::max_amount, "amount too big");
    *to_issue = asset((uint64_t)amount_128_scaled, token_symbol);

    // further parse recipient address
    uint64_t eos_address_64 = 0;
    for (int i = 0; i < 8; i++){
        eos_address_64 = (eos_address_64 << 8) + (uint8_t)eos_recipient_name->content[24 + i];
    }
    *recipient = name(eos_address_64);
    eosio_assert(is_account(*recipient), "recipient account does not exist");
    print("recipient:", *recipient);
    print("\n");

    // further parse lock id
    uint64_t lock_id_64 = 0;
    for (int i = 0; i < 8; i++){
        lock_id_64 = (lock_id_64 << 8) + (uint8_t)lock_id->content[24 + i];
    }
    *lock = lock_id_64;
}

ACTION Issue::config(name token_contract, symbol token_symbol, name bridge_contract) {
    require_auth(_self);
    state_type state_inst(_self, _self.value);

    state s = {token_contract, token_symbol, bridge_contract};
    state_inst.set(s, _self);
}

// TODO: duplicated from longest chain - make shared in common
uint64_t sha_and_crop(const uint8_t *input, uint size) {
    capi_checksum256 sha_csum = sha256(input, size);

    uint64_t res = 0;
    for(int i = 0; i < 8; i++){
        res |= ((uint64_t)(sha_csum.hash[i]) << (i * 8));
    }
    return res;
}

ACTION Issue::issue(const bytes& header_rlp,
                    const bytes& receipt_rlp,
                    uint event_num_in_tx) {
    state_type state_inst(_self, _self.value);
    auto s = state_inst.get();

    // verify on longest chain
    onlongest_type onlongest_table(s.bridge_contract, s.bridge_contract.value);
    uint64_t header_sha256 = sha_and_crop(header_rlp.data(), header_rlp.size());
    auto itr = onlongest_table.find(header_sha256);
    eosio_assert(itr != onlongest_table.end(),
                 "header not verified on longest path in bridge contract");

    // verify min work amount was done
    //// TODO - move to input.
    uint128_t min_accumulated_work_1k_res = 100;
    ///////////////
    eosio_assert(itr->accumulated_work >= min_accumulated_work_1k_res * 1000,
                 "min accumulated work not reached");

    // calculate sealed header hash
    capi_checksum256 header_hash = keccak256(header_rlp.data(), header_rlp.size());

    // make sure the reciept exists in bridge contract
    uint64_t receipt_header_hash = get_reciept_header_hash(receipt_rlp, header_hash);
    receipts_type receipt_table(s.bridge_contract, s.bridge_contract.value);
    eosio_assert(receipt_table.find(receipt_header_hash) != receipt_table.end(),
                 "reciept not verified in bridge contract");

    // get actual receipt values
    name recipient;
    asset to_issue;
    uint64_t lock_id;
    parse_reciept(&recipient, &to_issue, &lock_id, receipt_rlp, s.token_symbol, event_num_in_tx);

    // store lock id
    lockid_type lockid_inst(_self, _self.value);
    bool exists = (lockid_inst.find(lock_id) != lockid_inst.end());
    eosio_assert(!exists, "current lock receipt was already processed");
    lockid_inst.emplace(_self, [&](auto& s) {
        s.lock_id = lock_id;
    });

    // first issue to self
    action {
        permission_level{_self, "active"_n},
        s.token_contract,
        "issue"_n,
        std::make_tuple(_self, to_issue, string("tokens to Issue contract"))
    }.send();

    // then send
    async_pay(_self, recipient, to_issue, s.token_contract, "tokens from Issue contract");

    print("done issuing of: ", to_issue);
    print("\n");
}

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        if (code == receiver){
            switch( action ) {
                EOSIO_DISPATCH_HELPER( Issue, (config)(issue))
            }
        }
        eosio_exit(0);
    }
}
