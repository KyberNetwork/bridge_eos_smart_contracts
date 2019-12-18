const Web3 = require('web3'); 
const client = require('node-rest-client-promise').Client();
const web3 = new Web3('wss://mainnet.infura.io/ws');
const CONTRACT_ADDRESS = "0x980358360409b1cc913A916bC0Bf6f52F775242A";
const etherescan_url = `http://api.etherscan.io/api?module=contract&action=getabi&address=${CONTRACT_ADDRESS}`
const seq = require('./issue-sequence')

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

async function getContractAbi() {
    const etherescan_response = await client.getPromise(etherescan_url)
    const CONTRACT_ABI = JSON.parse(etherescan_response.data.result);
    return CONTRACT_ABI;
}


async function handleLock(blockNum, txHash) {
    console.log("handleLock")

    sleeptTime = 120000
    console.log("waiting " + sleeptTime/1000 + " seconds for work to accumulate before issuing pegged tokens")
    await sleep(sleeptTime)
    await seq.issueSequence("scripts/local/wknc_app_sample_input/cfg.json",
                            blockNum,
                            txHash)
}

async function futureEventQuery(){
    const CONTRACT_ABI = await getContractAbi();
    const contract = new web3.eth.Contract(CONTRACT_ABI, CONTRACT_ADDRESS);

    
    // debug code
    /*
    blockNum=9114720
    txHash="0x36be65c56ea30607f2e748f998efeeb1c2fe9d18abcc38be70f6cb7017466c28"
    await seq.issueSequence("scripts/local/wknc_app_sample_input/cfg.json",
                            blockNum,
                            txHash)
    return
    */

    contract.events.Lock({fromBlock: 'latest'})
    .on('data', (event) => {
        console.log("event", event);
        
        blockNum = event["blockNumber"]
        console.log("blockNum", blockNum);

        txHash = event["transactionHash"]
        console.log("txHash", txHash);

        handleLock(blockNum, txHash)
    })
    .on('error', console.error);
    
}

futureEventQuery();