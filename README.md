# Waterloo ETH->EOS side bridge

## Summary

Waterloo is a bi-directional ETH<->EOS decentralized relay bridge.
For the EOS->ETH side, see:
https://github.com/KyberNetwork/bridge_eth_smart_contracts

## Installation

Run `git submodule update --recursive`.
Follow each submodule build procedure

## Note

In eth-proof/node_modules/xhr2/lib/xhr2.js:719, need to fix absolute url string:

    .... 
    }
    console.log("fixing url to ", absoluteUrlString, " !!!!!!!!!!!!!!!!!!!")
    absoluteUrlString = "https://mainnet.infura.io"
    .....