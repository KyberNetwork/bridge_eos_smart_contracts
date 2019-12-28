# in one terminal:
# rm -rf ~/.local/share/eosio/nodeos/data; nodeos -e -p eosio --plugin eosio::chain_api_plugin --plugin eosio::history_api_plugin --contracts-console --verbose-http-errors --filter-on "*"
 
#in another terminal
set -x
DIR=bridge_$(date +"%Y_%m_%d_%H_%M")
REPO=git@github.com:KyberNetwork/bridge_eos_smart_contracts.git
BRANCH=master
EXISTING_TMP_DIR='/home/talbaneth/eos/bridge_eos_smart_contracts/tmp'

mkdir $DIR
cd $DIR
git clone $REPO
cd bridge_eos_smart_contracts
git checkout $BRANCH
bash scripts/build.sh

mkdir tmp #dir for caching relay information
cp -rf $EXISTING_TMP_DIR/* tmp/

bash scripts/local/run_all.sh
cd ..
