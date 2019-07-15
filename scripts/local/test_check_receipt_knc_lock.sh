set -x

# test case
bash scripts/local/create_relay.sh 8123245 --genesis
bash scripts/local/create_relay.sh 8123246
bash scripts/local/create_relay.sh 8123247

bash scripts/local/create_verify.sh 8123247 0x47d76b0a9290ad65db9e33301e9f68c45c005942ebb4c7f91503424cc599fcbf

bash scripts/local/deploy.sh 8123245
node scripts/local/relay_8123245.js --create_relayer
node scripts/local/relay_8123246.js
node scripts/local/relay_8123247.js

node scripts/local/verify_8123247.js
