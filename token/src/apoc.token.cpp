#include <apoc.token.hpp>

namespace eosio
{

   void token::create(const name &issuer,
                      const asset &maximum_supply)
   {
      require_auth(get_self());

      auto sym = maximum_supply.symbol;
      check(sym.is_valid(), "invalid symbol name");
      check(maximum_supply.is_valid(), "invalid supply");
      check(maximum_supply.amount > 0, "max-supply must be positive");

      stats statstable(get_self(), sym.code().raw());
      auto existing = statstable.find(sym.code().raw());
      check(existing == statstable.end(), "token with symbol already exists");

      statstable.emplace(get_self(), [&](auto &s)
                         {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer; });
   }

   void token::issue(const name &to, const asset &quantity, const string &memo)
   {
      auto sym = quantity.symbol;
      check(sym.is_valid(), "invalid symbol name");
      check(memo.size() <= 256, "memo has more than 256 bytes");

      stats statstable(get_self(), sym.code().raw());
      auto existing = statstable.find(sym.code().raw());
      check(existing != statstable.end(), "token with symbol does not exist, create token before issue");
      const auto &st = *existing;
      check(to == st.issuer || to == name(INVESTOR_CONTRACT) || to == name(STAKING_CONTRACT), "tokens can only be issued to issuer account");

      check(has_auth(st.issuer) || has_auth(name(INVESTOR_CONTRACT)) || has_auth(name(STAKING_CONTRACT)),"No Authority");
      check(quantity.is_valid(), "invalid quantity");
      check(quantity.amount > 0, "must issue positive quantity");

      check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
      check(quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

      statstable.modify(st, same_payer, [&](auto &s)
                        { s.supply += quantity; });

      name temp = has_auth(st.issuer) ? st.issuer : (has_auth(name(INVESTOR_CONTRACT)) ? name(INVESTOR_CONTRACT) : name(STAKING_CONTRACT));
      add_balance(temp, quantity, temp);
   }

   void token::retire(const asset &quantity, const string &memo)
   {
      auto sym = quantity.symbol;
      check(sym.is_valid(), "invalid symbol name");
      check(memo.size() <= 256, "memo has more than 256 bytes");

      stats statstable(get_self(), sym.code().raw());
      auto existing = statstable.find(sym.code().raw());
      check(existing != statstable.end(), "token with symbol does not exist");
      const auto &st = *existing;

      require_auth(st.issuer);
      check(quantity.is_valid(), "invalid quantity");
      check(quantity.amount > 0, "must retire positive quantity");

      check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");

      statstable.modify(st, same_payer, [&](auto &s)
                        { s.supply -= quantity; });

      sub_balance(st.issuer, quantity);
   }

   void token::bloktransfer(const bool &block)
   {
      check(has_auth(get_self()) || has_auth(name(INVESTOR_CONTRACT)), "No Authority");
      blocks blocked(get_self(), get_self().value);
      if (blocked.begin() == blocked.end())
      {
         blocked.emplace(get_self(), [&](auto &a)
                        { a.blocked = block; });
      }
      else
      {
         auto itr = blocked.begin();
         blocked.modify(itr, same_payer, [&](auto &a)
                       { a.blocked = block; });
      }
   }

   void token::transfer(const name &from,
                        const name &to,
                        const asset &quantity,
                        const string &memo)
   {
      check(from != to, "cannot transfer to self");
      blocks blocked(get_self(), get_self().value);
      check(blocked.begin() != blocked.end(), "No transfer status found, Please contact admin !");
      auto itr = blocked.begin();
      if (itr->blocked)
         check(from == name(INVESTOR_CONTRACT) || from == name(STAKING_CONTRACT), "Transfer not allowed");
      require_auth(from);
      check(is_account(to), "to account does not exist");
      auto sym = quantity.symbol.code();
      stats statstable(get_self(), sym.raw());
      const auto &st = statstable.get(sym.raw());

      require_recipient(from);
      require_recipient(to);

      check(quantity.is_valid(), "invalid quantity");
      check(quantity.amount > 0, "must transfer positive quantity");
      check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
      check(memo.size() <= 256, "memo has more than 256 bytes");
      auto payer = has_auth(to) ? to : from;
      sub_balance(from, quantity);
      add_balance(to, quantity, payer);
   }

   void token::sub_balance(const name &owner, const asset &value)
   {
      accounts from_acnts(get_self(), owner.value);

      const auto &from = from_acnts.get(value.symbol.code().raw(), "no balance object found");
      check(from.balance.amount >= value.amount, "overdrawn balance");

      from_acnts.modify(from, owner, [&](auto &a)
                        { a.balance -= value; });
   }

   void token::add_balance(const name &owner, const asset &value, const name &ram_payer)
   {
      accounts to_acnts(get_self(), owner.value);
      auto to = to_acnts.find(value.symbol.code().raw());
      if (to == to_acnts.end())
      {
         to_acnts.emplace(ram_payer, [&](auto &a)
                          { a.balance = value; });
      }
      else
      {
         to_acnts.modify(to, same_payer, [&](auto &a)
                         { a.balance += value; });
      }
   }

   void token::open(const name &owner, const symbol &symbol, const name &ram_payer)
   {
      require_auth(ram_payer);

      check(is_account(owner), "owner account does not exist");

      auto sym_code_raw = symbol.code().raw();
      stats statstable(get_self(), sym_code_raw);
      const auto &st = statstable.get(sym_code_raw, "symbol does not exist");
      check(st.supply.symbol == symbol, "symbol precision mismatch");

      accounts acnts(get_self(), owner.value);
      auto it = acnts.find(sym_code_raw);
      if (it == acnts.end())
      {
         acnts.emplace(ram_payer, [&](auto &a)
                       { a.balance = asset{0, symbol}; });
      }
   }

   void token::close(const name &owner, const symbol &symbol)
   {
      require_auth(owner);
      accounts acnts(get_self(), owner.value);
      auto it = acnts.find(symbol.code().raw());
      check(it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect.");
      check(it->balance.amount == 0, "Cannot close because the balance is not zero.");
      acnts.erase(it);
   }

   std::string token::tokenname()
   {
      return "Apocalypseium";
   }

   std::string token::tokensymbol()
   {
      return "APOC";
   }

   std::int64_t token::decimals()
   {
      return 5;
   }

   asset token::totalsupply()
   {
      auto sym_code = eosio::symbol_code("APOC");
      auto contract_address = get_self();
      return get_supply(contract_address, sym_code);
   }

   asset token::balanceof(const name &owner)
   {
      auto sym_code = eosio::symbol_code("APOC");
      auto contract_address = get_self();
      return get_balance(contract_address, owner, sym_code);
   }

} /// namespace eosio
