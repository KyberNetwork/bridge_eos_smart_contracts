const fs = require('fs')
const Eos = require('eosjs')
const path = require('path');
const common = require('./common');
const exec = require('await-exec')

var Web3 = require('web3')
var EP = require('./../eth-proof/index') // TODO: make it a dependency
const rlp = require('rlp');

function fromStrToBuff(inputStr) {
    compressed = inputStr.split(' ').join('')
    asBuff = Buffer.from(compressed, 'hex')
    return asBuff
}

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

module.exports.verifyReceipt= async function(verifierEos,
                                       verifierAccount,
                                       bridgeAccount,
                                       dirForRelayFiles,
                                       deleteRelayFiles,
                                       txHash,
                                       ethUrl){

    // call tool to produce a receipt proof
    var eP = new EP(new Web3.providers.HttpProvider(ethUrl))

    JSON_PATH = dirForRelayFiles + "/" + "verifyproof_output_" + txHash + ".json"
    if (!fs.existsSync(JSON_PATH)) {
        
        eP.getReceiptProof(txHash).then((result)=>{

            // verify proof is indeed correct 
            EP.receipt(result.path,
                       result.value,
                       result.parentNodes,
                       result.header,
                       result.blockHash
            )
          
            // output to json
            var outputObject = {};
            outputObject["value_rlp"] = Buffer.from(rlp.encode(result.value),'hex').toString('hex').match(/../g).join(' ')
            outputObject["parent_nodes_rlp"] = []
            for (i = 0; i < result.parentNodes.length; i++) { 
                outputObject["parent_nodes_rlp"].push(Buffer.from(rlp.encode(result.parentNodes[i]),'hex').toString('hex').match(/../g).join(' '))
            }
            outputObject["path"] = result.path.toString('hex')
            outputObject["header_rlp"] = Buffer.from(rlp.encode(result.header),'hex').toString('hex').match(/../g).join(' ')
            outputObject["block_hash"] = Buffer.from(result.blockHash,'hex').toString('hex').match(/../g).join(' ')
            fs.writeFile(JSON_PATH, JSON.stringify(outputObject), (err) => {
                if (err) {
                    console.error(err);
                    return;
                };
                console.log("File has been created");
            });

        }).catch((e)=>{console.log(e)})
    }

    await sleep(10000)

    data = JSON.parse(fs.readFileSync(JSON_PATH));

    header_rlp = fromStrToBuff(data["header_rlp"])
    receipt_rlp = fromStrToBuff(data["value_rlp"])
    
    all_parent_nodes_rlps_str = ""
    all_parnet_rlp_sizes = []
    for (i in data["parent_nodes_rlp"]) {
        all_parent_nodes_rlps_str = all_parent_nodes_rlps_str + data["parent_nodes_rlp"][i]
        all_parnet_rlp_sizes.push(fromStrToBuff(data["parent_nodes_rlp"][i]).length)
    }
    all_parent_nodes_rlps = fromStrToBuff(all_parent_nodes_rlps_str)
    
   encode_path_str = data["path"].split(' ').join('')
    if (encode_path_str[0] == "1" || encode_path_str[0] == "3") {
        encode_path_str = "00" + encode_path_str
    }
    encoded_path = Buffer.from(encode_path_str, 'hex')

    if (deleteRelayFiles) fs.unlinkSync(JSON_PATH)

    // call bridge contract to verify the proof and store evidence
    bridgeAsVerifier= await verifierEos.contract(bridgeAccount);

    await bridgeAsVerifier.checkreceipt({ 
        header_rlp:header_rlp,
        receipt_rlp:receipt_rlp,
        encoded_path:encoded_path,
        all_parent_nodes_rlps:all_parent_nodes_rlps,
        all_parnet_rlp_sizes:all_parnet_rlp_sizes,
    },
    { authorization: [`${verifierAccount}@active`] } )
}
