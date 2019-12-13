#include "Token.hpp"

using namespace eosio;

ACTION Token::create(name issuer, asset maximum_supply) {
    accounts accounts_inst(_self, _self.value);
    auto it1 = accounts_inst.begin();
    while (it1 != accounts_inst.end()) {
        it1 = accounts_inst.erase(it1);
    }

    stats stats_inst(_self, _self.value);
    auto it2 = stats_inst.begin();
    while (it2 != stats_inst.end()) {
        it2 = stats_inst.erase(it2);
    }
}

ACTION Token::issue(name to, asset quantity, string memo) {
    auto sym = quantity.symbol;
    eosio_assert(sym.is_valid(), "invalid symbol name");
    eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

    auto sym_name = sym.code().raw();
    stats statstable(_self, sym_name);
    auto existing = statstable.find(sym_name);
    eosio_assert(existing != statstable.end(), "token with symbol does not exist, create token before issue");
    const auto& st = *existing;
    eosio_assert( to == st.issuer, "tokens can only be issued to issuer account" );

    require_auth(st.issuer);
    eosio_assert(quantity.is_valid(), "invalid quantity");
    eosio_assert(quantity.amount > 0, "must issue positive quantity");

    eosio_assert(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
    eosio_assert(quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify(st, eosio::same_payer, [&](auto& s) {
        s.supply += quantity;
    });

    add_balance(st.issuer, quantity, st.issuer);
}

ACTION Token::transfer(name from, name to, asset quantity, string memo) {
    eosio_assert(from != to, "cannot transfer to self");
    require_auth(from);
    eosio_assert(is_account(to), "to account does not exist");
    auto sym = quantity.symbol.code().raw();
    stats statstable(_self, sym);
    const auto& st = statstable.get(sym);

    require_recipient(from);
    require_recipient(to);

    eosio_assert(quantity.is_valid(), "invalid quantity");
    eosio_assert(quantity.amount > 0, "must transfer positive quantity");
    eosio_assert(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
    eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

    auto payer = has_auth(to) ? to : from;

    sub_balance(from, quantity);
    add_balance(to, quantity, payer);
}

void Token::sub_balance(name owner, asset value) {
    accounts from_acnts(_self, owner.value);

    const auto& from = from_acnts.get(value.symbol.code().raw(), "no balance object found");
    eosio_assert(from.balance.amount >= value.amount, "overdrawn balance");

    from_acnts.modify(from, owner, [&](auto& a) {
        a.balance -= value;
    });
}

void Token::add_balance(name owner, asset value, name ram_payer) {
    accounts to_acnts(_self, owner.value);
    auto to = to_acnts.find(value.symbol.code().raw());
    if (to == to_acnts.end()) {
        to_acnts.emplace(ram_payer, [&](auto& a) {
            a.balance = value;
        });
    } else {
        to_acnts.modify(to, eosio::same_payer, [&](auto& a) {
            a.balance += value;
        });
    }
}

EOSIO_DISPATCH(Token, (create)(issue)(transfer))
