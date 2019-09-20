const fs = require('fs')
const Eos = require('eosjs')
const path = require('path');
const assert = require('assert');
const exec = require('await-exec')


/* Assign keypairs. to accounts. Use unique name prefixes to prevent collisions between test modules. */
const keyPairArray = JSON.parse(fs.readFileSync("scripts/local/keys.json"))
const relayerData =    {account: "relayer",   publicKey: keyPairArray[0][0], privateKey: keyPairArray[0][1]}
const systemData =  {account: "eosio", publicKey: "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV", privateKey: "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"}
const bridgeData = {account: "bridge", publicKey: "5JeKxP8eTUB9pU2jMKHBJB6cQc16VpMntZoeYz5Fv65wRyPNkZN", privateKey: "5JeKxP8eTUB9pU2jMKHBJB6cQc16VpMntZoeYz5Fv65wRyPNkZN"}

bridgeData.eos = Eos({ keyProvider: bridgeData.privateKey /*, verbose: 'false' */})
relayerData.eos = Eos({ keyProvider: relayerData.privateKey /* , verbose: 'false' */})
systemData.eos = Eos({ keyProvider: systemData.privateKey /* , verbose: 'false' */})

function hexToBytes(hex) {
    hex = hex.replace("0x","")
    for (var bytes = [], c = 0; c < hex.length; c += 2)
    bytes.push(parseInt(hex.substr(c, 2), 16));
    return bytes;
}

const snooze = ms => new Promise(resolve => setTimeout(resolve, ms));

async function isBlockInScratchpad(block, eos, bridgeAccount) {
    let scratchpadReply = await eos.getTableRows({
        code: bridgeAccount,
        scope:bridgeAccount,
        table:"scratchdata",
        json: true
    })

    for (var t = 0; t < scratchpadReply.rows.length; t++) {
        if (block == scratchpadReply.rows[t].last_relayed_block) {
            return true
            break
        }
    }
    return false
}

ANCHOR_SMALL_INTERVAL = 5
ANCHOR_BIG_INTERVAL = 10
GENESIS_BLOCK = parseInt(process.argv[2])
START_BLOCK = parseInt(process.argv[3])
END_BLOCK = parseInt(process.argv[4])
PRE_GENESIS_HEADER_HASH = hexToBytes(process.argv[5]) // NOTE: similar to etherscan, had to reverse bytes from hex(uint64_t)
INIT = (START_BLOCK == GENESIS_BLOCK)

async function main (){

    if(INIT) {
        /* create eos handler objects */
        await systemData.eos.transaction(tr => {tr.newaccount({creator: "eosio", name:relayerData.account, owner: relayerData.publicKey, active: relayerData.publicKey})});
    }

    /* create contract objects */
    bridgeAsBridge = await bridgeData.eos.contract("bridge");
    bridgeAsRelayer = await relayerData.eos.contract("bridge");

    if(INIT) {
        await bridgeAsBridge.setgenesis({
            genesis_block_num : START_BLOCK,
            previous_header_hash : PRE_GENESIS_HEADER_HASH, 
            initial_difficulty : 0
        },
        { authorization: [`${bridgeData.account}@active`] });
    }

    currentBlock = START_BLOCK;
    previous_anchor_pointer = parseInt((currentBlock - GENESIS_BLOCK) / 5);
    while(currentBlock <= END_BLOCK) {
        console.log("block", currentBlock)

        /* if first block in scratchpad, then first init scratchpad */
        if (currentBlock % ANCHOR_SMALL_INTERVAL == 1) {
            console.log("initscratch")
            while(1) {
                await bridgeAsRelayer.initscratch({
                    msg_sender : relayerData.account,
                    anchor_block_num : currentBlock + ANCHOR_SMALL_INTERVAL - 1,
                    previous_anchor_pointer : previous_anchor_pointer,
                    },
                    { authorization: [`${relayerData.account}@active`] }
                )

                /* verify new scratchpad is there with last_block as previous anchor */
                initSucceed = await isBlockInScratchpad(currentBlock - 1, bridgeData.eos, bridgeData.account)
                if (initSucceed) {
                    break
                } else {
                    console.log("scratchpad not initialized, snooze and retry")
                    await snooze(3000)
                };
            }

            previous_anchor_pointer++
        }

        console.log("relay block")
        while(1) {
            await exec(`bash scripts/local/tool/create_relay.sh ${currentBlock}`)

            /* verify relayed succeeded */
            relaySucceed = await isBlockInScratchpad(currentBlock, bridgeData.eos, bridgeData.account)
            if (relaySucceed) {
                break
            } else {
                console.log("relay failed, snooze and retry")
                await snooze(3000)
            };
        }

        /* if last block in scratchpad, then finalize scratchpad */
        if (currentBlock % ANCHOR_SMALL_INTERVAL == 0) {
            console.log("finalize")
            
            while(1) {
                await bridgeAsRelayer.finalize({
                    msg_sender : relayerData.account,
                    anchor_block_num : currentBlock,
                },
                { authorization: [`${relayerData.account}@active`] });

                /* verify finalize succeeded */
                finalizeFailed = await isBlockInScratchpad(currentBlock, bridgeData.eos, bridgeData.account)
                if (!finalizeFailed) {
                    break
                } else {
                    console.log("finalize failed, snooze and retry")
                    await snooze(3000)
                };
            }
        }

        currentBlock++

    }
}

main()