/* node apps/wknc-app/wknc-app.js --cfg scripts/local/wknc_app_sample_input/cfg.json --blockVerify=8123247 --receiptVerify=0x47d76b0a9290ad65db9e33301e9f68c45c005942ebb4c7f91503424cc599fcbf */

const fs = require('fs')
const Eos = require('eosjs')
const path = require('path');
const argv = require('yargs').argv
const waterloo = require('../../waterloo-bridge') // TODO remove path when publishing

async function main(){

    const cfg = JSON.parse(fs.readFileSync(argv.cfg))

    doVerify = !!argv.blockVerify
    if (doVerify) {
        BLOCK_TO_VERIFY = parseInt(argv.blockVerify)

        verifierKeys = JSON.parse(fs.readFileSync(cfg.verifier_keypair_json))
        verifierPrivateKey = verifierKeys[1]
        verifierEos = Eos({ keyProvider: verifierPrivateKey /*, verbose: 'false' */})
 
        await waterloo.verifyOnLongest(verifierEos,
                      cfg.verifier,
                      cfg.bridge,
                      cfg.dir_for_verify_files,
                      cfg.delete_verify_files,
                      cfg.anchor_small_interval,
                      BLOCK_TO_VERIFY,
                      cfg.min_accumulated_work_1k_res)
    }

    doReceipt = !!argv.receiptVerify
    if (doReceipt) {

        RECEIPT_TO_VERIFY = argv.receiptVerify

        verifierKeys = JSON.parse(fs.readFileSync(cfg.verifier_keypair_json))
        verifierPrivateKey = verifierKeys[1]
        verifierEos = Eos({ keyProvider: verifierPrivateKey /*, verbose: 'false' */})
        await waterloo.verifyReceipt(verifierEos,
                      cfg.verifier,
                      cfg.bridge,
                      cfg.dir_for_verify_files,
                      cfg.delete_verify_files,
                      RECEIPT_TO_VERIFY,
                      cfg.eth_url)
    }
}

main()
