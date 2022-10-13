#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <stdint.h>
#include <string>

using namespace eosio;
using namespace std;

#define EOSIO name("eosio")
namespace eosiosystem
{
   class system_contract;
}

namespace eosio
{
   CONTRACT vesting : public contract
   {
   public:
      using contract::contract;

      const string TOKEN_CONTRACT = "waxapoctoken";
      static constexpr symbol TOKEN = symbol(symbol_code("APOC"), 4);

      ACTION addconfig(const name& pool, const asset &token_pool, const int &tge_rate, const uint32_t release_period, const uint32_t &cliff_period, const bool &pause_claim);
      ACTION claim(const name &user);
      ACTION create(const name& pool, const name &user, const asset &quantity, const time_point_sec &start_date, const uint32_t &release_delay);
      ACTION cancel(const name &user);
      ACTION clear(const name &pool, const name &user);

   private:
      TABLE config_t
      {
         name pool;
         asset token_pool;
         asset current_token_pool;
         int tge_rate;
         uint32_t release_period;
         uint32_t cliff_period;
         int users_vested;
         bool pause_claim;
         auto primary_key() const { return pool.value; };
      };

      TABLE vesting_t
      {
         name pool;
         name user;
         asset quantity;
         asset tokens_claimed;
         time_point_sec start_date;
         uint32_t vesting_length;
         time_point_sec end_date;
         uint32_t last_claim;
         uint32_t release_delay;
         asset unclaimed;
         auto primary_key() const { return user.value; };
      };
      typedef eosio::multi_index<name("vesting"), vesting_t> vesting_s;
      typedef eosio::multi_index<name("config"), config_t> config_s;
      config_s config = config_s(get_self(),get_self().value);
      uint64_t get_amount_by_now(const name &user);
      void remove(const name &user);
   };
}