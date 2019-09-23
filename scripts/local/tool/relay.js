const fs = require('fs')
const Eos = require('eosjs')
const path = require('path');
const assert = require('assert');
const exec = require('await-exec')
const Web3 = require("web3");
const ecc = require('eosjs-ecc');

const snooze = ms => new Promise(resolve => setTimeout(resolve, ms));

function round_up(val, denom) {
    console.log("(val + denom - 1)", (val + denom - 1))
    console.log("(val + denom - 1) / denom)", (val + denom - 1) / denom)
    return (parseInt((val + denom - 1) / denom) * parseInt(denom));
}

function hexToBytes(hex) {
    hex = hex.replace("0x","")
    for (var bytes = [], c = 0; c < hex.length; c += 2)
    bytes.push(parseInt(hex.substr(c, 2), 16));
    return bytes;
}

String.prototype.leftJustify = function( length, char ) {
    var fill = [];
    while ( fill.length + this.length < length ) {
      fill[fill.length] = char;
    }
    return fill.join('') + this;
}

function parseRelayDaya(jsonFile){
    rawdata = fs.readFileSync(jsonFile);
    data = JSON.parse(rawdata);

    header_rlp = data["header_rlp"].replace("0x", "");
    header_rlp_buf = Buffer.from(header_rlp, 'hex')
    proof_length = data["proof_length"]

    dag_chunks = []
    data['elements'].forEach(function(element) {
        stripped = element.replace("0x", "");
        padded = stripped.leftJustify(32 * 2, '0')
        reversed = Uint8Array.from(Buffer.from(padded, 'hex')).reverse();
        dag_chunks.push(reversed)
    });
    dags = Buffer.concat(dag_chunks);

    proof_chunks = []
    data["merkle_proofs"].forEach(function(element) {
        stripped = element.replace("0x", "");
        padded = stripped.leftJustify(16 * 2, '0')
        proof_chunks.push(Buffer.from(padded, 'hex'))
    });
    proofs = Buffer.concat(proof_chunks);
    
    parsedData = {header_rlp: header_rlp_buf, proof_length: proof_length, dags: dags, proofs: proofs}
    return parsedData;
}

function returnTuple(cfg, block) {
    const buf = Buffer.allocUnsafe(16);
    relayerAsInt = cfg.relayerEos.modules.format.encodeName(cfg.relayerAccount)
    buf.writeIntBE(relayerAsInt, 0, 8)
    buf.writeIntLE(block, 8, 8)

    shaRes = ecc.sha256(buf)
    forEndianConversion = shaRes.substr(0, 16)
    tuple = parseInt('0x'+forEndianConversion.match(/../g).reverse().join(''));
    return tuple
}

async function isBlockInScratchpad(cfg, tuple, block) {
    let scratchpadReply = await cfg.relayerEos.getTableRows({
        code: cfg.bridgeAccount,
        scope: cfg.bridgeAccount,
        table: "scratchdata",
        json: true,
        lower_bound: (tuple - 10000),
        upper_bound: (tuple + 10000),
    })

    for (var t = 0; t < scratchpadReply.rows.length; t++) {
        if (block == scratchpadReply.rows[t].last_relayed_block) {
            return true
            break
        }
    }
    return false
}

async function getCroppedHeaderHash(cfg, blockNum) {
    web3 = new Web3(cfg.ethUrl)
    reply = await web3.eth.getBlock(blockNum)
    endian = reply["hash"].substring(2, 18)
    r = parseInt('0x'+endian.match(/../g).reverse().join('')); // reverse endianess
    return r
}

async function initScratchpad(cfg, bridgeAsRelayer, tuple, currentBlock) {
    console.log("initscratch")
    initSucceed = await isBlockInScratchpad(cfg, tuple, currentBlock - 1)
    if (initSucceed) console.log("scratchpad already initialized")

    // get previous anchor pointer
    hash = await getCroppedHeaderHash(cfg, currentBlock - 1)

    /* TODO: returned hash loses some precision on parseInt so we use a range and assume no collisions.
     * Need to further investigate
     */
    let anchorsReply = await cfg.relayerEos.getTableRows({
        code: cfg.bridgeAccount,
        scope:cfg.bridgeAccount,
        table:"anchors",
        json: true,
        key_type: 'i64',
        index_position: '2',
        lower_bound: (hash - 10000),
        upper_bound: (hash + 10000),
    })
    previous_anchor_pointer = anchorsReply["rows"][0]["current"]

    while(!initSucceed) {
        await bridgeAsRelayer.initscratch({
            msg_sender : cfg.relayerAccount,
            anchor_block_num : currentBlock + cfg.anchorSmallInterval - 1,
            previous_anchor_pointer : previous_anchor_pointer,
            },
            { authorization: [`${cfg.relayerAccount}@active`] }
        )

        initSucceed = await isBlockInScratchpad(cfg, tuple, currentBlock - 1)
        if (!initSucceed) {
            console.log("scratchpad not initialized, snooze and retry")
            await snooze(1000)
        }
    }
}

async function relayBlock(cfg, bridgeAsRelayer, tuple, currentBlock) {
    console.log("relay block")
    relaySucceed = await isBlockInScratchpad(cfg, tuple, currentBlock)
    if (relaySucceed) console.log("block already relayed")

    while(!relaySucceed) {
        JSON_PATH = cfg.dirForRelayFiles + "/" + "ethashproof_output_" + currentBlock + ".json"
        if (!fs.existsSync(JSON_PATH)) {
            await exec(`ethashproof/cmd/relayer/relayer ${currentBlock} | sed -e '1,/Json output/d' > ${JSON_PATH}`)
        }
        parsedData = parseRelayDaya(JSON_PATH);
        if (cfg.deleteRelayFiles) fs.unlinkSync(JSON_PATH)

        try {
            await bridgeAsRelayer.relay({ 
                msg_sender: cfg.relayerAccount,
                header_rlp: parsedData.header_rlp,
                dags: parsedData.dags,
                proofs: parsedData.proofs,
                proof_length: parsedData.proof_length},
                { authorization: [`${cfg.relayerAccount}@active`] }
            )
        } catch(e){
            console.log(e)
        }

        /* verify relayed succeeded */
        relaySucceed = await isBlockInScratchpad(cfg, tuple, currentBlock)
        if (!relaySucceed) {
            console.log("relay failed, snooze and retry")
            await snooze(1000)
        };
    }
}

async function finalizeScratchpad(cfg, bridgeAsRelayer, tuple, currentBlock) {
    console.log("finalize")
    finalizeSucceed = !(await isBlockInScratchpad(cfg, tuple, currentBlock))
    if (finalizeSucceed) console.log("finalized already done")

    while(!finalizeSucceed) {
        await bridgeAsRelayer.finalize({
            msg_sender : cfg.relayerAccount,
            anchor_block_num : currentBlock,
        },
        { authorization: [`${cfg.relayerAccount}@active`] });

        /* verify finalize succeeded */
        finalizeSucceed = !(await isBlockInScratchpad(cfg, tuple, currentBlock))
        if (!finalizeSucceed) {
            console.log("finalize failed, snooze and retry")
            await snooze(1000)
        };
    }
}

module.exports.mainLoop = async function(cfg, startBlock, endBlock)
{
    // TODO: pass these as well as tuple to inner function??

    bridgeContract = await cfg.relayerEos.contract(cfg.bridgeAccount);

    currentBlock = startBlock;
    currentTuple = 0;
    while(currentBlock <= endBlock) {
        console.log("block", currentBlock)

        /* if first block in scratchpad, then first init scratchpad */
        if (currentBlock % cfg.anchorSmallInterval == 1) {
            currentTuple = returnTuple(cfg, currentBlock + cfg.anchorSmallInterval - 1)
            await initScratchpad(cfg, bridgeContract, currentTuple, currentBlock)
        }

        await relayBlock(cfg, bridgeContract, currentTuple, currentBlock)

        /* if last block in scratchpad, then finalize scratchpad */
        if (currentBlock % cfg.anchorSmallInterval == 0) {
            await finalizeScratchpad(cfg, bridgeContract, currentTuple, currentBlock)
        }
        currentBlock++
    }
}