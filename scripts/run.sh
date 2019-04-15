#cleos wallet unlock --password XXXXXXXXXXXXXXXXXXXX...

#in another shell:
#rm -rf ~/.local/share/eosio/nodeos/data
#nodeos -e -p eosio --plugin eosio::chain_api_plugin --plugin eosio::history_api_plugin --contracts-console --verbose-http-errors

set -x

PUBLIC_KEY=EOS5CYr5DvRPZvfpsUGrQ2SnHeywQn66iSbKKXn4JDTbFFr36TRTX
EOSIO_DEV_KEY=EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV

cleos create account eosio bridge $PUBLIC_KEY
cleos set contract bridge . Bridge.wasm --abi Bridge.abi -p bridge@active
cleos push action bridge start '[]' -p bridge@active
cleos push action bridge start '[]' -p bridge@active