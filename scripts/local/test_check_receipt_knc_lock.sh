set -x

# test case
#bash scripts/local/create_relay.sh 8123245
#bash scripts/local/create_relay.sh 8123246
#bash scripts/local/create_relay.sh 8123247

# deploy and set 8106145 as genesis
bash scripts/local/deploy.sh 8123245
node scripts/local/relay_8123245.js --create_relayer
node scripts/local/relay_8123246.js
node scripts/local/relay_8123247.js

node scripts/local/verify_8123247.js
