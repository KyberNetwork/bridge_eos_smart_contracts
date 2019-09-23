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

function returnTuple(block, relayerAccount, eos) {
    const buf = Buffer.allocUnsafe(16);
    relayerAsInt = eos.modules.format.encodeName(relayerAccount)
    buf.writeIntBE(relayerAsInt, 0, 8)
    buf.writeIntLE(block, 8, 8)

    shaRes = ecc.sha256(buf)
    forEndianConversion = shaRes.substr(0, 16)
    tuple = parseInt('0x'+forEndianConversion.match(/../g).reverse().join(''));
    return tuple
}

async function isBlockInScratchpad(block, eos, bridgeAccount) {
    let scratchpadReply = await eos.getTableRows({
        code: bridgeAccount,
        scope:bridgeAccount,
        table:"scratchdata",
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

async function getCroppedHeaderHash(blockNum) {
    web3 = new Web3(ETHEREUM_URL)
    reply = await web3.eth.getBlock(blockNum)
    endian = reply["hash"].substring(2, 18)
    r = parseInt('0x'+endian.match(/../g).reverse().join('')); // reverse endianess
    return r
}

async function setGenesis(bridgeAccount) {
    bridgeAsBridge = await bridgeEos.contract(bridgeAccount);
    await bridgeAsBridge.setgenesis({
        genesis_block_num : GENESIS_BLOCK,
        previous_header_hash : PRE_GENESIS_HEADER_HASH, 
        initial_difficulty : 0
    },
    { authorization: [`${bridgeAccount}@active`] });
}

async function initScratchpad(currentBlock, eos, bridgeAccount, relayerAccount) {
    console.log("initscratch")
    initSucceed = await isBlockInScratchpad(currentBlock - 1, eos, bridgeAccount)
    if (initSucceed) console.log("scratchpad already initialized")

    // get previous anchor pointer
    hash = await getCroppedHeaderHash(currentBlock - 1)

    /* TODO: returned hash loses some precision on parseInt so we use a range and assume no collisions.
     * Need to further investigate
     */
    let anchorsReply = await eos.getTableRows({
        code: bridgeAccount,
        scope:bridgeAccount,
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
            msg_sender : relayerAccount,
            anchor_block_num : currentBlock + ANCHOR_SMALL_INTERVAL - 1,
            previous_anchor_pointer : previous_anchor_pointer,
            },
            { authorization: [`${relayerAccount}@active`] }
        )

        initSucceed = await isBlockInScratchpad(currentBlock - 1, eos, bridgeAccount)
        if (!initSucceed) {
            console.log("scratchpad not initialized, snooze and retry")
            await snooze(1000)
        }
    }
}

async function relayBlock(currentBlock, eos, bridgeAccount, relayerAccount) {
    console.log("relay block")
    relaySucceed = await isBlockInScratchpad(currentBlock, eos, bridgeAccount)
    if (relaySucceed) console.log("block already relayed")

    while(!relaySucceed) {
        JSON_PATH = DIR_FOR_RELAY_FILES + "/" + "ethashproof_output_" + currentBlock + ".json"
        if (!fs.existsSync(JSON_PATH)) {
            await exec(`ethashproof/cmd/relayer/relayer ${currentBlock} | sed -e '1,/Json output/d' > ${JSON_PATH}`)
        }
        parsedData = parseRelayDaya(JSON_PATH);
        if (DELETE_RELAY_FILES) fs.unlinkSync(JSON_PATH)

        try {
            await bridgeAsRelayer.relay({ 
                msg_sender: relayerAccount,
                header_rlp: parsedData.header_rlp,
                dags: parsedData.dags,
                proofs: parsedData.proofs,
                proof_length: parsedData.proof_length},
                { authorization: [`${relayerAccount}@active`] }
            )
        } catch(e){
            console.log(e)
        }

        /* verify relayed succeeded */
        relaySucceed = await isBlockInScratchpad(currentBlock, eos, bridgeAccount)
        if (!relaySucceed) {
            console.log("relay failed, snooze and retry")
            await snooze(1000)
        };
    }
}

async function finalizeScratchpad(currentBlock, eos, bridgeAccount, relayerAccount) {
    console.log("finalize")
    finalizeSucceed = !(await isBlockInScratchpad(currentBlock, eos, bridgeAccount))
    if (finalizeSucceed) console.log("finalized already done")

    while(!finalizeSucceed) {
        await bridgeAsRelayer.finalize({
            msg_sender : relayerAccount,
            anchor_block_num : currentBlock,
        },
        { authorization: [`${relayerAccount}@active`] });

        /* verify finalize succeeded */
        finalizeSucceed = !(await isBlockInScratchpad(currentBlock, eos, bridgeAccount))
        if (!finalizeSucceed) {
            console.log("finalize failed, snooze and retry")
            await snooze(1000)
        };
    }
}

async function mainLoop(bridgeEos, relayerEos, bridgeAccount, relayerAccount){

    // TODO: pass these as well as tuple to inner function??
    bridgeAsRelayer = await relayerEos.contract(bridgeAccount);
    eos = relayerEos // can be any eos object

    currentBlock = START_BLOCK;
    tuple = 0;
    while(currentBlock <= END_BLOCK) {
        console.log("block", currentBlock)

        /* if first block in scratchpad, then first init scratchpad */
        if (currentBlock % ANCHOR_SMALL_INTERVAL == 1) {
            tuple = returnTuple(currentBlock + ANCHOR_SMALL_INTERVAL - 1, relayerAccount, relayerEos)
            await initScratchpad(currentBlock, eos, bridgeAccount, relayerAccount)
        }

        await relayBlock(currentBlock, eos, bridgeAccount, relayerAccount)

        /* if last block in scratchpad, then finalize scratchpad */
        if (currentBlock % ANCHOR_SMALL_INTERVAL == 0) {
            await finalizeScratchpad(currentBlock, eos, bridgeAccount, relayerAccount)
        }
        currentBlock++
    }
}


async function main(){
    
    ///////// these should be given from outside as part of a test
    const keyPairArray = JSON.parse(fs.readFileSync("scripts/local/keys.json"))
    const relayerData =    {account: "relayer",   publicKey: keyPairArray[0][0], privateKey: keyPairArray[0][1]}
    const systemData =  {account: "eosio", publicKey: "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV", privateKey: "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"}
    const bridgeData = {account: "bridge", publicKey: "5JeKxP8eTUB9pU2jMKHBJB6cQc16VpMntZoeYz5Fv65wRyPNkZN", privateKey: "5JeKxP8eTUB9pU2jMKHBJB6cQc16VpMntZoeYz5Fv65wRyPNkZN"}

    bridgeEos = Eos({ keyProvider: bridgeData.privateKey /*, verbose: 'false' */})
    relayerEos = Eos({ keyProvider: relayerData.privateKey /* , verbose: 'false' */})

    bridgeAccount = bridgeData.account
    relayerAccount = relayerData.account

    // only for init
    relayerPublicKey = relayerData.publicKey
    systemEos = Eos({ keyProvider: systemData.privateKey /* , verbose: 'false' */})

    // TODO - insert these to configuration as well so can write tests
    // maintain option to pass private keys as well and not eos objects!!!
    ETHEREUM_URL = "https://mainnet.infura.io"
    ANCHOR_SMALL_INTERVAL = 5
    ANCHOR_BIG_INTERVAL = 10
    START_BLOCK = parseInt(process.argv[3])
    END_BLOCK = parseInt(process.argv[4])
    DIR_FOR_RELAY_FILES = "tmp"
    DELETE_RELAY_FILES = false

    ////// these are part of the tool but not part of the basic package ///// 
    GENESIS_BLOCK = parseInt(process.argv[2])
    INIT = (START_BLOCK == GENESIS_BLOCK)
    PRE_GENESIS_HEADER_HASH = hexToBytes(process.argv[5]) // 8 msbs from etherscan parent hash

    /* create relayer account */
    if(INIT) await systemEos.transaction(tr => {tr.newaccount({
        creator: "eosio",
        name:relayerAccount, 
        owner: relayerPublicKey,
        active: relayerPublicKey})
    });
    
    if (INIT) await setGenesis(bridgeAccount)
    ///////////////////////////////////////////////////////////////////

    await mainLoop(bridgeEos, // relevant only if start == genesis, but not really - should handle!!!
                   relayerEos,
                   bridgeAccount,
                   relayerAccount);
}
main()