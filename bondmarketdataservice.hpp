#ifndef BOND_MARKET_DATA_SERVICE_HPP
#define BOND_MARKET_DATA_SERVICE_HPP

#include <string>
#include <vector>
#include "soa.hpp"
#include "marketdataservice.hpp"
#include "products.hpp"


class BondMarketDataService : public MarketDataService<Bond>
{
private:
    std::map<std::string, OrderBook<Bond>> orderbooks;
    std::vector<ServiceListener<OrderBook<Bond>>*> listeners;
public:

    BondMarketDataService() {};
  
    // Get the best bid/offer order
    BidOffer GetBestBidOffer(const string &productId) override {
        auto it = orderbooks.find(productId);
        if (it != orderbooks.end()) {
            return BidOffer(it->second.GetBidStack()[0], it->second.GetOfferStack()[0]);
        }
        throw std::runtime_error("Order book not found for product: " + productId);
    }

    // Aggregate the order book
    const OrderBook<Bond>& AggregateDepth(const string &productId) override {
        auto it = orderbooks.find(productId);
        if (it != orderbooks.end()) {
            return it->second;
        }
        throw std::runtime_error("Order book not found for product: " + productId);
    }

    OrderBook<Bond>& GetData(string productId) override {
        auto it = orderbooks.find(productId);
        if (it != orderbooks.end()) {
            return it->second;
        }
        throw std::runtime_error("Order book not found for product: " + productId);
    }
    
    void OnMessage(OrderBook<Bond> &orderbook) override {
        orderbooks.insert_or_assign(orderbook.GetProduct().GetProductId(), orderbook);
        for (auto listener : listeners) {
            listener->ProcessAdd(orderbook);
        }
    }

    void AddListener(ServiceListener<OrderBook<Bond>> *listener) override {
        listeners.push_back(listener);
    }
    const vector<ServiceListener<OrderBook<Bond>>*>& GetListeners() const override {
        return listeners;
    }

};

#endif