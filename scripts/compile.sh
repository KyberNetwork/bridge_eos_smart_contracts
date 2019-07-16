cd contracts/Bridge/ ;  eosio-cpp Bridge.cpp -o Bridge.wasm --abigen ; cd ../..
cd contracts/Issue/ ;  eosio-cpp Issue.cpp -o Issue.wasm --abigen ; cd ../..
cd contracts/Token/  ;  eosio-cpp Token.cpp  -o Token.wasm --abigen  ; cd ../..