#include <eosio/chain/genesis_state.hpp>

namespace eosio {
namespace chain {
    extern std::string dmc_root_key;
}
} // namespace eosio::chain

#define eosio_root_key dmc_root_key

#include "chain/genesis_state.cpp"
