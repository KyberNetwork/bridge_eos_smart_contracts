const fs = require('fs')
const Eos = require('eosjs')
const path = require('path');
const ecc = require('eosjs-ecc');
const common = require('./common');
const exec = require('await-exec')

module.exports.verify = async function(verifierEos,
                                       verifierAccount,
                                       bridgeAccount,
                                       dirForRelayFiles,
                                       deleteRelayFiles,
                                       smallInterval,
                                       currentBlock){

    bridgeAsVerifier= await verifierEos.contract(bridgeAccount);

    rounded_down = common.round_down(currentBlock, smallInterval)
    lower = (rounded_down == currentBlock) ? currentBlock - smallInterval + 1: rounded_down + 1;
    upper = common.round_up(currentBlock, smallInterval)

    allListElements = []
    for(i = lower; i <= upper; i++) {

        // get header rlps of all small interval block from relay tool and hash it with sha256

        // TODO - shared with relay.js, can unite
        JSON_PATH = dirForRelayFiles + "/" + "ethashproof_output_" + i + ".json"
        if (!fs.existsSync(JSON_PATH)) {
            await exec(`ethashproof/cmd/relayer/relayer ${currentBlock} | sed -e '1,/Json output/d' > ${JSON_PATH}`)
        }
        parsedData = common.parseRelayDaya(JSON_PATH);
        if (deleteRelayFiles) fs.unlinkSync(JSON_PATH)

        shaRes = ecc.sha256(parsedData["header_rlp"])
        reversed = Uint8Array.from(Buffer.from(shaRes.substring(0, 16), 'hex')).reverse()
        allListElements.push(reversed)
    }

    index = (currentBlock == smallInterval) ? smallInterval - 1 :
                                               currentBlock % smallInterval - 1
    currentBlockSha = Buffer.from(allListElements[index])
    allProofs = Buffer.concat(allListElements)

    await bridgeAsVerifier.veriflongest({
        header_rlp_sha256 : currentBlockSha,
        block_num :currentBlock,
        interval_list_proof : allProofs
    },
    { authorization: [`${verifierAccount}@active`] });
    
    console.log("verified block", currentBlock)

}
