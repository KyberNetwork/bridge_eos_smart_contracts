#include <eosiolib/print.hpp>
using eosio::print;

#define PARENT_HASH_FIELD       0
#define OMMERS_HASH_FIELD       1
#define BENEFICIARY_FIELD       2
#define STATE_ROOT_FIELD        3
#define TRANSACTION_ROOT_FIELD  4
#define RECIEPT_ROOT_FIELD      5
#define LOGS_BLOOM_FIELD        6
#define DIFFICULTY_FIELD        7
#define NUMBER_FIELD            8
#define GAS_LIMIT_FIELD         9
#define GAS_USED_FIELD          10
#define TIMESTAMP_FIELD         11
#define EXTRA_DATA_FIELD        12
#define MIX_HASH_FIELD          13
#define NONCE_FIELD             14

typedef unsigned int uint;

typedef struct _rlp_elem {
    unsigned char*     content;
    uint               len;
    struct _rlp_elem*  list;
    bool               is_list;
    struct _rlp_elem*  next;
} rlp_elem;


/*****************************************************************************/

static rlp_elem* get_n_elem(rlp_elem* list, uint n) {
    uint i;
    rlp_elem* curr = list->list;

    for(i = 0 ; i < n ; i++) {
        if(curr->next == NULL) return NULL;
        curr = curr->next;
    }

    return curr;
}

/*****************************************************************************/

static uint decode_single_byte(unsigned char* input, rlp_elem* output) {
    output->content = input;
    output->len = 1;
    output->is_list = false;

    return 1;
}

/*****************************************************************************/

static uint decode_number(unsigned char* input, uint len) {
    uint i, result;
    result = 0;
    for(i = 0 ; i < len ; i++) {
        result = result << 8;
        result += (unsigned char)input[i];
    }

    return result;
}

/*****************************************************************************/

static uint decode_string(unsigned char* input, rlp_elem* output) {
    unsigned char first_byte = input[0];
    uint len_num_bytes;
    uint string_len;

    output->is_list = false;

    if(first_byte <= 0xb7) {
        output->len = first_byte - 0x80;
        output->content = input + 1;
        len_num_bytes = 1;
    }
    else {
        len_num_bytes = first_byte - 0xb7;
        string_len = decode_number(input + 1, len_num_bytes);
        output->len = string_len;
        output->content = input + 1 + len_num_bytes;
        len_num_bytes++;
    }

    return output->len + len_num_bytes;
}

/*****************************************************************************/

static uint decode_list(unsigned char* input, rlp_elem* list, rlp_elem** scrath_pad) {
    unsigned char first_byte = input[0];
    uint len_num_bytes;
    uint list_len;
    uint elm_ind;
    uint elm_len;
    uint outputs_ind;
    unsigned char* current_input = input;
    rlp_elem* prev = NULL;
    rlp_elem* curr;
    //printf("decode list start %u\n", cntr++);

    if(first_byte <= 0xf7) {
        list_len = first_byte - 0xc0;
        len_num_bytes = 0;
        current_input = input + 1;
    }
    else {
        len_num_bytes = first_byte - 0xf7;
        list_len = decode_number(input + 1, len_num_bytes);
        current_input = input + 1 + len_num_bytes;
    }

    //printf("list len %u\n", list_len);

    outputs_ind = 0;
    list->is_list = true;
    curr = list->list = (*scrath_pad)++;
    for(elm_ind = 0 ; /*elm_ind < list_len*/ current_input < input + list_len + len_num_bytes + 1 /* elm_ind < 20*/ ; elm_ind++)
    {
        first_byte = current_input[0];

        memset(curr, 0x00, sizeof(rlp_elem));

        if(first_byte <= 0x7f) elm_len = decode_single_byte(current_input,curr);
        else if(first_byte <= 0xbf) elm_len = decode_string(current_input,curr);
        else {
            curr->list = (*scrath_pad)++;
            memset(curr->list, 0x00, sizeof(rlp_elem));

            elm_len = decode_list(current_input,
                          curr,
                          scrath_pad);
        }

        current_input += elm_len;
        outputs_ind += 1;

        if(prev != NULL) prev->next = curr;

        prev = curr;
        curr = (*scrath_pad)++;
    }

    //printf("decode list end %u\n", cntr--);

    return list_len + len_num_bytes + 1;
}

/*****************************************************************************/

static uint remove_last_field_from_rlp(unsigned char* rlp, uint field_len) {
    uint field_len_field_len;
    uint rlp_org_len;
    uint rlp_final_len;
    uint rlp_len_num_bytes;

    if(rlp[0] <= 0xf7) printf("error: rlp is too short\n");

    rlp_len_num_bytes = rlp[0] - 0xf7;
    rlp_org_len = decode_number(rlp + 1, rlp_len_num_bytes);

    if(field_len >= 55) printf("error: unexpected field_len\n");
    if(rlp_org_len <= 55 - field_len) printf("error: rlp is too short\n");

    if(field_len == 0) field_len_field_len = 0; // would happen once evey 2^64 blocks...
    else if(field_len <= 55) field_len_field_len = 1;
    else printf("error: unexpected field_len\n");

    rlp_final_len = rlp_org_len - (field_len + field_len_field_len);

    // make sure number of bytes wasn't chage
    if(rlp_final_len <= 256) printf("error: unexpcted rlp len\n");
    if(rlp_org_len >= 256 * 256) printf("error: unexpecpted rlp len\n");

    // new length is exactly 2 bytes
    rlp[1] = rlp_final_len >> 8;
    rlp[2] = rlp_final_len & 0xFF;

    return rlp_final_len + 3;
}

/*****************************************************************************/
