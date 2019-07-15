set -x

# test case
bash scripts/local/create_relay.sh 8100068 --genesis
bash scripts/local/create_relay.sh 8100069
bash scripts/local/create_relay.sh 8100070

# deploy and set 8100068 as genesis
bash scripts/local/deploy.sh 8100068
node scripts/local/relay_8100068.js --create_relayer
node scripts/local/relay_8100069.js
node scripts/local/relay_8100070.js

node scripts/local/verify_8100070.js
