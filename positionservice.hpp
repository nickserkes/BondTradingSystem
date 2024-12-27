/**
 * positionservice.hpp
 * Defines the data types and Service for positions.
 *
 * @author Breman Thuraisingham
 */
#ifndef POSITION_SERVICE_HPP
#define POSITION_SERVICE_HPP

#include <string>
#include <map>
#include "soa.hpp"
#include "tradebookingservice.hpp"
#include "products.hpp"
using namespace std;

/**
 * Position class in a particular book.
 * Type T is the product type.
 */
template<typename T>
class Position
{

public:

  // ctor for a position
  Position(const T &_product);

  // Get the product
  const T& GetProduct() const;

  // Get the position quantity
  long GetPosition(string &book);

  long AddPosition(string &book, long quantity);

  // Get the aggregate position
  long GetAggregatePosition() const;

  // to_string
  string to_string() const;

private:
  T product;
  map<string,long> positions;

};

/**
 * Position Service to manage positions across multiple books and securties.
 * Keyed on product identifier.
 * Type T is the product type.
 */
template<typename T>
class PositionService : public Service<string,Position <T> >
{
public:  
  // Add a trade to the service
  virtual void AddTrade(const Trade<T> &trade) = 0;
};

template<typename T>
Position<T>::Position(const T &_product) :
  product(_product)
{
}

template<typename T>
const T& Position<T>::GetProduct() const
{
  return product;
}

template<typename T>
long Position<T>::GetPosition(string &book)
{
  if (positions.find(book) == positions.end()) {
    return 0;
  }
  return positions[book];
}

template<typename T>
long Position<T>::AddPosition(string &book, long quantity)
{
  if (positions.find(book) == positions.end()) {
    positions[book] = 0;
  }
  positions[book] += quantity;
  return positions[book];
}

template<typename T>
long Position<T>::GetAggregatePosition() const
{
  long aggregatePosition = 0;
  for (auto it = positions.begin(); it != positions.end(); ++it) {
    aggregatePosition += it->second;
  }
  return aggregatePosition;
}

template<typename T>
string Position<T>::to_string() const {
  stringstream ss;
  ss << product.GetProductId() << "," ;
  for (const auto& entry : positions) {
    ss << entry.first << "," << entry.second << ",";
  }
  ss << "Aggregate," <<GetAggregatePosition();
  return ss.str();
}

#endif
