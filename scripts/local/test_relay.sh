set -x

# test case
bash scripts/local/create_relay.bash 3
bash scripts/local/create_relay.bash 4
bash scripts/local/create_relay.bash 5
bash scripts/local/create_relay.bash 6
bash scripts/local/create_relay.bash 7
# deploy and set 3 as genesis
bash scripts/local/deploy.sh 3
node scripts/local/relay_3.js --create_relayer
node scripts/local/relay_4.js
node scripts/local/relay_5.js
node scripts/local/relay_6.js
node scripts/local/relay_7.js

node scripts/local/verify_3.js
node scripts/local/verify_4.js