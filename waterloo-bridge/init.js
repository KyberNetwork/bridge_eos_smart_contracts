const fs = require('fs')
const Eos = require('eosjs')
const path = require('path');
const assert = require('assert');
const Web3 = require('web3');
const ecc = require('eosjs-ecc');
const common = require('./common');

// 8 msbs from etherscan parent hash
async function getCroppedHeaderAsBuf(ethUrl, blockNum) {
    web3 = new Web3(ethUrl)
    reply = await web3.eth.getBlock(blockNum)
    hashCropped =reply["hash"].substring(2, 18)
    return Buffer.from(hashCropped, "hex")
}

module.exports.storeroots = async function(
        bridgeEos,
        deployerAccount,
        bridgeAccount,
        epochNums,
        dagRoots)
{
    bridgeAsBridge = await bridgeEos.contract(bridgeAccount);

    for (i = 0; i < epochNums.length; i++) { 
        await bridgeAsBridge.storeroots({
            epoch_nums : epochNums[i],
            dag_roots : dagRoots[i],
            },
            { authorization: [`${deployerAccount}@active`] 
        });
    }

    return;
}

module.exports.setgenesis = async function(
        bridgeEos,
        deployerAccount,
        bridgeAccount,
        genesisBlockNum,
        ethUrl)
{
    preGenesisHeaderHash = await getCroppedHeaderAsBuf(ethUrl, genesisBlockNum - 1)

    bridgeAsBridge = await bridgeEos.contract(bridgeAccount);
    await bridgeAsBridge.setgenesis({
        genesis_block_num : genesisBlockNum,
        previous_header_hash : preGenesisHeaderHash, 
        initial_difficulty : 0
        },
        { authorization: [`${deployerAccount}@active`] 
    });
    
    return;
}

