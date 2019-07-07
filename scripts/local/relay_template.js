const fs = require('fs')
const Eos = require('eosjs')
const path = require('path');
const assert = require('assert');

/* Assign keypairs. to accounts. Use unique name prefixes to prevent collisions between test modules. */
const keyPairArray = JSON.parse(fs.readFileSync("scripts/local/keys.json"))
const relayerData =    {account: "relayer",   publicKey: keyPairArray[0][0], privateKey: keyPairArray[0][1]}
const systemData =  {account: "eosio", publicKey: "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV", privateKey: "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"}

relayerData.eos = Eos({ keyProvider: relayerData.privateKey /* , verbose: 'false' */})
systemData.eos = Eos({ keyProvider: systemData.privateKey /* , verbose: 'false' */})

async function main (){

    /* create eos handler objects */
    //await systemData.eos.transaction(tr => {tr.newaccount({creator: "eosio", name:relayerData.account, owner: relayerData.publicKey, active: relayerData.publicKey})});
    
    /* create contract objects */
    bridgeAsRelayer = await relayerData.eos.contract("bridge");
    
// auto manufactured code to be added below:
