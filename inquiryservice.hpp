/**
 * inquiryservice.hpp
 * Defines the data types and Service for customer inquiries.
 *
 * @author Breman Thuraisingham
 */
#ifndef INQUIRY_SERVICE_HPP
#define INQUIRY_SERVICE_HPP

#include "soa.hpp"
#include "tradebookingservice.hpp"
#include "helperfunction.hpp"
// Various inqyury states
enum InquiryState { RECEIVED, QUOTED, DONE, REJECTED, CUSTOMER_REJECTED };

/**
 * Inquiry object modeling a customer inquiry from a client.
 * Type T is the product type.
 */
template<typename T>
class Inquiry
{

public:

  // ctor for an inquiry
  Inquiry(string _inquiryId, const T &_product, Side _side, long _quantity, double _price, InquiryState _state);

  // Add copy constructor
  Inquiry(const Inquiry& other) : 
      inquiryId(other.inquiryId),
      product(other.product),
      side(other.side),
      quantity(other.quantity),
      price(other.price),
      state(other.state) {}

  // Add default constructor
  Inquiry() : 
      inquiryId(""),
      product(T()),
      side(BUY),
      quantity(0),
      price(0.0),
      state(RECEIVED) {}

  // Get the inquiry ID
  const string& GetInquiryId() const;

  // Get the product
  const T& GetProduct() const;

  // Get the side on the inquiry
  Side GetSide() const;

  // Get the quantity that the client is inquiring for
  long GetQuantity() const;

  // Get the price that we have responded back with
  double GetPrice() const;

  double SetPrice(double _price);

  double ChangeState(InquiryState _state);

  // Get the current state on the inquiry
  InquiryState GetState() const;

  string to_string() const;

private:
  string inquiryId;
  T product;
  Side side;
  long quantity;
  double price;
  InquiryState state;

};

/**
 * Service for customer inquirry objects.
 * Keyed on inquiry identifier (NOTE: this is NOT a product identifier since each inquiry must be unique).
 * Type T is the product type.
 */
template<typename T>
class InquiryService : public Service<string,Inquiry <T> >
{

public:

  // Send a quote back to the client
  virtual void SendQuote(const string &inquiryId, double price) = 0;

  // Reject an inquiry from the client
  virtual void RejectInquiry(const string &inquiryId) = 0;

};

template<typename T>
Inquiry<T>::Inquiry(string _inquiryId, const T &_product, Side _side, long _quantity, double _price, InquiryState _state) :
  product(_product)
{
  inquiryId = _inquiryId;
  side = _side;
  quantity = _quantity;
  price = _price;
  state = _state;
}

template<typename T>
const string& Inquiry<T>::GetInquiryId() const
{
  return inquiryId;
}

template<typename T>
const T& Inquiry<T>::GetProduct() const
{
  return product;
}

template<typename T>
Side Inquiry<T>::GetSide() const
{
  return side;
}

template<typename T>
long Inquiry<T>::GetQuantity() const
{
  return quantity;
}

template<typename T>
double Inquiry<T>::GetPrice() const
{
  return price;
}

template<typename T>
double Inquiry<T>::SetPrice(double _price) {
    price = _price;
    return price;
}

template<typename T>
double Inquiry<T>::ChangeState(InquiryState _state) {
    state = _state;
    return 0.0;
}

template<typename T>
InquiryState Inquiry<T>::GetState() const
{
  return state;
}

template<typename T>
string Inquiry<T>::to_string() const{
  string sideString = side == Side::BUY ? "BUY" : "SELL";
  string stateString = state == RECEIVED ? "RECEIVED" : state == QUOTED ? "QUOTED" : state == DONE ? "DONE" : state == REJECTED ? "REJECTED" : state == CUSTOMER_REJECTED ? "CUSTOMER_REJECTED" : "UNKNOWN";
  return product.GetProductId() + "," + inquiryId + "," + sideString + "," + std::to_string(quantity) + "," + convert_to_fractional(price) + "," + stateString;
}

#endif
