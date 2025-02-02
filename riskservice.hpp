/**
 * riskservice.hpp
 * Defines the data types and Service for fixed income risk.
 *
 * @author Breman Thuraisingham
 */
#ifndef RISK_SERVICE_HPP
#define RISK_SERVICE_HPP

#include "soa.hpp"
#include "positionservice.hpp"

/**
 * PV01 risk.
 * Type T is the product type.
 */
template<typename T>
class PV01
{

public:

  // ctor for a PV01 value
  PV01();
  PV01(const T &_product, double _pv01, long _quantity);

  // Get the product on this PV01 value
  const T& GetProduct() const;

  // Get the PV01 value
  double GetPV01() const;

  // Get the quantity that this risk value is associated with
  long GetQuantity() const;

  // Get the total risk value
  double GetTotalRisk() const;

  virtual string to_string() const {
    return GetProduct().GetProductId() + "," + std::to_string(GetPV01()) + "," + std::to_string(GetQuantity()) + "," + std::to_string(GetTotalRisk());
  }

private:
  T product;
  double pv01;
  long quantity;

};

/**
 * A bucket sector to bucket a group of securities.
 * We can then aggregate bucketed risk to this bucket.
 * Type T is the product type.
 */
template<typename T>
class BucketedSector
{

public:

  // ctor for a bucket sector
  BucketedSector(const vector<T> &_products, string _name);

  // Add copy constructor
  BucketedSector(const BucketedSector& other) : products(other.products), name(other.name) {}

  // Get the products associated with this bucket
  const vector<T>& GetProducts() const;

  // Get the name of the bucket
  const string& GetName() const;

  const string GetProductId() const;

private:
  vector<T> products;
  string name;

};

/**
 * Risk Service to vend out risk for a particular security and across a risk bucketed sector.
 * Keyed on product identifier.
 * Type T is the product type.
 */
template<typename T>
class RiskService : public Service<string,PV01 <T> >
{

public:

  // Add a position that the service will risk
  virtual void AddPosition(Position<T> &position) = 0;

  // Get the bucketed risk for the bucket sector
  virtual PV01<BucketedSector<T>> GetBucketedRisk(const BucketedSector<T>& sector) const = 0;

};

template<typename T>
PV01<T>::PV01() :
  product()
{
}

template<typename T>
PV01<T>::PV01(const T &_product, double _pv01, long _quantity) :
  product(_product)
{
  pv01 = _pv01;
  quantity = _quantity;
}

template<typename T>
const T& PV01<T>::GetProduct() const {
  return product;
}

template<typename T>
double PV01<T>::GetPV01() const {
  return pv01;
}

template<typename T>
long PV01<T>::GetQuantity() const {
  return quantity;
}

template<typename T>
double PV01<T>::GetTotalRisk() const {
  return pv01*quantity;
}

template<typename T>
BucketedSector<T>::BucketedSector(const vector<T>& _products, string _name) :
  products(_products)
{
  name = _name;
}

template<typename T>
const vector<T>& BucketedSector<T>::GetProducts() const
{
  return products;
}

template<typename T>
const string& BucketedSector<T>::GetName() const
{
  return name;
}

template<typename T>
const string BucketedSector<T>::GetProductId() const
{
  return "";
}

#endif