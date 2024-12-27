#ifndef BOND_RISK_SERVICE_HPP
#define BOND_RISK_SERVICE_HPP

#include <cmath>
#include "riskservice.hpp"
#include "products.hpp"
#include "positionservice.hpp"
#include "soa.hpp"
#include "bondpositionservice.hpp"
#include "pricesocketreaderconnector.hpp"

using namespace std;

// Function to calculate bond price
double calculatePrice(double couponRate, double yield, int timeToMaturity, double faceValue = 100) {
    double price = 0.0;
    int periods = timeToMaturity * 2; // Semi-annual periods
    double semiAnnualYield = yield / 2;
    double semiAnnualCoupon = couponRate * faceValue / 2;

    // Sum of discounted coupon payments
    for (int t = 1; t <= periods; ++t) {
        price += semiAnnualCoupon / pow(1 + semiAnnualYield, t);
    }

    // Add discounted face value
    price += faceValue / pow(1 + semiAnnualYield, periods);

    return price;
}

// Function to calculate PV01 for a bond priced at 100, assuming yield = coupon rate
double calculatePV01(double couponRate, int timeToMaturity) {
    double faceValue = 100.0; // Bond priced at 100
    double yield = couponRate; // Assume yield equals coupon rate

    // Calculate the bond price at initial yield
    double initialPrice = calculatePrice(couponRate, yield, timeToMaturity, faceValue);

    // Adjust yield by 1 basis point (0.0001) and calculate new price
    double newPrice = calculatePrice(couponRate, yield + 0.0001, timeToMaturity, faceValue);

    // Calculate PV01 as the difference in price
    return initialPrice - newPrice;
}

/*
class BondPV01: public PV01<Bond> {
public:
    // Add default constructor
    BondPV01() : PV01<Bond>() {
        std::cerr << "BondPV01 default constructor called" << std::endl;
    }
    
    // Existing constructor
    BondPV01(const Bond& _product, double _pv01, long _quantity) 
        : PV01<Bond>(_product, _pv01, _quantity) {
        std::cerr << "BondPV01 constructor called" << std::endl;
        }
};
*/


class BondRiskServiceListener : public ServiceListener<Position<Bond>> {
private:
    RiskService<Bond>& bondRiskService;

public:
    BondRiskServiceListener(RiskService<Bond>& service) : bondRiskService(service) {}

    void ProcessAdd(Position<Bond>& data) override {
        bondRiskService.AddPosition(data);
    }

    void ProcessRemove(Position<Bond>& data) override {
        // No implementation needed
    }

    void ProcessUpdate(Position<Bond>& data) override {
        bondRiskService.AddPosition(data);
    }
};


class BondRiskService : public RiskService<Bond> {
    public:
    BondPositionService* bondPositionService;
    BondRiskServiceListener* listener;
    vector<ServiceListener<PV01<Bond>>*> listeners;
    map<string, PV01<Bond>> risk;
    map<string, double> pv01_lookup;

    BondRiskService(BondPositionService* _bondPositionService) : bondPositionService(_bondPositionService) {
        listener = new BondRiskServiceListener(*this);  
        bondPositionService->AddListener(listener);
    }

    ~BondRiskService() {
        delete listener;
    }

    void OnMessage(PV01<Bond>& data) override {}

    PV01<Bond>& GetData(string key) override {
        return risk.at(key);
    }

    const PV01<Bond>& GetData(string key) const {
        return risk.at(key);
    }

    PV01<BucketedSector<Bond>> GetBucketedRisk(const BucketedSector<Bond>& sector) const override {
        double pv01 = 0.0;
        for (const auto& bond : sector.GetProducts()) {
            if (risk.find(bond.GetProductId()) != risk.end()) {
                PV01<Bond> pv01_obj = GetData(bond.GetProductId());
                pv01 += pv01_obj.GetPV01()*pv01_obj.GetQuantity();
            }
        }
        BucketedSector<Bond> bucketedSector = sector;
        return PV01<BucketedSector<Bond>>(bucketedSector, pv01, 1);
    }

    // Add a listener to the service
    void AddListener(ServiceListener<PV01<Bond>>* listener) override {
        listeners.push_back(listener);
    }

    void AddPosition(Position<Bond>& position) override {
        string productId = position.GetProduct().GetProductId();
        long quantity = position.GetAggregatePosition();
        double pv01 = getPV01(position.GetProduct());
        
        // Create BondPV01 object first
        PV01<Bond> bondPv01(position.GetProduct(), pv01, quantity);
        
        // Use find and update pattern (works in all C++ versions)
        auto it = risk.find(productId);
        if (it != risk.end()) {
            it->second = bondPv01;  // Update existing
        } else {
            risk.insert(std::make_pair(productId, bondPv01));  // Insert new
        }
        
        for (auto& listener : listeners) {
            listener->ProcessAdd(risk.at(productId));
        }
    }

    double getPV01(const Bond& bond){
        if (pv01_lookup.find(bond.GetProductId()) == pv01_lookup.end()) {
            float coupon = bond.GetCoupon();
            date maturity = bond.GetMaturityDate();
            float time_to_maturity = maturity.year() - 2024;
            float pv01 = calculatePV01(coupon, time_to_maturity);
            pv01_lookup[bond.GetProductId()] = pv01;
        }
        return pv01_lookup[bond.GetProductId()];
    }


    const vector<ServiceListener<PV01<Bond>>*>& GetListeners() const override {
        return listeners;
    }
};

#endif // BOND_RISK_SERVICE_HPP