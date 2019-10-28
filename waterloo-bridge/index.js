var Init = require('./init');
var Relay = require('./relay');
var Verify = require('./verify');

//merging all functionality 
var WaterlooBridge = Object.assign(Init, Relay, Verify);

module.exports = WaterlooBridge
