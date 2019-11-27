set -x

# compile contracts
# bash scripts/compile.sh

# create bridge account
BRIDGE_ACCOUNT=bridge
BRIDGE_PUBLIC_KEY=EOS5CYr5DvRPZvfpsUGrQ2SnHeywQn66iSbKKXn4JDTbFFr36TRTX
cleos create account eosio $BRIDGE_ACCOUNT $BRIDGE_PUBLIC_KEY

# create relayer account
RELAYER_ACCOUNT=relay
RELAYER_PUBLIC_KEY=EOS5BVngJvx5Y1f4tdzK1bVykB79ps1ZRtDBvJo4d7kELvSta5ryN
cleos create account eosio $RELAYER_ACCOUNT $RELAYER_PUBLIC_KEY

# deploy bridge
cleos set contract $BRIDGE_ACCOUNT contracts/Bridge Bridge.wasm --abi Bridge.abi -p $BRIDGE_ACCOUNT@active

# fill out cfg.json and keypair files.
# run relay-app.js

