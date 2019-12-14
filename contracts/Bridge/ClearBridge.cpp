#include "../Common/common.hpp"
#include "../Common/rlp/rlp.hpp"

CONTRACT Bridge : public contract {

    public:
        using contract::contract;

        TABLE state {
            uint64_t   headers_head;
            uint128_t  headers_head_difficulty;
            uint64_t   headers_head_block_num;
            uint64_t   genesis_block_num;
        };

        TABLE roots {
            uint64_t epoch_num;
            bytes    root;
            uint64_t primary_key() const { return epoch_num; }
        };

        TABLE headers {
            // TODO - only 8B out of the hash, in production add 32B to result and compare
            uint64_t         header_hash;
            uint64_t         previous_hash;
            uint128_t        total_difficulty;
            uint64_t         block_num;
            uint64_t primary_key() const { return header_hash; }
        };

        TABLE receipts {
            // TODO - only 8B out of the hash, in production add 32B to result and compare
            uint64_t receipt_header_hash;
            uint64_t primary_key() const { return receipt_header_hash; }
        };

        typedef eosio::singleton<"state"_n, state> state_type;
        typedef eosio::multi_index<"roots"_n, roots> roots_type;
        typedef eosio::multi_index<"headers"_n, headers> headers_type;
        typedef eosio::multi_index<"receipts"_n, receipts> receipts_type;

        ACTION clearstate();
        ACTION clearheaders();
        ACTION clearroots();
        ACTION clearrecs();
    private:
};

ACTION Bridge::clearstate()
{
    state_type state_inst(_self, _self.value);
    if(state_inst.exists()) {
        state_inst.remove();
    }
}

ACTION Bridge::clearheaders()
{
    headers_type headers_inst(_self, _self.value);
    auto it = headers_inst.begin();
    while (it != headers_inst.end()) {
        it = headers_inst.erase(it);
    }
}

ACTION Bridge::clearroots()
{
    roots_type roots_inst(_self, _self.value);
    auto it = roots_inst.begin();
    while (it != roots_inst.end()) {
        it = roots_inst.erase(it);
    }
}

ACTION Bridge::clearrecs()
{
    receipts_type receipts_inst(_self, _self.value);
    auto it = receipts_inst.begin();
    while (it != receipts_inst.end()) {
        it = receipts_inst.erase(it);
    }
}

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        if (code == receiver){
            switch( action ) {
                EOSIO_DISPATCH_HELPER( Bridge, (clearstate)(clearheaders)(clearroots)(clearrecs))
            }
        }
        eosio_exit(0);
    }
}
