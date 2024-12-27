#include "soa.hpp"
#include "pricingservice.hpp"
#include "products.hpp"
#include <iostream>

#ifndef BONDPRICINGSERVICE_HPP
#define BONDPRICINGSERVICE_HPP

class BondPricingService : public Service<string,Price<Bond> > {
public:
    BondPricingService() {}

    void OnMessage(Price<Bond>& data) override {
        prices[data.GetProduct().GetProductId()] = &data;
        for (auto listener : listeners) {
            listener->ProcessAdd(data);
        }
        return;
    }

    Price<Bond>& GetData(string key) override {
        if (prices.find(key) != prices.end()) {
            return *prices[key];
        }
        else {
            throw std::invalid_argument("Key not found");
        }
    }

    void AddListener(ServiceListener<Price<Bond>>* listener) override {
        listeners.push_back(listener);
    }

    const std::vector<ServiceListener<Price<Bond>>*>& GetListeners() const override {
        return listeners;
    }

    ~BondPricingService() {}

    private:
        std::map<std::string, Price<Bond>*> prices;
        std::vector<ServiceListener<Price<Bond>>*> listeners;
};

#endif