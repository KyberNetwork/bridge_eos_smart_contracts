git submodule update --init --recursive
cd ethashproof; ./build.sh; cd ..
cd waterloo-bridge; npm install; cd ..
npm install
bash scripts/compile.sh
mkdir tmp # dir for caching relay information
