const fs = require('fs')
const Eos = require('eosjs')
const path = require('path');
const assert = require('assert');
const exec = require('await-exec')
const Web3 = require("web3");
const ecc = require('eosjs-ecc');

/* Assign keypairs. to accounts. Use unique name prefixes to prevent collisions between test modules. */
const keyPairArray = JSON.parse(fs.readFileSync("scripts/local/keys.json"))
const relayerData =    {account: "relayer",   publicKey: keyPairArray[0][0], privateKey: keyPairArray[0][1]}
const systemData =  {account: "eosio", publicKey: "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV", privateKey: "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"}
const bridgeData = {account: "bridge", publicKey: "5JeKxP8eTUB9pU2jMKHBJB6cQc16VpMntZoeYz5Fv65wRyPNkZN", privateKey: "5JeKxP8eTUB9pU2jMKHBJB6cQc16VpMntZoeYz5Fv65wRyPNkZN"}

bridgeData.eos = Eos({ keyProvider: bridgeData.privateKey /*, verbose: 'false' */})
relayerData.eos = Eos({ keyProvider: relayerData.privateKey /* , verbose: 'false' */})
systemData.eos = Eos({ keyProvider: systemData.privateKey /* , verbose: 'false' */})

ETHEREUM_URL = "https://mainnet.infura.io"
ANCHOR_SMALL_INTERVAL = 5
ANCHOR_BIG_INTERVAL = 10
GENESIS_BLOCK = parseInt(process.argv[2])
START_BLOCK = parseInt(process.argv[3])
END_BLOCK = parseInt(process.argv[4])
PRE_GENESIS_HEADER_HASH = hexToBytes(process.argv[5]) // 8 msbs from etherscan parent hash
INIT = (START_BLOCK == GENESIS_BLOCK)

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

function returnTuple(block, relayerAccount) {
    const buf = Buffer.allocUnsafe(16);
    relayerAsInt = bridgeData.eos.modules.format.encodeName(relayerAccount)
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

async function setGenesis() {
    await bridgeAsBridge.setgenesis({
        genesis_block_num : START_BLOCK,
        previous_header_hash : PRE_GENESIS_HEADER_HASH, 
        initial_difficulty : 0
    },
    { authorization: [`${bridgeData.account}@active`] });
}

async function initScratchpad(currentBlock) {
    console.log("initscratch")
    initSucceed = await isBlockInScratchpad(currentBlock - 1, bridgeData.eos, bridgeData.account /* TODO - should it be passed or constant? */)
    if (initSucceed) console.log("scratchpad already initialized")

    // get previous anchor pointer
    hash = await getCroppedHeaderHash(currentBlock - 1)

    /* TODO: returned hash loses some precision on parseInt so we use a range and assume no collisions.
     * Need to further investigate
     */
    let anchorsReply = await bridgeData.eos.getTableRows({
        code: bridgeData.account,
        scope:bridgeData.account,
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
            msg_sender : relayerData.account,
            anchor_block_num : currentBlock + ANCHOR_SMALL_INTERVAL - 1,
            previous_anchor_pointer : previous_anchor_pointer,
            },
            { authorization: [`${relayerData.account}@active`] }
        )

        initSucceed = await isBlockInScratchpad(currentBlock - 1, bridgeData.eos, bridgeData.account)
        if (!initSucceed) {
            console.log("scratchpad not initialized, snooze and retry")
            await snooze(1000)
        }
    }
}

async function relayBlock(currentBlock) {
    console.log("relay block")
    relaySucceed = await isBlockInScratchpad(currentBlock, bridgeData.eos, bridgeData.account)
    if (relaySucceed) console.log("block already relayed")

    while(!relaySucceed) {
        await exec(`bash scripts/local/tool/create_relay.sh ${currentBlock}`)

        /* verify relayed succeeded */
        relaySucceed = await isBlockInScratchpad(currentBlock, bridgeData.eos, bridgeData.account)
        if (!relaySucceed) {
            console.log("relay failed, snooze and retry")
            await snooze(1000)
        };
    }
}

async function finalizeScratchpad(currentBlock) {
    console.log("finalize")
    finalizeSucceed = !(await isBlockInScratchpad(currentBlock, bridgeData.eos, bridgeData.account))
    if (finalizeSucceed) console.log("finalized already done")

    while(!finalizeSucceed) {
        await bridgeAsRelayer.finalize({
            msg_sender : relayerData.account,
            anchor_block_num : currentBlock,
        },
        { authorization: [`${relayerData.account}@active`] });

        /* verify finalize succeeded */
        finalizeSucceed = !(await isBlockInScratchpad(currentBlock, bridgeData.eos, bridgeData.account))
        if (!finalizeSucceed) {
            console.log("finalize failed, snooze and retry")
            await snooze(1000)
        };
    }
}

async function main (){
    /* create relayer account */
    if(INIT) await systemData.eos.transaction(tr => {tr.newaccount({creator: "eosio", name:relayerData.account, owner: relayerData.publicKey, active: relayerData.publicKey})});

    bridgeAsBridge = await bridgeData.eos.contract("bridge");
    bridgeAsRelayer = await relayerData.eos.contract("bridge");

    if (INIT) await setGenesis()

    currentBlock = START_BLOCK;
    tuple = 0;
    while(currentBlock <= END_BLOCK) {
        console.log("block", currentBlock)

        /* if first block in scratchpad, then first init scratchpad */
        if (currentBlock % ANCHOR_SMALL_INTERVAL == 1) {
            tuple = returnTuple(currentBlock + ANCHOR_SMALL_INTERVAL - 1, relayerData.account)
            await initScratchpad(currentBlock)
        }

        await relayBlock(currentBlock, )

        /* if last block in scratchpad, then finalize scratchpad */
        if (currentBlock % ANCHOR_SMALL_INTERVAL == 0) await finalizeScratchpad(currentBlock)
        currentBlock++
    }
}

main()