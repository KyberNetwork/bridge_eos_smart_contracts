/* example:
     bash scripts/local/bridge_bringup.sh
     node apps/relay-app/relay-app.js --cfg scripts/local/relay_app_sample_input/cfg.json --genesis=8585001 --start=8585001 --end=8585170 --blockVerify=8585005
*/

const fs = require('fs')
const Eos = require('eosjs')
const path = require('path');
const argv = require('yargs').argv
const waterloo = require('../../waterloo-bridge') // TODO remove path when publishing
const roots = require('./roots')

function hexToBytes(hex) {
    hex = hex.replace("0x","")
    for (var bytes = [], c = 0; c < hex.length; c += 2)
    bytes.push(parseInt(hex.substr(c, 2), 16));
    return bytes;
}

async function main(){

    const cfg = JSON.parse(fs.readFileSync(argv.cfg))

    doInit = !!argv.genesis
    if (doInit) {
        deployerKeys = JSON.parse(fs.readFileSync(cfg.deployer_keypair_json))
        deployerPrivateKey = deployerKeys[1]
        bridgeEos = Eos({ keyProvider: deployerPrivateKey /*, verbose: 'false' */})

        GENESIS_BLOCK = parseInt(argv.genesis)

        await waterloo.storeroots(
                bridgeEos,
                cfg.deployer,
                cfg.deployer,
                roots.epochNums,
                roots.dagRoots
        )

        await waterloo.setgenesis(
            bridgeEos,
            cfg.deployer,
            cfg.deployer,
            GENESIS_BLOCK,
            cfg.eth_url
        )
    }

    doRelay = (argv.start && argv.end)
    if (doRelay) {
        START_BLOCK = parseInt(argv.start)
        END_BLOCK = parseInt(argv.end)

        relayerKeys = JSON.parse(fs.readFileSync(cfg.relayer_keypair_json))
        relayerPrivateKey = relayerKeys[1]
        relayerEos = Eos({ keyProvider: relayerPrivateKey /*, verbose: 'false' */})
 
        mainLoopCfg = {
            relayerEos : relayerEos,
            bridgeAccount : cfg.deployer,
            relayerAccount : cfg.relayer,
            ethUrl : cfg.eth_url,
            anchorSmallInterval : cfg.anchor_small_interval,
            anchorBigInterval : cfg.anchor_big_interval,
            dirForRelayFiles : cfg.dir_for_relay_files,
            deleteRelayFiles : cfg.delete_relay_files
        }
    
        await waterloo.mainLoop(mainLoopCfg, START_BLOCK, END_BLOCK)
    }

    doVerify = !!argv.blockVerify
    if (doVerify) {
        BLOCK_TO_VERIFY = parseInt(argv.blockVerify)

        // assume current verifier is the relayer
        verifierKeys = JSON.parse(fs.readFileSync(cfg.relayer_keypair_json))
        verifierPrivateKey = verifierKeys[1]
        verifierEos = Eos({ keyProvider: verifierPrivateKey /*, verbose: 'false' */})
 
        waterloo.verify(verifierEos,
                      cfg.relayer, //again, assume current verifier is the relayer
                      cfg.deployer,
                      cfg.dir_for_relay_files,
                      cfg.delete_relay_files,
                      cfg.anchor_small_interval,
                      BLOCK_TO_VERIFY,
                      cfg.min_accumulated_work_1k_res)
    }
}

main()
