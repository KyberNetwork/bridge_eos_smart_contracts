CREATE_H_FILE = False
CREATE_CLEOS_FILE = True
PROOF_SIZE = 25
NUM_PROOFS = 64

f = open("ethashproof_output_proofs.txt", "r")
data = f.read()
split_data = data.strip().replace('0x', '').split(" ")

debug_counter = 0

# split each 32B element to two 16B chunks
split_chunks = []
for str in split_data:
    split_chunks.append(str.rjust(16 * 2, '0'))

# write header file
if CREATE_H_FILE:
    final_st = "std::string proofs_4700000[][MAX_PROOF_DEPTH] = {{" + "\n"
    
    proofs_counter = 0
    while proofs_counter < NUM_PROOFS:
        chunk_counter = 0
        while chunk_counter < PROOF_SIZE:
            final_st = final_st + '"' + split_chunks[proofs_counter * PROOF_SIZE + chunk_counter] + '"'
            if chunk_counter != (PROOF_SIZE - 1):
                final_st = final_st + ","
            final_st = final_st + "\n"
            chunk_counter = chunk_counter + 1
        if proofs_counter != (NUM_PROOFS - 1):
            final_st = final_st + "},{" + "\n"
        proofs_counter = proofs_counter + 1
    final_st = final_st + "}};" + "\n"

if CREATE_CLEOS_FILE:

    final_st = "cleos push action bridge start \'{\"dag_vec\":["
    for chunk in split_chunks:
        n=2
        split_to_2 = ["0x" + dag[i:i+n] for i in range(0, len(dag), n)]
        as_str = (', '.join(split_to_2) + ',')
        #if (dag != dag_strings[-1]):
        #    as_str = as_str + ","
        final_st = final_st + as_str + "\n"
    final_st = final_st + "]}\' -p bridge@active"

print final_st
######################################33







print(final_st)
