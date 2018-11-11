#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>

using namespace eosio;


const symbol hub_symbol = symbol("EOSHUB", 4);

struct [[eosio::table]] service {
    name owner;

    std::string name;
    std::string description;
    std::string url;

    auto primary_key() { return owner.value; }
};

typedef eosio::multi_index<name("services"), service> services_index;

struct [[eosio::table]] account {
    name owner;

    asset balance;
    asset staked_balance;

    uint64_t last_collection_time;

    auto primary_key() { return owner.value; }
};

typedef eosio::multi_index<name("accounts"), account> accounts_index;


class [[eosio::contract]] eoshub : public eosio::contract {

  public:
    using contract::contract;

    // regservice registers an eosaccount service listing (metadata about the service etc)
    [[eosio::action]] void regservice(name owner, std::string description, std::string url) {
        require_auth(owner);

        services_index services(_self, _code.value);
        services.emplace(owner, [&]( auto& svc ) {
            svc.owner = owner;
            svc.description = description;
            svc.url = url;
        });
    }

    // stake stakes a balance in eoshub allowing it to be used for api services
    [[eosio::action]] void stake(name owner, asset stakeAmount) {
        require_auth(owner);

        accounts_index accounts(_self, _code.value);
        auto itr = accounts.find(owner.value);

        eosio_assert(itr != accounts.end(), "account not found");
        eosio_assert(stakeAmount.symbol == itr->balance.symbol, "incorrect symbol");
        eosio_assert(stakeAmount.amount <= itr->balance.amount, "insufficient funds");        
    }

    // unstake unstakes balances allowing them to be withdrawn from the contract
    [[eosio::action]] void unstake() {
        //todo: timeout 1 month
    }

    // regapikey stakes a certain amount of eoshub with the service tied to an EOS public key
    [[eosio::action]] void regapikey(std::string key, asset delegateAmount, name service) {

    }

    // unregapikey unstakes an amount of eoshub with the service
    [[eosio::action]] void unregapikey(name client, asset delegateAmount, name service) {
        
    }

    // collectreward collects earnings from shares since account.lastCollectedBlock
    [[eosio::action]] void collectreward(name user) { 

    }

    // withdraw withdraws an amount of eoshub (from the unstaked balance) to a given account
    [[eosio::action]] void withdraw(name from, asset withdrawlAmount) { 
        require_auth(from);
        accounts_index accounts(_self, _code.value);

        auto itr = accounts.find(from.value);
        eosio_assert(itr != accounts.end(), "account not found");

        eosio_assert(withdrawlAmount.symbol == hub_symbol, "withdrawlAmount has incorrect symbol");
        eosio_assert(withdrawlAmount.is_valid(), "withdrawlAmount is not valid");
        eosio_assert(withdrawlAmount.amount > 0, "withdrawlAmount must be positive");

    }

    // depositinf notification _from_ the eoshub.token contract
    [[eosio::action]] void depositinf( name from, name to, asset quantity, std::string memo ) {
        if(from == _self || to != _self)
            return;

        eosio_assert(quantity.symbol == hub_symbol, "incorrect symbol");
        eosio_assert(quantity.is_valid(), "quantity is not valid");
        eosio_assert(quantity.amount > 0, "amount must be positive");

        accounts_index accounts(_self, _code.value);
        auto itr = accounts.find(from.value);

        // We don't currently have an account for this user, lets start one
        if (itr == accounts.end()) {
            accounts.emplace(from, [&](auto &a) {
                a.owner = from;
                a.balance = quantity;
                a.staked_balance = asset(0, hub_symbol);
                a.last_collection_time = now();
            });
            return;
        }

        // This account does exist, lets add its balance to our records
        eosio_assert(itr != accounts.end(), "account doesn't exist");
        accounts.modify(itr, from, [&](auto &a){
            a.balance += quantity
        });
    }
};

// Custom Dispatcher to handle deposit notifications into the token contract
extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        if(code==receiver)
        {
            switch(action)
            {
                EOSIO_DISPATCH_HELPER(eoshub, (regservice)(stake)(unstake)(regapikey)(unregapikey)(collectreward)(withdraw) )
            }
        }
        else if(code==name("eosio.token").value && action==name("transfer").value) {
            execute_action( name(receiver), name(code), &eoshub::depositinf );
        }
    }
};


