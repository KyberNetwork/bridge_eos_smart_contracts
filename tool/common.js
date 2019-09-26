const fs = require('fs')

String.prototype.leftJustify = function( length, char ) {
    var fill = [];
    while ( fill.length + this.length < length ) {
      fill[fill.length] = char;
    }
    return fill.join('') + this;
}

module.exports.round_up = function(val, denom) {
    return (parseInt((val + denom - 1) / denom) * parseInt(denom));
}

module.exports.round_down = function(val, denom) {
    return (parseInt(val / denom) * parseInt(denom));
}

module.exports.parseRelayDaya = function parseRelayDaya(jsonFile){
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


