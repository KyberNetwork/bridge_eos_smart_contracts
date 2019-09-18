set -x

# test case
bash scripts/local/create_relay.sh 3
bash scripts/local/create_relay.sh 4
bash scripts/local/create_relay.sh 5
bash scripts/local/create_relay.sh 6
bash scripts/local/create_relay.sh 7
# deploy and set 3 as genesis
bash scripts/local/deploy.sh 3
node scripts/local/relay_3.js --create_relayer
node scripts/local/relay_4.js
node scripts/local/relay_5.js
node scripts/local/relay_6.js
node scripts/local/relay_7.js
