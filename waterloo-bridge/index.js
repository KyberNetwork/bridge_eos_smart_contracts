var Init = require('./init');
var Relay = require('./relay');
var VerifyOnLongest = require('./verifyOnLongest');
var VerifyReceipt = require('./verifyReceipt');

//merging all functionality 
var WaterlooBridge = Object.assign(Init, Relay, VerifyOnLongest, VerifyReceipt);

module.exports = WaterlooBridge
