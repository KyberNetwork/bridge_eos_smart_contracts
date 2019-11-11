var Init = require('./init');
var Relay = require('./relay');
var VerifyOnLongest = require('./verifyOnLongest');

//merging all functionality 
var WaterlooBridge = Object.assign(Init, Relay, VerifyOnLongest);

module.exports = WaterlooBridge
