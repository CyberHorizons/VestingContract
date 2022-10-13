#include "vesting.hpp"
#include <stdint.h>
#include <string>
#include <utility>
using namespace eosio;

static inline time_point_sec current_time_point_sec()
{
   return time_point_sec(current_time_point());
}

ACTION vesting::addconfig(const name &pool, const asset &token_pool, const int &tge_rate, const uint32_t release_period, const uint32_t &cliff_period, const bool &pause_claim)
{
   require_auth(get_self());
   auto its = config.find(pool.value);
   if (its == config.end())
   {
      config.emplace(get_self(), [&](auto &v)
                     {
      v.pool = pool;
      v.token_pool = token_pool;
      v.current_token_pool = token_pool;
      v.tge_rate = tge_rate;
      v.release_period = release_period;
      v.cliff_period = cliff_period;
      v.users_vested = 0;
      v.pause_claim = pause_claim; });
   }
   else
   {
      config.modify(its, get_self(),
                    [&](auto &v)
                    {
                       v.token_pool = token_pool;
                       v.tge_rate = tge_rate;
                       v.release_period = release_period;
                       v.cliff_period = cliff_period;
                       v.pause_claim = pause_claim;
                    });
   }
}

ACTION vesting::claim(const name &user)
{
   check(has_auth(user) || has_auth(get_self()), "No authority");
   vesting_s vesting(get_self(), user.value);
   auto its = vesting.require_find(user.value, "User entry doesn't exist");
   auto itr = config.require_find(its->pool.value, "Token Pool does't exists");
   check(!itr->pause_claim, "All Claims Paused For Now !");
   asset claim;
   claim.amount = get_amount_by_now(user);
   claim.amount += its->unclaimed.amount; // its->unclaimed.amount is TGE amount
   claim.symbol = TOKEN;
   check(claim.amount > 0.00000, "Nothing to Claim For Now !");
   uint32_t claim_time;
   if (current_time_point_sec().utc_seconds >= its->last_claim)
   {
      int days_bynow = (int)((current_time_point_sec().utc_seconds - its->last_claim) / its->release_delay);
      claim_time = its->last_claim + days_bynow * its->release_delay;
   }
   else
      claim_time = its->last_claim;

   if (current_time_point_sec().utc_seconds > its->end_date.utc_seconds)
   {
      vesting.erase(its);
      config.modify(itr, get_self(),
                    [&](auto &v)
                    { 
                      v.current_token_pool.amount -= claim.amount;
                      v.users_vested -= 1; 
                    });
   }
   else
   {
      vesting.modify(its, get_self(),
                     [&](auto &v)
                     {
                        v.tokens_claimed.amount += claim.amount;
                        v.last_claim = claim_time;
                        v.unclaimed.amount = 0;
                     });
      config.modify(itr, get_self(),
                    [&](auto &v)
                    { v.current_token_pool.amount -= claim.amount; });
   }

   // action(permission_level{get_self(), name("active")}, name(TOKEN_CONTRACT),
   //        name("issue"),
   //        std::make_tuple(get_self(), claim,
   //                        std::string("Tokens Minted for Investor Pool")))
   //     .send();

   action(permission_level{get_self(), name("active")}, name(TOKEN_CONTRACT),
          name("transfer"),
          std::make_tuple(get_self(), user, claim,
                          std::string("Claimed Tokens")))
       .send();
}

ACTION vesting::create(const name &pool, const name &user, const asset &quantity, const time_point_sec &start_date, const uint32_t &release_delay)
{
   require_auth(get_self());
   check(is_account(user), "must provide an existing account");
   check(user != get_self(), "user must not be the contract itself");
   auto itr = config.require_find(pool.value, "Token Pool does't exists");
   check(start_date.utc_seconds >= current_time_point_sec().utc_seconds, "invalid start date");
   vesting_s vesting(get_self(), user.value);
   auto its = vesting.find(user.value);
   check(its == vesting.end(), "User already exists");
   vesting.emplace(get_self(), [&](auto &v)
                   {
      asset temp;
      temp.amount = 0;
      temp.symbol = TOKEN;
      asset unclaimed;
      unclaimed.symbol = TOKEN;
      float rate = float(itr->tge_rate) / 100;
      unclaimed.amount = (float)(quantity.amount * rate); 

      time_point_sec end;
      end.utc_seconds = start_date.utc_seconds + itr->release_period;

      time_point_sec last_claim_temp;
      int first_claim = start_date.utc_seconds + itr->cliff_period;
      last_claim_temp.utc_seconds = first_claim - release_delay;
      
      v.pool = itr->pool;
      v.user = user;
      v.start_date = start_date;
      v.vesting_length = itr->release_period;
      v.quantity = quantity;
      v.release_delay = release_delay;
      v.tokens_claimed = temp;
      v.end_date = end;
      v.unclaimed = unclaimed;
      v.last_claim = last_claim_temp.utc_seconds; });

   config.modify(itr, get_self(),
                 [&](auto &v)
                 { v.users_vested += 1; });
}

ACTION vesting::cancel(const name &user)
{
   require_auth(get_self());
   vesting_s vesting(get_self(), user.value);
   auto its = vesting.require_find(user.value,"User doesn't exist !");
   auto itr = config.require_find(its->pool.value, "Token Pool does't exists");
   check(!itr->pause_claim, "All Claims Paused For Now !");
   asset claim;
   claim.amount = get_amount_by_now(user);
   claim.amount += its->unclaimed.amount;
   claim.symbol = TOKEN;

   if(claim.amount > 0.00000){
      config.modify(itr, get_self(),
                    [&](auto &v)
                    { v.current_token_pool.amount -= claim.amount;
                        v.users_vested -= 1; });
      action(permission_level{get_self(), name("active")}, name(TOKEN_CONTRACT),
          name("transfer"),
          std::make_tuple(get_self(), user, claim,
                          std::string("Claimed Tokens")))
       .send();
   }
   else
   config.modify(itr, get_self(),
                 [&](auto &v)
                 { v.users_vested -= 1; });
   remove(user);
}

uint64_t vesting::get_amount_by_now(const name &user)
{
   vesting_s vesting(get_self(), user.value);
   auto its = vesting.require_find(user.value, "User not found!");
   auto itr = config.require_find(its->pool.value, "Token Pool does't exists");
   uint64_t amount_by_now = 0;
   if (current_time_point_sec().utc_seconds >= (its->last_claim + its->release_delay))
   {
      if (current_time_point_sec().utc_seconds < its->end_date.utc_seconds)
      {
         int days_bynow = (int)((current_time_point_sec().utc_seconds - its->last_claim) / its->release_delay);
         float rate = float(itr->tge_rate) / 100;
         float tge_amount = (float)(its->quantity.amount * rate);
         float post_cliff_amount = (float)its->quantity.amount - tge_amount;
         int total_days = (int)(((its->vesting_length) - itr->cliff_period) / its->release_delay);
         float amount_per_day = post_cliff_amount / (float)total_days;
         amount_by_now = amount_per_day * (float)days_bynow;
      }
      else
         amount_by_now = its->quantity.amount - its->tokens_claimed.amount;
   }
   return amount_by_now;
}

void vesting::remove(const name &user)
{
   vesting_s vesting(get_self(), user.value);
   auto its = vesting.require_find(user.value, "User not found!");
   its = vesting.erase(its);
}

ACTION vesting::clear(const name &pool, const name &user)
{  
   require_auth(get_self());
   if(pool.to_string() != "none"){
      auto itr = config.require_find(pool.value,"Pool Doesn't exist !");
      config.erase(itr);
   }
   if(user.to_string() != "none") 
   remove(user);
}
