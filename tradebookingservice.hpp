/**
 * tradebookingservice.hpp
 * Defines the data types and Service for trade booking.
 *
 * @author Breman Thuraisingham
 */
#ifndef TRADE_BOOKING_SERVICE_HPP
#define TRADE_BOOKING_SERVICE_HPP

#include <string>
#include <vector>
#include "soa.hpp"
#include <map>
using namespace std;

// Trade sides
enum Side { BUY, SELL };

/**
 * Trade object with a price, side, and quantity on a particular book.
 * Type T is the product type.
 */
template<typename T>
class Trade
{

public:

  // ctor for a trade
  Trade(const T &_product, string _tradeId, double _price, string _book, long _quantity, Side _side);

  // Get the product
  const T& GetProduct() const;

  // Get the trade ID
  const string& GetTradeId() const;

  // Get the mid price
  double GetPrice() const;

  // Get the book
  const string& GetBook() const;

  // Get the quantity
  long GetQuantity() const;

  // Get the side
  Side GetSide() const;

private:
  T product;
  string tradeId;
  double price;
  string book;
  long quantity;
  Side side;

};

template<typename T>
class TradeBookingServiceListener : public ServiceListener<ExecutionOrder<T>> {
private:
    Service<string,Trade <T> >* service;
    vector<string> books = {"TRSY1", "TRSY2", "TRSY3"};
    int orderID = 0;
public:
    TradeBookingServiceListener(Service<string,Trade <T> >* _service) : service(_service) {}

    void ProcessAdd(ExecutionOrder<T>& data) override {
      T product = data.GetProduct();
      double price = data.GetPrice();
      string book = books[orderID%3];
      orderID++;
      string tradeId = 'E' + to_string(orderID);
      long quantity = data.GetVisibleQuantity() + data.GetHiddenQuantity();
      Side side = data.GetSide() == BID ? BUY : SELL;
      Trade<T> trade(product, tradeId, price, book, quantity, side);
      service->OnMessage(trade);
    }

    void ProcessRemove(ExecutionOrder<T>& data) override {}

    void ProcessUpdate(ExecutionOrder<T>& data) override {}
};

/**
 * Trade Booking Service to book trades to a particular book.
 * Keyed on trade id.
 * Type T is the product type.
 */
template<typename T>
class TradeBookingService : public Service<string,Trade<T>>
{
private:
  vector<ServiceListener<Trade<T>>*> listeners;
  map<string, Trade<T>>* trades;
  TradeBookingServiceListener<T>* listener;

public:
  TradeBookingService(){
    trades = new map<string, Trade<T>>();
    listener = new TradeBookingServiceListener<T>(this);
  }

  ~TradeBookingService() {
    delete trades;
  }

  // Book the trade
  void BookTrade(Trade<T> &trade) {
    trades->insert_or_assign(trade.GetTradeId(), trade);
    for (auto listener : listeners) {
      listener->ProcessAdd(trade);
    }
  }
  

  // Get the trade
  Trade<T>& GetTrade(const string& tradeId) {
    return trades->at(tradeId);
  }
  void OnMessage(Trade<T>& data) override {
    BookTrade(data);
  }

  void AddListener(ServiceListener<Trade<T>>* listener) override {
    listeners.push_back(listener);
  }

  const vector<ServiceListener<Trade<T>>*>& GetListeners() const override {
    return listeners;
  }

  Trade<T>& GetData(string key) override {
    return trades->at(key);
  }
};

template<typename T>
Trade<T>::Trade(const T &_product, string _tradeId, double _price, string _book, long _quantity, Side _side) :
  product(_product)
{
  tradeId = _tradeId;
  price = _price;
  book = _book;
  quantity = _quantity;
  side = _side;
}

template<typename T>
const T& Trade<T>::GetProduct() const
{
  return product;
}

template<typename T>
const string& Trade<T>::GetTradeId() const
{
  return tradeId;
}

template<typename T>
double Trade<T>::GetPrice() const
{
  return price;
}

template<typename T>
const string& Trade<T>::GetBook() const
{
  return book;
}

template<typename T>
long Trade<T>::GetQuantity() const
{
  return quantity;
}

template<typename T>
Side Trade<T>::GetSide() const
{
  return side;
}

#endif
