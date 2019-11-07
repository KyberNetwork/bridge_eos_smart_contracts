set -x
BLOCK_NUM=$1
TX_HASH=$2
EVENT_NUM_IN_TX=$3

node scripts/local/for_wknc_app/get_reciept_proof_data.js $TX_HASH
mv verifyproof_output.json scripts/local/for_wknc_app/ 

cd scripts/local/for_wknc_app
#python parse_verifyproof_output.py $EVENT_NUM_IN_TX > verify_$BLOCK_NUM.js
python parse_verifyproof_output.py $EVENT_NUM_IN_TX> issue_$BLOCK_NUM.js

cat issue_template.js >> issue_$BLOCK_NUM.js
cd ../../..
