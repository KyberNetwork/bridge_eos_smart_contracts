'''
* dag manipulation on go tool output:
     -each 32 bytes chunk is internally reversed

* proofs manipulation on go tool output:
    -none
'''

PROOF_SIZE = 25
NUM_PROOFS = 64
MAX_PROOFS = 30 # this limitation is because of "/usr/local/bin/cleos: Argument list too long" error

# get dag data
f = open("ethash_output_dags.txt", "r")
data = f.read()
split_data = data.strip().replace('0x', '').split(" ")
dag_chunks = []
for str in split_data:
    # padd with 0s
    pad = str.rjust(32 * 2, '0')
    # reverse byte order
    rev =  "".join(reversed([pad[i:i+2] for i in range(0, len(pad), 2)]))
    dag_chunks.append(rev)

# get proofs data
f = open("ethashproof_output_proofs.txt", "r")
data = f.read()
split_data = data.strip().replace('0x', '').split(" ")
proof_chunks = []
counter = 0
for str in split_data:
    proof_chunks.append(str.rjust(16 * 2, '0'))
    counter = counter + 1
    if counter >= (MAX_PROOFS * PROOF_SIZE):
        break

# print cleos cmd
final_st = "cleos push action bridge start \'{\"dag_vec\":[\n"
for chunk in dag_chunks:
    n=2
    split_to_2 = ["0x" + chunk[i:i+n] for i in range(0, len(chunk), n)]
    as_str = (', '.join(split_to_2) + ',')
    final_st = final_st + as_str + "\n"
final_st = final_st + "],\n"

final_st = final_st + "\"proof_vec\":[\n"

for chunk in proof_chunks:
    n=2
    split_to_2 = ["0x" + chunk[i:i+n] for i in range(0, len(chunk), n)]
    as_str = (', '.join(split_to_2) + ',')
    final_st = final_st + as_str + "\n"
final_st = final_st + "]}\' -p bridge@active"

print(final_st)
