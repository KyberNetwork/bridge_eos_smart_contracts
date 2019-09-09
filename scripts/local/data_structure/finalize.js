const fs = require('fs')
const Eos = require('eosjs')
const path = require('path');
const assert = require('assert');

/* Assign keypairs. to accounts. Use unique name prefixes to prevent collisions between test modules. */
const keyPairArray = JSON.parse(fs.readFileSync("scripts/local/keys.json"))
const relayerData =    {account: "relayer",   publicKey: keyPairArray[0][0], privateKey: keyPairArray[0][1]}
const systemData =  {account: "eosio", publicKey: "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV", privateKey: "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"}
const bridgeData = {account: "bridge", publicKey: "5JeKxP8eTUB9pU2jMKHBJB6cQc16VpMntZoeYz5Fv65wRyPNkZN", privateKey: "5JeKxP8eTUB9pU2jMKHBJB6cQc16VpMntZoeYz5Fv65wRyPNkZN"}

bridgeData.eos = Eos({ keyProvider: bridgeData.privateKey /*, verbose: 'false' */})
relayerData.eos = Eos({ keyProvider: relayerData.privateKey /* , verbose: 'false' */})
systemData.eos = Eos({ keyProvider: systemData.privateKey /* , verbose: 'false' */})

async function main (){

    /* create contract objects */
    bridgeAsBridge = await bridgeData.eos.contract("bridge");
    bridgeAsRelayer = await relayerData.eos.contract("bridge");

    await bridgeAsBridge.finalize({
        msg_sender : 0,
        anchor_block_num : process.argv[2], //8123003,
    },
    { authorization: [`${bridgeData.account}@active`] });

}

main()