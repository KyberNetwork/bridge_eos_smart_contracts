#include <math.h>

#include "../Common/common.hpp"
#include "../Common/rlp/nested_rlp.hpp"


CONTRACT Issue : public contract {

    public:
        using contract::contract;

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

        ACTION clearstate();
        ACTION clearlockid();

    private:

};

ACTION Issue::clearstate()
{
    state_type state_inst(_self, _self.value);
    if(state_inst.exists()) {
        state_inst.remove();
    }
}

ACTION Issue::clearlockid()
{
    lockid_type lockid_inst(_self, _self.value);
    auto it = lockid_inst.begin();
    while (it != lockid_inst.end()) {
        it = lockid_inst.erase(it);
    }
}

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        if (code == receiver){
            switch( action ) {
                EOSIO_DISPATCH_HELPER( Issue, (clearstate)(clearlockid))
            }
        }
        eosio_exit(0);
    }
}
