var Relay = require('./relay');
var Verify = require('./verify');

//merging all functionality 
var WaterlooBridge = Object.assign(Relay, Verify);

module.exports = WaterlooBridge
