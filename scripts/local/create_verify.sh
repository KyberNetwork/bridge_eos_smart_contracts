set -x

BLOCK_NUM=$1
TX_HASH=$2

node scripts/local/get_reciept_proof_data.js $TX_HASH
mv verifyproof_output.json scripts/local/ 

cd scripts/local/
python parse_verifyproof_output.py > verify_$BLOCK_NUM.js

cat verify_template.js >> verify_$BLOCK_NUM.js
cd ../..
 
 