set - x
bash scripts/local/data_structure/deploy.sh 8123001
node scripts/local/data_structure/init.js --create_relayer
node scripts/local/data_structure/init_scratch.js 8123005 0
node scripts/local/data_structure/relay_8123001.js
node scripts/local/data_structure/relay_8123002.js
node scripts/local/data_structure/relay_8123003.js
node scripts/local/data_structure/relay_8123004.js
node scripts/local/data_structure/relay_8123005.js
node scripts/local/data_structure/finalize.js 8123005
node scripts/local/data_structure/init_scratch.js 8123010 1
node scripts/local/data_structure/relay_8123006.js
node scripts/local/data_structure/relay_8123007.js
node scripts/local/data_structure/relay_8123008.js
node scripts/local/data_structure/relay_8123009.js
node scripts/local/data_structure/relay_8123010.js
node scripts/local/data_structure/finalize.js 8123010
node scripts/local/data_structure/init_scratch.js 8123015 2
node scripts/local/data_structure/relay_8123011.js
node scripts/local/data_structure/relay_8123012.js
node scripts/local/data_structure/relay_8123013.js
node scripts/local/data_structure/relay_8123014.js
node scripts/local/data_structure/relay_8123015.js
node scripts/local/data_structure/finalize.js 8123015
node scripts/local/data_structure/init_scratch.js 8123020 3
node scripts/local/data_structure/relay_8123016.js
node scripts/local/data_structure/relay_8123017.js
node scripts/local/data_structure/relay_8123018.js
node scripts/local/data_structure/relay_8123019.js
node scripts/local/data_structure/relay_8123020.js
node scripts/local/data_structure/finalize.js 8123020
node scripts/local/data_structure/init_scratch.js 8123025 4
node scripts/local/data_structure/relay_8123021.js
node scripts/local/data_structure/relay_8123022.js
node scripts/local/data_structure/relay_8123023.js
node scripts/local/data_structure/relay_8123024.js
node scripts/local/data_structure/relay_8123025.js
node scripts/local/data_structure/finalize.js 8123025
node scripts/local/data_structure/verify_8123008.js