var Web3 = require('web3')
var EP   = require('./../../../eth-proof/index')
var eP   = new EP(new Web3.providers.HttpProvider("https://mainnet.infura.io"))
const rlp = require('rlp');
const fs = require("fs")

//  TODO - use caching for the output file, same as the relay files.
eP.getReceiptProof(process.argv[2]).then((result)=>{

    // verify proof is indeed correct 
    EP.receipt(result.path, result.value, result.parentNodes, result.header, result.blockHash)
  
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
    fs.writeFile("./verifyproof_output.json", JSON.stringify(outputObject), (err) => {
        if (err) {
            console.error(err);
            return;
        };
        console.log("File has been created");
    });

}).catch((e)=>{console.log(e)})
