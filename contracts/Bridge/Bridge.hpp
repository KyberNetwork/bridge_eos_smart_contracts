#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/types.h>
#include <eosiolib/crypto.h>
#include <eosiolib/asset.hpp>
#include <eosiolib/symbol.hpp>
#include <eosiolib/singleton.hpp>

#include <string>
#include <vector>

using std::string;
using std::vector;
using bytes = vector<uint8_t>;
using namespace eosio;

typedef unsigned int uint;

CONTRACT Bridge : public contract {

    public:
        using contract::contract;

        ACTION relay(const vector<uint8_t>& header_rlp,
                     const vector<uint8_t>& dags,
                     const vector<uint8_t>& proofs,
                     uint proof_length);

        ACTION veriflongest(const vector<uint8_t>& header_hash);

        ACTION checkreceipt(const vector<uint8_t>& header_rlp,
                            const vector<uint8_t>& encoded_path,
                            const vector<uint8_t>& receipt_rlp,
                            const vector<uint8_t>& all_parent_nodes_rlps,
                            const vector<uint>& all_parnet_rlp_sizes);

        ACTION storeroots(uint64_t genesis_block_num,
                          const vector<uint64_t>& epoch_nums,
                          const vector<uint8_t>& dag_roots);

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

    /////// for longest chain optimized data structure ///////
        void setgenesis(uint64_t genesis_block_num,
                        uint64_t header_hash,
                        uint64_t difficulty);

        void initscratch(uint64_t msg_sender,
                         uint64_t anchor_block_num,
                         uint64_t previous_anchor_pointer);

        void storeheader(uint64_t msg_sender,
                         uint64_t block_num,
                         uint128_t difficulty,
                         uint64_t header_hash,
                         uint64_t previous_hash,
                         const vector<uint8_t>& header_rlp);

        void finalize(uint64_t msg_sender,
                      uint64_t anchor_block_num);

        void veriflongest(uint64_t header_rlp_sha256,
                          uint  block_num,
                          vector<uint64_t> interval_list_proof);

        // internal in data structure module:
        uint64_t allocate_pointer(uint64_t header_hash);

        TABLE newstate {
            uint64_t   last_issued_key;
            uint128_t  anchors_head_difficulty;
            uint64_t   anchors_head_block_num;
            uint64_t   anchors_head_pointer;
            uint64_t   genesis_block_num;
        };

        // anchors are the headers that are maintained in permanent storage
        TABLE anchors {
            // related to data structure
            uint64_t current;
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
            uint64_t         last_block_hash; // for ethash verification
            uint128_t        total_difficulty;
            vector<uint64_t> small_interval_list;
            uint64_t         previous_anchor_pointer;
            uint64_t primary_key() const { return anchor_sender_hash; }
        };

        typedef eosio::singleton<"newstate"_n, newstate> newstate_type;
        typedef eosio::multi_index<"anchors"_n, anchors> anchors_type;
        typedef eosio::multi_index<"scratchdata"_n, scratchdata> scratchdata_type;
    //////////////////////////////////////////////////////////

    private:
        void parse_header(struct header_info_struct* header_info, const bytes& header_rlp);
        void store_header(struct header_info_struct* header_info, const bytes& header_rlp);
};
