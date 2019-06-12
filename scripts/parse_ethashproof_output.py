CREATE_H_FILE = False
CREATE_CLEOS_FILE = True
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

if CREATE_CLEOS_FILE:

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


if CREATE_H_FILE:
    final_st = "std::string proofs_4700000[][MAX_PROOF_DEPTH] = {{" + "\n"
    
    proofs_counter = 0
    proof_amount = min([NUM_PROOFS,MAX_PROOFS])
    while proofs_counter < proof_amount:
        chunk_counter = 0
        while chunk_counter < PROOF_SIZE:
            final_st = final_st + '"' + split_chunks[proofs_counter * PROOF_SIZE + chunk_counter] + '"'
            if chunk_counter != (PROOF_SIZE - 1):
                final_st = final_st + ","
            final_st = final_st + "\n"
            chunk_counter = chunk_counter + 1
        if proofs_counter != (proof_amount - 1):
            final_st = final_st + "},{" + "\n"
        proofs_counter = proofs_counter + 1
    final_st = final_st + "}};" + "\n"

print(final_st)
