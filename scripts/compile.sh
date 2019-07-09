cd contracts/Bridge/ ;  eosio-cpp Bridge.cpp -o Bridge.wasm --abigen ; cd ../..
cd contracts/Unlock/ ;  eosio-cpp Unlock.cpp -o Unlock.wasm --abigen ; cd ../..
cd contracts/Token/  ;  eosio-cpp Token.cpp  -o Token.wasm --abigen  ; cd ../..