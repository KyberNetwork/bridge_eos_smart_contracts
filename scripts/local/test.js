// example: bash scripts/local/deploy.sh; node scripts/local/test.js 8585001 8585001 8585070 0x4ce1301838af85b73 8585012

const fs = require('fs')
const Eos = require('eosjs')
const path = require('path');
const argv = require('yargs').argv
const relay = require('../../tool/relay')
const verify = require('../../tool/verify')

function hexToBytes(hex) {
    hex = hex.replace("0x","")
    for (var bytes = [], c = 0; c < hex.length; c += 2)
    bytes.push(parseInt(hex.substr(c, 2), 16));
    return bytes;
}

async function main(){

    const keyPairArray = JSON.parse(fs.readFileSync("scripts/local/keys.json"))
    const relayerData =    {account: "relayer",   publicKey: keyPairArray[0][0], privateKey: keyPairArray[0][1]}
    const systemData =  {account: "eosio", publicKey: "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV", privateKey: "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"}
    const bridgeData = {account: "bridge", publicKey: "5JeKxP8eTUB9pU2jMKHBJB6cQc16VpMntZoeYz5Fv65wRyPNkZN", privateKey: "5JeKxP8eTUB9pU2jMKHBJB6cQc16VpMntZoeYz5Fv65wRyPNkZN"}

    systemEos = Eos({ keyProvider: systemData.privateKey /* , verbose: 'false' */})
    bridgeEos = Eos({ keyProvider: bridgeData.privateKey /*, verbose: 'false' */})
    relayerEos = Eos({ keyProvider: relayerData.privateKey /* , verbose: 'false' */})

    doInit = !!argv.genesis
    if (doInit) {
        GENESIS_BLOCK = parseInt(argv.genesis)
        PRE_GENESIS_HEADER_HASH = hexToBytes(argv.genesisHash) // 8 msbs from etherscan parent hash

        /* create relayer account */
        await systemEos.transaction(tr => {tr.newaccount({ 
            creator: "eosio",
            name: relayerData.account, 
            owner: relayerData.publicKey,
            active: relayerData.publicKey})
        });

        bridgeAsBridge = await bridgeEos.contract(bridgeData.account);
        await bridgeAsBridge.setgenesis({
            genesis_block_num : GENESIS_BLOCK,
            previous_header_hash : PRE_GENESIS_HEADER_HASH, 
            initial_difficulty : 0
            },
            { authorization: [`${bridgeData.account}@active`] 
        });
    }

    doRelay = (argv.start && argv.end)
    if (doRelay) {
        START_BLOCK = parseInt(argv.start)
        END_BLOCK = parseInt(argv.end)

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
    }

    doVerify = !!argv.blockVerify
    if (doVerify) {
        BLOCK_TO_VERIFY = parseInt(argv.blockVerify)
        MIN_ACCUMULATED_WORK_1K_RES = 236802888840643 // 236802888840643073 without compressing

        verifierData = relayerData
        verifierEos = Eos({ keyProvider: verifierData.privateKey /*, verbose: 'false' */})
        verify.verify(verifierEos,
                      verifierData.account,
                      bridgeData.account,
                      "tmp",
                      false,
                      50,
                      BLOCK_TO_VERIFY,
                      MIN_ACCUMULATED_WORK_1K_RES)
    }
}
main()