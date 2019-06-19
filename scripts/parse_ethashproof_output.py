'''
* dag manipulation on go tool output:
     -each 32 bytes chunk is internally reversed

* proofs manipulation on go tool output:
    -none
'''

PROOF_SIZE = 25
NUM_PROOFS = 64
MAX_PROOFS = 30 # this limitation is because of "/usr/local/bin/cleos: Argument list too long" error

# get json data

import json

#also add:
    #get:
        #rlp encoded raw data header
        #nonce,
        #all 128 results of dataset lookup result
        #witnessforlookup,
    #parse block number
    

dag_chunks = []
proof_chunks = []
with open('ethhashproof_output.json') as json_file:  
    data = json.load(json_file)
    for elem in data['elements']:
        stripped = elem.strip().replace('0x', '')
        padded = stripped.rjust(32 * 2, '0')
        rev =  "".join(reversed([padded[i:i+2] for i in range(0, len(padded), 2)])).encode("utf-8")
        dag_chunks.append(rev)

    counter = 0
    for elem in data['merkle_proofs']:
        stripped = elem.strip().replace('0x', '')
        padded = stripped.rjust(16 * 2, '0')
        proof_chunks.append(padded.encode("utf-8"))
        counter = counter + 1
        if counter >= (MAX_PROOFS * PROOF_SIZE):
            break

    header_rlp = data['header_rlp'].strip().replace('0x', '').encode("utf-8")
    proof_length = data['proof_length']

# print cleos cmd
final_st = "cleos push action bridge verify \'{ \n"

final_st = final_st + "\"header_rlp_vec\":[\n"
split_to_2 = ["0x" + header_rlp[i:i+2] for i in range(0, len(header_rlp), 2)]
as_str = (', '.join(split_to_2) + ',')
final_st = final_st + as_str + "],\n"

final_st = final_st + "\"dag_vec\":[\n"
for chunk in dag_chunks:
    split_to_2 = ["0x" + chunk[i:i+2] for i in range(0, len(chunk), 2)]
    as_str = (', '.join(split_to_2) + ',')
    final_st = final_st + as_str + "\n"
final_st = final_st + "],\n"

final_st = final_st + "\"proof_vec\":[\n"

for chunk in proof_chunks:
    n=2
    split_to_2 = ["0x" + chunk[i:i+2] for i in range(0, len(chunk), 2)]
    as_str = (', '.join(split_to_2) + ',')
    final_st = final_st + as_str + "\n"
final_st = final_st + "],\n"

final_st = final_st + "\"proof_length\":" + str(proof_length) + "\n"

final_st = final_st + "}\' -p bridge@active"




print(final_st)
