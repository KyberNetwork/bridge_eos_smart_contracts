# get json data

import json

def from_buffer_to_str(buffer):
    stripped = buffer.strip().replace(' ', '').replace(',', '').replace('0x','')
    split_to_2 = ["0x" + stripped[i:i+2] for i in range(0, len(stripped), 2)]
    as_str = "[" + (', '.join(split_to_2) + ',') + "]"
    return (as_str)

final_st = ""
with open('verifyproof_output.json') as json_file:  

    data = json.load(json_file)
    final_st += "rlp_receipt = " + from_buffer_to_str(data["value_rlp"])+ "\n"

    parent_sizes = []
    merged_items = ""
    for item in data["parent_nodes_rlp"]:
        parent_sizes.append(len(str(item.replace(' ',''))) / 2)
        merged_items += item
    final_st += "all_parnet_rlp_sizes = " + str(parent_sizes)+ "\n"
    final_st += "all_parent_nodes_rlps = " + from_buffer_to_str(str(merged_items))+ "\n"

    buffer = data["path"]
    if int(buffer[0]) == 1 or int(buffer[0]) == 3: #TODO: test this
        buffer = "00" + buffer
    striped = buffer.strip().replace(' ', '').replace('0x','')
    striped_len = len(striped)
    if striped_len % 2:
        padded = striped.rjust(striped_len+1, '0')
    else:
        padded = striped
    final_st += "encoded_path = " + from_buffer_to_str(padded)+ "\n"
    
with open('ethashproof_output.json') as json_file:  
    data = json.load(json_file)
    final_st += "header_rlp_vec = " + from_buffer_to_str(data["header_rlp"])+ "\n"

print(final_st)
