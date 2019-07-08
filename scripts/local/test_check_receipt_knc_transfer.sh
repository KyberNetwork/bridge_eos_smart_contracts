set -x

# test case
#bash scripts/local/create_relay.bash 8106145
#bash scripts/local/create_relay.bash 8106146
#bash scripts/local/create_relay.bash 8106147

# deploy and set 8100068 as genesis
bash scripts/local/deploy.sh 8106145
node scripts/local/relay_8106145.js --create_relayer
node scripts/local/relay_8106146.js
node scripts/local/relay_8106147.js

node scripts/local/verify_8106147.js
