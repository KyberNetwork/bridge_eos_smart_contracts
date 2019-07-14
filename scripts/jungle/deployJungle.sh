set -x

jleos="cleos -u http://jungle2.cryptolions.io:80"

#cleos wallet unlock --password XXXXXXXXXXXXXXXXXXXX...

PUBLIC_KEY=EOS7qKhbpNCruW5F93FkRSWxoLk2HXJoDyxGU4GD1rKPLTtMvsabC
ACCOUNT_NAME=lionofcourse
EOS_ACCOUNT="eosio.token"
ALICE_ACCOUNT="testalicaaaa"
BRIDGE_ACCOUNT="testbridge14"

#check network liveness:
#$jleos get info
#$jleos get account lionofcourse 

#create account:
$jleos system newaccount --stake-net "0.1 EOS" --stake-cpu "0.1 EOS" --buy-ram-kbytes 8 -x 1000 $ACCOUNT_NAME $BRIDGE_ACCOUNT $PUBLIC_KEY

#buy ram if needed:
$jleos system buyram $ACCOUNT_NAME $BRIDGE_ACCOUNT --kbytes 600

#move some eos to the accounts
$jleos push action eosio.token transfer "[ \"$ACCOUNT_NAME\", \"$BRIDGE_ACCOUNT\", \"10.0000 EOS\", "m" ]" -p $ACCOUNT_NAME@active

#delegate bw in each party
$jleos system delegatebw $BRIDGE_ACCOUNT $BRIDGE_ACCOUNT "5.0 EOS" "5.0 EOS"
$jleos set contract $BRIDGE_ACCOUNT . Bridge.wasm --abi Bridge.abi -p $BRIDGE_ACCOUNT@active

$jleos push action $BRIDGE_ACCOUNT storeroots '{
"epoch_nums":[0,156],
"roots": [0x55,0xb8,0x91,0xe8,0x42,0xe5,0x8f,0x58,0x95,0x6a,0x84,0x7c,0xbb,0xf6,0x78,0x21,
	         0xd0,0xeb,0x4a,0x9f,0xf0,0xdc,0x08,0xa9,0x14,0x9b,0x27,0x5e,0x3a,0x64,0xe9,0x3d]
}' -p $BRIDGE_ACCOUNT@active
exit





