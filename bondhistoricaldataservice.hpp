#ifndef BOND_HISTORICAL_DATA_SERVICE_HPP
#define BOND_HISTORICAL_DATA_SERVICE_HPP

#include "soa.hpp"
#include "historicaldataservice.hpp"
#include "products.hpp"
#include "filewriterconnector.hpp"
#include "positionservice.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

using namespace std;
template<typename T>
class BondHistoricalDataServiceListener : public ServiceListener<T> {
    private:
        HistoricalDataService<T>* service;
    public:
        BondHistoricalDataServiceListener(HistoricalDataService<T>* service) : service(service) {}
        void ProcessAdd(T& data) override {
            auto now = chrono::system_clock::now();
            auto time_now = chrono::system_clock::to_time_t(now);
            auto ms_part = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
            stringstream ss;
            ss << put_time(localtime(&time_now), "%H:%M:%S") << "."
               << setfill('0') << setw(3) << ms_part;
            string timestamp = ss.str();
            service->PersistData(timestamp, data);
        }
        void ProcessRemove(T& data) override {}
        void ProcessUpdate(T& data) override {}
};


template<typename T>
class BondHistoricalDataService : public HistoricalDataService<T> {
private:
    FileWriterConnector connector;
    vector<ServiceListener<T>*> listeners;
    

public:
    BondHistoricalDataService(const std::string& filename, FileWriterConnectorType type) : connector(filename, type) {}
    
    void OnMessage(T& data) override {
        for (auto& listener : listeners) {
            listener->ProcessAdd(data);
        }
    }

    void PersistData(const std::string key, const T& data) override {
        string persistData = key + "," + data.to_string();
        connector.Publish(persistData);
    }

    void AddListener(ServiceListener<T>* listener) override{
        listeners.push_back(listener);
    }

    const vector<ServiceListener<T>*>& GetListeners() const override {
        return listeners;
    }

    T& GetData(string key) override { throw std::runtime_error("Not implemented"); }
    
};

#endif