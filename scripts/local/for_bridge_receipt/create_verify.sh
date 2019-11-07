set -x
BLOCK_NUM=$1
TX_HASH=$2
EVENT_NUM_IN_TX=$3

node scripts/local/for_bridge_receipt/get_reciept_proof_data.js $TX_HASH
mv verifyproof_output.json scripts/local/for_bridge_receipt/ 

cd scripts/local/for_bridge_receipt
#python parse_verifyproof_output.py $EVENT_NUM_IN_TX > verify_$BLOCK_NUM.js
python parse_verifyproof_output.py > verify_$BLOCK_NUM.js

cat verify_template.js >> verify_$BLOCK_NUM.js
cd ../../..
 
 