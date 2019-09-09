set -x
BLOCK_NUM=$1;
GENESIS=$2 # either empty or "--genesis"

cd ethashproof/cmd/relayer/
./relayer $BLOCK_NUM | sed -e '1,/Json output/d' > ethashproof_output.json
cd ../../..
mv ethashproof/cmd/relayer/ethashproof_output.json scripts/local/data_structure 
cd scripts/local/data_structure
cp relay_template.js relay_$BLOCK_NUM.js
python parse_ethashproof_output.py $GENESIS >> relay_$BLOCK_NUM.js
cd ../../..