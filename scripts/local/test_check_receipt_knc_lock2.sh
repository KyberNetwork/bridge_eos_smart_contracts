set -x

# test case
#bash scripts/local/create_relay.sh 8129057 --genesis
#bash scripts/local/create_relay.sh 8129058
#bash scripts/local/create_relay.sh 8129059

#bash scripts/local/create_verify.sh 8129059 0xcd04de15afc93ef49dd841aed6bf7c9baf9fcea3a4d2f0007c57c170df73642d

bash scripts/local/deploy.sh 8129057
node scripts/local/relay_8129057.js --create_relayer
node scripts/local/relay_8129058.js
node scripts/local/relay_8129059.js

node scripts/local/verify_8129059.js
