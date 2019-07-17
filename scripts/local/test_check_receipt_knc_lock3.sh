set -x


# test case
#bash scripts/local/create_relay.sh 8161440 --genesis
#bash scripts/local/create_relay.sh 8161441
#bash scripts/local/create_relay.sh 8161442
#bash scripts/local/create_relay.sh 8161443
#bash scripts/local/create_relay.sh 8161444
#bash scripts/local/create_relay.sh 8161445
#bash scripts/local/create_relay.sh 8161446

bash scripts/local/create_verify.sh 8161443 0x6f3a28137d693e61051c55ed4725e8eb93b93e542e87a59c3388d80bb997d6ae 2

bash scripts/local/deploy.sh 8161440
node scripts/local/relay_8161440.js --create_relayer
node scripts/local/relay_8161441.js
node scripts/local/relay_8161442.js
node scripts/local/relay_8161443.js
node scripts/local/relay_8161444.js
node scripts/local/relay_8161445.js
node scripts/local/relay_8161446.js

node scripts/local/verify_8161443.js
