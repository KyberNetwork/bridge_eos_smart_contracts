#cleos wallet issue --password XXXXXXXXXXXXXXXXXXXX...

#in another shell:
#rm -rf ~/.local/share/eosio/nodeos/data
#nodeos -e -p eosio --plugin eosio::chain_api_plugin --plugin eosio::history_api_plugin --contracts-console --verbose-http-errors

set -x

PUBLIC_KEY=EOS5CYr5DvRPZvfpsUGrQ2SnHeywQn66iSbKKXn4JDTbFFr36TRTX

cleos create account eosio issue $PUBLIC_KEY
cleos create account eosio token $PUBLIC_KEY
cleos create account eosio multimultixx $PUBLIC_KEY

# token
cleos set contract token contracts/Token Token.wasm --abi Token.abi -p token@active
cleos set account permission token active "{\"threshold\": 1, \"keys\":[{\"key\":\"$PUBLIC_KEY\", \"weight\":1}] , \"accounts\":[{\"permission\":{\"actor\":\"token\",\"permission\":\"eosio.code\"},\"weight\":1}], \"waits\":[] }" owner -p token@active
cleos push action token create '[ "issue", "1000000000.0000 WATER"]'  -p token@active

# issue
cleos set contract issue contracts/Issue Issue.wasm --abi Issue.abi -p issue@active
cleos set account permission issue active "{\"threshold\": 1, \"keys\":[{\"key\":\"$PUBLIC_KEY\", \"weight\":1}] , \"accounts\":[{\"permission\":{\"actor\":\"issue\",\"permission\":\"eosio.code\"},\"weight\":1}], \"waits\":[] }" owner -p issue@active

cleos push action issue config "{
\"token_contract\":\"token\"
\"token_symbol\": \"4,WATER\",
\"bridge_contract\": \"bridge\"
}" -p issue@active



