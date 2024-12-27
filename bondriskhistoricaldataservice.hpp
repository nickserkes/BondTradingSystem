#ifndef BOND_RISK_HISTORICAL_DATA_SERVICE_HPP
#define BOND_RISK_HISTORICAL_DATA_SERVICE_HPP

#include "soa.hpp"
#include "historicaldataservice.hpp"
#include "products.hpp"
#include "filewriterconnector.hpp"
#include "positionservice.hpp"
#include "bondriskservice.hpp"
#include "riskservice.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include "pricesocketreaderconnector.hpp"
using namespace std;

class BondRiskHistoricalDataServiceListener : public ServiceListener<PV01<Bond>> {
    private:
        HistoricalDataService<PV01<Bond>>* service;
    public:
        BondRiskHistoricalDataServiceListener(HistoricalDataService<PV01<Bond>>* service) : service(service) {}

        void ProcessAdd(PV01<Bond>& data) override {
            auto now = chrono::system_clock::now();
            auto time_now = chrono::system_clock::to_time_t(now);
            auto ms_part = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
            stringstream ss;
            ss << put_time(localtime(&time_now), "%H:%M:%S") << "."
               << setfill('0') << setw(3) << ms_part;
            string timestamp = ss.str();
            service->PersistData(timestamp, data);
        }
        void ProcessRemove(PV01<Bond>& data) override {}
        void ProcessUpdate(PV01<Bond>& data) override {}
};


class BondRiskHistoricalDataService : public HistoricalDataService<PV01<Bond>> {
private:
    FileWriterConnector connector;
    vector<ServiceListener<PV01<Bond>>*> listeners;
    map<string, BucketedSector<Bond>> sectors;
    BondRiskService* riskService;
    

public:
    BondRiskHistoricalDataService(const std::string& filename, BondRiskService* _riskService) : connector(filename, RISK), riskService(_riskService) {
        std::map<std::string, Bond> bondMap = GetBondMap();
        const vector<string> FrontEnd = {"91282CLY5", "91282CMB4"};
        const vector<string> Belly = {"91282CMA6", "91282CLZ2", "91282CLW9"};
        const vector<string> LongEnd = {"912810UF3","912810UE6"};
        vector<Bond> FrontEndBonds;
        vector<Bond> BellyBonds;
        vector<Bond> LongEndBonds;
        for (auto& bond : bondMap) {
            if (std::find(FrontEnd.begin(), FrontEnd.end(), bond.second.GetProductId()) != FrontEnd.end()) {
                FrontEndBonds.push_back(bond.second);
            }
            else if (std::find(Belly.begin(), Belly.end(), bond.second.GetProductId()) != Belly.end()) {
                BellyBonds.push_back(bond.second);
            }
            else if (std::find(LongEnd.begin(), LongEnd.end(), bond.second.GetProductId()) != LongEnd.end()) {
                LongEndBonds.push_back(bond.second);
            }
        }
        BucketedSector<Bond> FrontEndSector(FrontEndBonds, "FrontEnd");
        BucketedSector<Bond> BellySector(BellyBonds, "Belly");
        BucketedSector<Bond> LongEndSector(LongEndBonds, "LongEnd");
        for (auto& cusip : FrontEnd) {
            sectors.insert({cusip, FrontEndSector});
        }
        for (auto& cusip : Belly) {
            sectors.insert({cusip, BellySector});
        }
        for (auto& cusip : LongEnd) {
            sectors.insert({cusip, LongEndSector});
        }
    }
    
    void OnMessage(PV01<Bond>& data) override {
        for (auto& listener : listeners) {
            listener->ProcessAdd(data);
        }
    }

    void PersistData(const std::string key, const PV01<Bond>& data) override {
        const BucketedSector<Bond>& sector = sectors.at(data.GetProduct().GetProductId());
        double SectorPV01 = riskService->GetBucketedRisk(sector).GetPV01();
        string persistData = key + "," + data.to_string() + "," + sector.GetName() + "," + std::to_string(SectorPV01);
        connector.Publish(persistData);
    }

    void AddListener(ServiceListener<PV01<Bond>>* listener) override{
        listeners.push_back(listener);
    }

    const vector<ServiceListener<PV01<Bond>>*>& GetListeners() const override {
        return listeners;
    }
    
    PV01<Bond>& GetData(string key) override { throw std::runtime_error("Not implemented"); }
    
};

#endif