/* node apps/wknc-app/wknc-app.js --cfg scripts/local/wknc_app_sample_input/cfg.json --blockVerify=8123247 --receiptVerify=0x47d76b0a9290ad65db9e33301e9f68c45c005942ebb4c7f91503424cc599fcbf --issue */

const fs = require('fs')
const Eos = require('eosjs')
const argv = require('yargs').argv
const waterloo = require('../../waterloo-bridge') // TODO remove path when publishing

function fromStrToBuff(inputStr) {
    compressed = inputStr.split(' ').join('')
    asBuff = Buffer.from(compressed, 'hex')
    return asBuff
}


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

    doIssue = !!argv.issue
    if (doIssue) {
        WkncAsVerifier = await verifierEos.contract(cfg.wknc);
    
        // currently assume verify cache file file was not deleted, so can take header and receipt from there
        JSON_PATH = cfg.dir_for_verify_files + "/" + "verifyproof_output_" + RECEIPT_TO_VERIFY + ".json"
        data = JSON.parse(fs.readFileSync(JSON_PATH));
    
        header_rlp = fromStrToBuff(data.header_rlp)
        receipt_rlp = fromStrToBuff(data.value_rlp)
    
        await WkncAsVerifier.issue({ 
            header_rlp:header_rlp,
            receipt_rlp:receipt_rlp,
            event_num_in_tx:1 // TODO : support event num dynamically
           },
        { authorization: [`${cfg.verifier}@active`] } )
    }

}

main()
