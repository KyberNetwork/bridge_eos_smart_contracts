// example: bash scripts/local/deploy.sh; node scripts/local/test.js 8585001 8585001 8585070 0x4ce1301838af85b73 8585012

const fs = require('fs')
const Eos = require('eosjs')
const path = require('path');
const relay = require('../../tool/relay')
const verify = require('../../tool/verify')

function hexToBytes(hex) {
    hex = hex.replace("0x","")
    for (var bytes = [], c = 0; c < hex.length; c += 2)
    bytes.push(parseInt(hex.substr(c, 2), 16));
    return bytes;
}

async function main(){

    GENESIS_BLOCK = parseInt(process.argv[2])
    START_BLOCK = parseInt(process.argv[3])
    END_BLOCK = parseInt(process.argv[4])
    PRE_GENESIS_HEADER_HASH = hexToBytes(process.argv[5]) // 8 msbs from etherscan parent hash
    INIT = (START_BLOCK == GENESIS_BLOCK)
    BLOCK_TO_VERIFY = parseInt((process.argv[6]))

    const keyPairArray = JSON.parse(fs.readFileSync("scripts/local/keys.json"))
    const relayerData =    {account: "relayer",   publicKey: keyPairArray[0][0], privateKey: keyPairArray[0][1]}
    const systemData =  {account: "eosio", publicKey: "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV", privateKey: "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"}
    const bridgeData = {account: "bridge", publicKey: "5JeKxP8eTUB9pU2jMKHBJB6cQc16VpMntZoeYz5Fv65wRyPNkZN", privateKey: "5JeKxP8eTUB9pU2jMKHBJB6cQc16VpMntZoeYz5Fv65wRyPNkZN"}

    systemEos = Eos({ keyProvider: systemData.privateKey /* , verbose: 'false' */})
    bridgeEos = Eos({ keyProvider: bridgeData.privateKey /*, verbose: 'false' */})
    relayerEos = Eos({ keyProvider: relayerData.privateKey /* , verbose: 'false' */})

    /* create relayer account */
    if(INIT) {
        await systemEos.transaction(tr => {tr.newaccount({ 
            creator: "eosio",
            name: relayerData.account, 
            owner: relayerData.publicKey,
            active: relayerData.publicKey})
        });
    }

    if(INIT) {
        bridgeAsBridge = await bridgeEos.contract(bridgeData.account);
        await bridgeAsBridge.setgenesis({
            genesis_block_num : GENESIS_BLOCK,
            previous_header_hash : PRE_GENESIS_HEADER_HASH, 
            initial_difficulty : 0
            },
            { authorization: [`${bridgeData.account}@active`] 
        });
    }

    cfg = {
        relayerEos : relayerEos,
        bridgeAccount : bridgeData.account,
        relayerAccount : relayerData.account,
        ethUrl : "https://mainnet.infura.io",
        anchorSmallInterval : 50,
        anchorBigInterval : 1000,
        dirForRelayFiles : "tmp",
        deleteRelayFiles : false
    }

    await relay.mainLoop(cfg, START_BLOCK, END_BLOCK)

    verifierData = relayerData
    verifierEos = Eos({ keyProvider: verifierData.privateKey /*, verbose: 'false' */})
    verify.verify(verifierEos,
                  verifierData.account,
                  bridgeData.account,
                  "tmp",
                  false,
                  50,
                  BLOCK_TO_VERIFY)
}
main()