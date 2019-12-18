/* node apps/wknc-app/wknc-app.js --cfg scripts/local/wknc_app_sample_input/cfg.json --blockVerify=8123247 --receiptVerify=0x47d76b0a9290ad65db9e33301e9f68c45c005942ebb4c7f91503424cc599fcbf --issue */
const seq = require('./issue-sequence')  
const argv = require('yargs').argv
const fs = require('fs')

async function main(){

    const cfg = JSON.parse(fs.readFileSync(argv.cfg))

    if (!!argv.blockVerify) {
        await seq.doVerify(cfg, parseInt(argv.blockVerify))
    }

    if (!!argv.receiptVerify) {
        await seq.doReceipt(cfg, argv.receiptVerify)
    }

    if (!!argv.issue) {
        await seq.doIssue(cfg, RECEIPT_TO_VERIFY)
    }

}

main()
