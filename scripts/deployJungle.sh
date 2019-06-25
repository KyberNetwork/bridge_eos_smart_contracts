set -x

jleos="cleos -u http://jungle2.cryptolions.io:80"

#cleos wallet unlock --password XXXXXXXXXXXXXXXXXXXX...

PUBLIC_KEY=EOS7qKhbpNCruW5F93FkRSWxoLk2HXJoDyxGU4GD1rKPLTtMvsabC
ACCOUNT_NAME=lionofcourse
EOS_ACCOUNT="eosio.token"
ALICE_ACCOUNT="testalicaaaa"
BRIDGE_ACCOUNT="testbridge13"

#check network liveness:
#$jleos get info
#$jleos get account lionofcourse 

#create account:
$jleos system newaccount --stake-net "0.1 EOS" --stake-cpu "0.1 EOS" --buy-ram-kbytes 8 -x 1000 $ACCOUNT_NAME $BRIDGE_ACCOUNT $PUBLIC_KEY

#buy ram if needed:
$jleos system buyram $ACCOUNT_NAME $BRIDGE_ACCOUNT --kbytes 600

#move some eos to the accounts
$jleos push action eosio.token transfer "[ \"$ACCOUNT_NAME\", \"$BRIDGE_ACCOUNT\", \"3.0000 EOS\", "m" ]" -p $ACCOUNT_NAME@active

#delegate bw in each party
$jleos system delegatebw $BRIDGE_ACCOUNT $BRIDGE_ACCOUNT "1.0 EOS" "1.0 EOS"
$jleos set contract $BRIDGE_ACCOUNT . Bridge.wasm --abi Bridge.abi -p $BRIDGE_ACCOUNT@active

$jleos push action $BRIDGE_ACCOUNT storeroots '{
"epoch_num_vec":[0,156],
"root_vec": [0xa4,0x32,0x06,0xa1,0xf0,0xa5,0xf8,0xce,0x71,0x68,0x3e,0xb4,0x65,0x00,0x42,0xf9,
             0x02,0xfd,0xc3,0xa0,0x9e,0x42,0xbe,0x95,0x44,0x04,0xfb,0x9a,0xe9,0x2b,0xee,0x04,

             0x5b,0xdb,0x12,0x86,0xfc,0x8b,0x60,0x3c,0x44,0xa9,0xf5,0xc6,0x3f,0x0a,0xd6,0x26,
             0x05,0x47,0x47,0x2f,0x51,0x70,0xb0,0x76,0x28,0x98,0xc5,0xc7,0xf7,0x20,0xb1,0x5e]
}' -p $BRIDGE_ACCOUNT@active
exit





