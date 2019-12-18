const fs = require('fs')
const Eos = require('eosjs')
const waterloo = require('../../waterloo-bridge') // TODO remove path when publishing

function fromStrToBuff(inputStr) {
    compressed = inputStr.split(' ').join('')
    asBuff = Buffer.from(compressed, 'hex')
    return asBuff
}

module.exports.doVerify = async function(cfg, blockNum) {
    BLOCK_TO_VERIFY = blockNum

    verifierKeys = JSON.parse(fs.readFileSync(cfg.verifier_keypair_json))
    verifierPrivateKey = verifierKeys[1]
    verifierEos = Eos({ keyProvider: verifierPrivateKey /*, verbose: 'false' */})

    console.log("verifyOnLongest")
    await waterloo.verifyOnLongest(verifierEos,
                  cfg.verifier,
                  cfg.bridge,
                  cfg.dir_for_verify_files,
                  cfg.delete_verify_files,
                  cfg.anchor_small_interval,
                  BLOCK_TO_VERIFY,
                  cfg.min_accumulated_work_1k_res)

}

module.exports.doReceipt =  async function(cfg, txHash) {

    RECEIPT_TO_VERIFY = txHash

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

module.exports.doIssue = async function(cfg) {
    WkncAsVerifier = await verifierEos.contract(cfg.wknc);
    
    // currently assume verify cache file file was not deleted, so can take header and receipt from there
    JSON_PATH = cfg.dir_for_verify_files + "/" + "verifyproof_output_" + RECEIPT_TO_VERIFY + ".json"
    data = JSON.parse(fs.readFileSync(JSON_PATH));

    header_rlp = fromStrToBuff(data.header_rlp)
    receipt_rlp = fromStrToBuff(data.value_rlp)

    await WkncAsVerifier.issue({ 
        header_rlp:header_rlp,
        receipt_rlp:receipt_rlp,
        event_num_in_tx:cfg.event_num // TODO : support event num dynamically
       },
    { authorization: [`${cfg.verifier}@active`] } )
}

module.exports.issueSequence = async function(cfgPath, blockNum, TxHash){
    const cfg = JSON.parse(fs.readFileSync(cfgPath))

    console.log("doVerify")
    await this.doVerify(cfg, blockNum)
    console.log("doReceipt")
    await this.doReceipt(cfg, TxHash)
    console.log("doIssue")
    await this.doIssue(cfg, TxHash)
}
