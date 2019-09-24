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

    await bridgeAsBridge.veriflongest({
        header_rlp_sha256 : [0x96, 0x4a, 0x55, 0xd0, 0xce, 0xec, 0x4f, 0x4d],  //0x964a55d0ceec4f4d,  10829562609278930765,
        block_num :8123008,
        interval_list_proof : [0x5b, 0x09, 0x98, 0xcf, 0xf5, 0x3c, 0xf0, 0x92, //0x5b0998cff53cf092,  6559942351181901970,
                               0xec, 0x63, 0x16, 0x9d, 0x01, 0xcc, 0x9d, 0x08, //0xec63169d01cc9d08,  17033483079241211144,
                               0x96, 0x4a, 0x55, 0xd0, 0xce, 0xec, 0x4f, 0x4d, //0x964a55d0ceec4f4d,  10829562609278930765,
                               0x7c, 0x5c, 0x48, 0x48, 0x3e, 0xbe, 0x50, 0x8b, //0x7c5c48483ebe508b,  8961116833687949451,
                               0xef, 0x46, 0x24, 0x56, 0x2a, 0xd4, 0x46, 0x71] //0xef4624562ad44671,  17241508175938864753]
    },
    { authorization: [`${bridgeData.account}@active`] });

}

main()