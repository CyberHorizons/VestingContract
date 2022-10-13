project(vesting)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( vesting vesting vesting.cpp )
target_include_directories( vesting PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( vesting ${CMAKE_SOURCE_DIR}/../ricardian )