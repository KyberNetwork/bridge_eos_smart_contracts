'''
* dag manipulation on go tool output:
     -each 32 bytes chunk is internally reversed

* proofs manipulation on go tool output:
    -none
'''

USE_CLEOS=False
'''
for non cleos mode need to copy template file and append to it. Like this:
cp run_template.js try_4700000.js
python parse_ethashproof_output.py >> run_4700000.js
'''

PROOF_SIZE = 25
NUM_PROOFS = 64
MAX_PROOFS = 64 # this limitation is because of "/usr/local/bin/cleos: Argument list too long" error

# get json data

import json
import sys

dag_chunks = []
proof_chunks = []
with open('ethashproof_output.json') as json_file:  
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

# print tool cmds

final_st = ""
if USE_CLEOS: 
    final_st = "cleos push action bridge relay \'{ \n"
else:
    if len(sys.argv) > 1 and sys.argv[1] == "--genesis":
        final_st = "await bridgeAsBridge.relay({ \n"
    else:
        final_st = "await bridgeAsRelayer.relay({ \n"

if USE_CLEOS:
    final_st = final_st + "\"header_rlp\":[\n"
else:
    final_st = final_st + "header_rlp:[\n"

split_to_2 = ["0x" + header_rlp[i:i+2] for i in range(0, len(header_rlp), 2)]
as_str = (', '.join(split_to_2) + ',')
rlp_arr = as_str

if USE_CLEOS:
    final_st = final_st + rlp_arr + "],\n"
else:
    final_st = final_st + rlp_arr + "],\n"

if USE_CLEOS:
    final_st = final_st + "\"dags\":[\n"
else:
    final_st = final_st + "dags:[\n"

dag_arr = ""
for chunk in dag_chunks:
    split_to_2 = ["0x" + chunk[i:i+2] for i in range(0, len(chunk), 2)]
    as_str = (', '.join(split_to_2) + ',')
    dag_arr = dag_arr + as_str + "\n"
final_st = final_st + dag_arr

if USE_CLEOS:
    final_st = final_st + "],\n"
else:
    final_st = final_st + "],\n"

if USE_CLEOS:
    final_st = final_st + "\"proofs\":[\n"
else:
    final_st = final_st + "proofs:[\n"

proof_arr = ""
for chunk in proof_chunks:
    n=2
    split_to_2 = ["0x" + chunk[i:i+2] for i in range(0, len(chunk), 2)]
    as_str = (', '.join(split_to_2) + ',')
    proof_arr = proof_arr + as_str + "\n"

if USE_CLEOS:
    final_st = final_st + proof_arr +"],\n"
else:
    final_st = final_st + proof_arr +"],\n"

if USE_CLEOS:
    final_st = final_st + "\"proof_length\":" + str(proof_length) + "\n"
else:
    final_st = final_st + "proof_length:" + str(proof_length) + "},\n"

if USE_CLEOS:
    final_st = final_st + "}\' -p bridge@active"
else:
    if len(sys.argv) > 1 and sys.argv[1] == "--genesis":
        final_st = final_st + "{ authorization: [`${bridgeData.account}@active`] } )\n"
    else:
        final_st = final_st + "{ authorization: [`${relayerData.account}@active`] } )\n"
    final_st = final_st + "}\n"
    final_st = final_st + "main()"

print(final_st)
