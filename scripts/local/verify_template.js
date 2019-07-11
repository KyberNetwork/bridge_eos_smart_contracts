
const fs = require('fs')
const Eos = require('eosjs')
const path = require('path');
const assert = require('assert');

/* Assign keypairs. to accounts. Use unique name prefixes to prevent collisions between test modules. */
const keyPairArray = JSON.parse(fs.readFileSync("scripts/local/keys.json"))
const userData =    {account: "user",   publicKey: keyPairArray[0][0], privateKey: keyPairArray[0][1]}
const systemData =  {account: "eosio", publicKey: "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV", privateKey: "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"}

userData.eos = Eos({ keyProvider: userData.privateKey /* , verbose: 'false' */})
systemData.eos = Eos({ keyProvider: systemData.privateKey /* , verbose: 'false' */})

async function main (){
  
    await systemData.eos.transaction(tr => {tr.newaccount({creator: "eosio", name:userData.account, owner: userData.publicKey, active: userData.publicKey})});

    /* create contract objects */
    bridgeAsUser = await userData.eos.contract("bridge");
    unlockAsUser = await userData.eos.contract("unlock");

await bridgeAsUser.checkreceipt({ 
    header_rlp_vec:header_rlp_vec,
    rlp_receipt:rlp_receipt,
    encoded_path:encoded_path,
    all_parent_nodes_rlps:all_parent_nodes_rlps,
    all_parnet_rlp_sizes:all_parnet_rlp_sizes,
},
{ authorization: [`${userData.account}@active`] } )
    
await unlockAsUser.unlock({ 
    header_rlp_vec:header_rlp_vec,
    rlp_receipt:rlp_receipt,
   },
{ authorization: [`${userData.account}@active`] } )
}

main()
