#cleos wallet issue --password XXXXXXXXXXXXXXXXXXXX...

#in another shell:
#rm -rf ~/.local/share/eosio/nodeos/data
#nodeos -e -p eosio --plugin eosio::chain_api_plugin --plugin eosio::history_api_plugin --contracts-console --verbose-http-errors

set -x

GENESIS_BLOCK=$1

PUBLIC_KEY=EOS5CYr5DvRPZvfpsUGrQ2SnHeywQn66iSbKKXn4JDTbFFr36TRTX
EOSIO_DEV_KEY=EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV

cleos create account eosio bridge $PUBLIC_KEY
cleos create account eosio issue $PUBLIC_KEY
cleos create account eosio token $PUBLIC_KEY
cleos create account eosio multimultixx $PUBLIC_KEY

# token
cleos set contract token contracts/Token Token.wasm --abi Token.abi -p token@active
cleos set account permission token active "{\"threshold\": 1, \"keys\":[{\"key\":\"$PUBLIC_KEY\", \"weight\":1}] , \"accounts\":[{\"permission\":{\"actor\":\"token\",\"permission\":\"eosio.code\"},\"weight\":1}], \"waits\":[] }" owner -p token@active
cleos push action token create '[ "issue", "1000000000.0000 WATER"]'  -p token@active

# bridge
cleos set contract bridge contracts/Bridge Bridge.wasm --abi Bridge.abi -p bridge@active
cleos push action bridge storeroots "{
\"genesis_block_num\":$GENESIS_BLOCK
\"epoch_nums\":[0,156,270],
\"dag_roots\": [0x55,0xb8,0x91,0xe8,0x42,0xe5,0x8f,0x58,0x95,0x6a,0x84,0x7c,0xbb,0xf6,0x78,0x21,
             0xd0,0xeb,0x4a,0x9f,0xf0,0xdc,0x08,0xa9,0x14,0x9b,0x27,0x5e,0x3a,0x64,0xe9,0x3d,
             0xa3,0x23,0x1a,0x20,0x7b,0x1d,0xaf,0x19,0x69,0x3a,0x1a,0x5a,0xd1,0x8c,0x6a,0xc4]
}" -p bridge@active

# issue
cleos set contract issue contracts/Issue Issue.wasm --abi Issue.abi -p issue@active
cleos set account permission issue active "{\"threshold\": 1, \"keys\":[{\"key\":\"$PUBLIC_KEY\", \"weight\":1}] , \"accounts\":[{\"permission\":{\"actor\":\"issue\",\"permission\":\"eosio.code\"},\"weight\":1}], \"waits\":[] }" owner -p issue@active

cleos push action issue config "{
\"token_contract\":\"token\"
\"token_symbol\": \"4,WATER\",
\"bridge_contract\": \"bridge\"
}" -p issue@active



