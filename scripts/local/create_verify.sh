#NOTE: first need to changge the tx hash in the iteration in tests/default/receipt.js that is marked to run (it.only)
set -x

BLOCK_NUM=$1

cd eth-proof
node_modules/.bin/mocha test/default/receipt.js
cd ..

mv eth-proof/verifyproof_output.json scripts/local/ 
cd scripts/local/
python parse_verifyproof_output.py > verify_$BLOCK_NUM.js

cat verify_template.js >> verify_$BLOCK_NUM.js
cd ../..
 
 