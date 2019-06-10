f = open("input_block_4700000.hpp", "r")

dag_strings = []
lines = f.read().splitlines()
for line in lines:
    startIndex = line.find('\"')
    if startIndex == 0:
        dag_strings.append(line.replace('\"', '').replace(',', '').replace(';', '').replace('{', '').replace('}', ''))
#print dag_strings
#final_st = "unsigned char new_dag_nodes_4700000[128][64] = {\n"
#print(dag_strings)
#dag_strings = []
final_st = "cleos push action bridge start \'{\"dag_vec\":["
for dag in dag_strings:
    n=2
    split_to_2 = ["0x" + dag[i:i+n] for i in range(0, len(dag), n)]
    as_str = (', '.join(split_to_2) + ',')
    #if (dag != dag_strings[-1]):
    #    as_str = as_str + ","
    final_st = final_st + as_str + "\n"
final_st = final_st + "]}\' -p bridge@active"

print final_st
    


##print(lines[0])

##print(f.read())