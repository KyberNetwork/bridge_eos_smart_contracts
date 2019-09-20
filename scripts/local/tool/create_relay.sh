set -x
BLOCK_NUM=$1;
GENESIS=$2 # either empty or "--genesis"

cd ethashproof/cmd/relayer/
./relayer $BLOCK_NUM | sed -e '1,/Json output/d' > ethashproof_output.json
#./relayer $BLOCK_NUM | tee /dev/tty | sed -e '1,/Json output/d' > ethashproof_output.json
cd ../../..
mv ethashproof/cmd/relayer/ethashproof_output.json scripts/local/tool 
cd scripts/local/tool
cp relay_template.js relay_$BLOCK_NUM.js
python parse_ethashproof_output.py $GENESIS >> relay_$BLOCK_NUM.js
cd ../../..
node scripts/local/tool/relay_$BLOCK_NUM.js
rm scripts/local/tool/relay_$BLOCK_NUM.js
rm scripts/local/tool/ethashproof_output.json
