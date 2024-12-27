#include "soa.hpp"
#include "products.hpp"
#include "pricingservice.hpp"
#include "bondpricingservice.hpp"
#include "streamingservice.hpp"
#include <string>
#include "executionservice.hpp"
#include "bondmarketdataservice.hpp"
#include <iomanip>

#ifndef BONDALGOEXECUTIONSERVICE_HPP
#define BONDALGOEXECUTIONSERVICE_HPP

// Add forward declarations for both classes
class BondAlgoExecutionService;
class BondAlgoExecutionServiceListener;


template<typename T>
class AlgoExecution
{
public:// Constructor matching how it's used in ProcessAdd
    AlgoExecution(ExecutionOrder<T>& _executionOrder) : executionOrder(_executionOrder) {}

    // Get the execution order
    ExecutionOrder<T>& GetExecutionOrder() { return executionOrder; }

private:
    ExecutionOrder<T> executionOrder;
};

class BondAlgoExecutionServiceListener : public ServiceListener<OrderBook<Bond>>
{
private:
    BondAlgoExecutionService* bondAlgoExecutionService;

public:
    BondAlgoExecutionServiceListener(BondAlgoExecutionService* service) : 
        bondAlgoExecutionService(service) {}

    // Process an add event to the Service
    void ProcessAdd(OrderBook<Bond>& data) override;
    
    // Process a remove event to the Service
    void ProcessRemove(OrderBook<Bond>& data) override {};
    
    // Process an update event to the Service
    void ProcessUpdate(OrderBook<Bond>& data) override {};
};

class BondAlgoExecutionService
{
private:
    map<string, AlgoExecution<Bond>> algoExecutions;
    vector<ServiceListener<AlgoExecution<Bond>>*> listeners;
    BondMarketDataService* bondMarketDataService;
    BondAlgoExecutionServiceListener* listener;
    bool is_buy = true;
    int next_order_ID = 1;

public:
    BondAlgoExecutionService(BondMarketDataService* _bondMarketDataService) : 
        bondMarketDataService(_bondMarketDataService)        
    {
        listener = new BondAlgoExecutionServiceListener(this);
        bondMarketDataService->AddListener(listener);
    }

    // Get data on our service given a key
    AlgoExecution<Bond>& GetData(string key) 
    {
        auto it = algoExecutions.find(key);
        if (it != algoExecutions.end()) {
            return it->second;
        }
        throw std::runtime_error("AlgoExecution not found for key: " + key);
    }

    // Add a new algo stream
    void AddAlgoExecution(const string& key, const AlgoExecution<Bond>& algoExecution) 
    {
        algoExecutions.insert_or_assign(key, algoExecution);
        
        // Notify all listeners
        for (auto& listener : listeners) {
            listener->ProcessAdd(algoExecutions.find(key)->second);
        }
    }

    void AggressOnBook(OrderBook<Bond>& orderbook) {
        // Create an execution order from the orderbook data
        PricingSide side;
        double quantity = 0;
        double price = 0;
        if (is_buy) {
            vector<Order> offerStack = orderbook.GetOfferStack();
            quantity += offerStack[0].GetQuantity();
            price = offerStack[0].GetPrice();
            side = PricingSide::BID;
        }
        else {
            vector<Order> bidStack = orderbook.GetBidStack();
            quantity += bidStack[0].GetQuantity();
            price = bidStack[0].GetPrice();
            side = PricingSide::OFFER;
        }
        is_buy = !is_buy;
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(8) << next_order_ID;
        ExecutionOrder<Bond> order(orderbook.GetProduct(), side, oss.str(), 
            OrderType::MARKET, price, quantity, 0.0, oss.str(), false);
        AlgoExecution<Bond> algoExecution(order);
        next_order_ID++;
        AddAlgoExecution(orderbook.GetProduct().GetProductId(), algoExecution);
    }

    // Add a listener to the Service
    void AddListener(ServiceListener<AlgoExecution<Bond>>* listener) 
    {
        listeners.push_back(listener);
    }

    // Get all listeners
    const vector<ServiceListener<AlgoExecution<Bond>>*>& GetListeners() const 
    {
        return listeners;
    }

    ~BondAlgoExecutionService() 
    {
        delete listener;
    }
};

void BondAlgoExecutionServiceListener::ProcessAdd(OrderBook<Bond>& orderbook) 
{
    // Create a price stream from the price
    Bond product = orderbook.GetProduct();
    if (orderbook.GetBidStack().empty() || orderbook.GetOfferStack().empty()) {
        return;  // Cannot calculate spread with empty stacks
    }
    double spread = orderbook.GetOfferStack()[0].GetPrice() - orderbook.GetBidStack()[0].GetPrice();
    if (spread > 1.00001/128.0) { // Only execute if the spread is less than than 1/128
        return;
    }
    bondAlgoExecutionService->AggressOnBook(orderbook);
}

#endif