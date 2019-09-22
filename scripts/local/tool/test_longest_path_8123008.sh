set -x

bash scripts/local/tool/deploy.sh
node scripts/local/tool/relay.js 8123001 8123001 8123012 0xc87f24d354120086
node scripts/local/tool/verify_8123008.js
