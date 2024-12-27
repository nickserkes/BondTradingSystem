#include "products.hpp"
#include "positionservice.hpp"
#include "tradebookingservice.hpp"


#ifndef BOND_POSITION_SERVICE_HPP
#define BOND_POSITION_SERVICE_HPP

class BondPositionServiceListener : public ServiceListener<Trade<Bond>> {
private:
    PositionService<Bond>* service;

public:
    // Simplified constructor
    BondPositionServiceListener(PositionService<Bond>& positionService) : service(&positionService) {}

    // Simplified ProcessAdd - just forwards to service
    void ProcessAdd(Trade<Bond>& data) override {
        service->AddTrade(data);
    }

    // Process a remove event to the Service
    void ProcessRemove(Trade<Bond>& data) override {}

    // Process an update event to the Service
    void ProcessUpdate(Trade<Bond>& data) override {}

    ~BondPositionServiceListener() {}
};


class BondPositionService : public PositionService<Bond>
{
private:
  BondPositionServiceListener* listener;
  std::map<string, Position<Bond>>* positions;
  vector<ServiceListener<Position<Bond>>*> listeners;
  TradeBookingService<Bond>* tradeBookingService;
public:
  //CREATE CONSTRUCTOR WITH BOND ID AND POSITION IN EACH BOOK
  BondPositionService(TradeBookingService<Bond>* _tradeBookingService) :
    tradeBookingService(_tradeBookingService)
  {
    positions = new std::map<string, Position<Bond>>();
    listener = new BondPositionServiceListener(*this);
    tradeBookingService->AddListener(listener);
  }
  // Add a trade to the service
  void AddTrade(const Trade<Bond> &trade) override {
    string productId = trade.GetProduct().GetProductId();
    string book = trade.GetBook();
    Side side = trade.GetSide();
    long quantity = trade.GetQuantity();

    // Check if position exists, if not create it with the product from the trade
    if (positions->find(productId) == positions->end()) {
        positions->insert({productId, Position<Bond>(trade.GetProduct())});
    }

    if (side == BUY) {
      positions->at(productId).AddPosition(book, quantity);
    } else {
      positions->at(productId).AddPosition(book, -quantity);
    }
    for (auto& listener : listeners) {
            listener->ProcessAdd(positions->at(productId));
        }
  };
  
  void OnMessage(Position<Bond>& data) override {}

  Position<Bond>& GetPosition(const string& productId) {
    auto it = positions->find(productId);
    if (it != positions->end()) {    
      return it->second;
    }
    throw std::runtime_error("Position not found for productId: " + productId);
  }
  void AddListener(ServiceListener<Position<Bond>>* listener) override {
    listeners.push_back(listener);
  }

  const vector<ServiceListener<Position<Bond>>*>& GetListeners() const override {
    return listeners;
  }

  Position<Bond>& GetData(string key) override {
    return GetPosition(key);
  }

  ~BondPositionService() {
    delete positions;
    delete listener;
  }
};

#endif