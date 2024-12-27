#include "soa.hpp"
#include "products.hpp"
#include "pricingservice.hpp"
#include "bondpricingservice.hpp"
#include "streamingservice.hpp"
#include <string>

#ifndef BONDALGOSTREAMINGSERVICE_HPP
#define BONDALGOSTREAMINGSERVICE_HPP

// Add forward declarations for both classes
class BondAlgoStreamingService;
class BondAlgoStreamingServiceListener;


template<typename T>
class AlgoStream
{
public:// Constructor matching how it's used in ProcessAdd
    AlgoStream(const T& product, const PriceStreamOrder& bidOrder, const PriceStreamOrder& offerOrder) :
        priceStream(product, bidOrder, offerOrder) {}

    // Keep existing constructor too if needed
    AlgoStream(const PriceStream<T>& _priceStream) : priceStream(_priceStream) {}

    // Get the price stream
    const PriceStream<T>& GetPriceStream() const { return priceStream; }

    string to_string() const {
        return priceStream.to_string();
    }

private:
    PriceStream<T> priceStream;
};

class BondAlgoStreamingServiceListener : public ServiceListener<Price<Bond>>
{
private:
    BondAlgoStreamingService* bondAlgoStreamingService;

public:
    BondAlgoStreamingServiceListener(BondAlgoStreamingService* service) : 
        bondAlgoStreamingService(service) {}

    // Process an add event to the Service
    void ProcessAdd(Price<Bond>& data) override;
    
    // Process a remove event to the Service
    void ProcessRemove(Price<Bond>& data) override {};
    
    // Process an update event to the Service
    void ProcessUpdate(Price<Bond>& data) override {};
};

class BondAlgoStreamingService
{
private:
    map<string, AlgoStream<Bond>> algoStreams;
    vector<ServiceListener<AlgoStream<Bond>>*> listeners;
    BondPricingService* bondPricingService;
    BondAlgoStreamingServiceListener* listener;

public:
    BondAlgoStreamingService(BondPricingService* _bondPricingService) : 
        bondPricingService(_bondPricingService)        
    {
        listener = new BondAlgoStreamingServiceListener(this);
        bondPricingService->AddListener(listener);
    }

    // Get data on our service given a key
    AlgoStream<Bond>& GetData(string key) 
    {
        auto it = algoStreams.find(key);
        if (it != algoStreams.end()) {
            return it->second;
        }
        throw std::runtime_error("AlgoStream not found for key: " + key);
    }

    // Add a new algo stream
    void AddAlgoStream(const string& key, const AlgoStream<Bond>& algoStream) 
    {
        algoStreams.insert_or_assign(key, algoStream);
        
        // Notify all listeners
        for (auto& listener : listeners) {
            listener->ProcessAdd(algoStreams.find(key)->second);
        }
    }

    // Add a listener to the Service
    void AddListener(ServiceListener<AlgoStream<Bond>>* listener) 
    {
        listeners.push_back(listener);
    }

    // Get all listeners
    const vector<ServiceListener<AlgoStream<Bond>>*>& GetListeners() const 
    {
        return listeners;
    }

    ~BondAlgoStreamingService() 
    {
        delete listener;
    }
};

void BondAlgoStreamingServiceListener::ProcessAdd(Price<Bond>& price) 
{
    // Create a price stream from the price
    Bond product = price.GetProduct();
    double mid = price.GetMid();
    double spread = price.GetBidOfferSpread();
    
    // Calculate bid/offer prices and quantities
    double bidPrice = mid - spread/2.0;
    double offerPrice = mid + spread/2.0;
    
    // Static variable to track alternating state
    static bool alternateState = false;
    
    // Alternate between 1M and 2M for visible quantity
    long quantity = alternateState ? 2000000 : 1000000;
    // Alternate between 2M and 4M for hidden quantity
    long hiddenQuantity = alternateState ? 4000000 : 2000000;
    
    // Toggle the state for next time
    alternateState = !alternateState;

    const PriceStreamOrder bidOrder(bidPrice, quantity, hiddenQuantity, BID);
    const PriceStreamOrder offerOrder(offerPrice, quantity, hiddenQuantity, OFFER);
    //PriceStream<Bond> priceStream(product, bidOrder, offerOrder);
    //AlgoStream<Bond> algoStream(priceStream);
    AlgoStream<Bond> algoStream(product, bidOrder, offerOrder);
    
    // Update the algo stream
    string productId = product.GetProductId();
    bondAlgoStreamingService->AddAlgoStream(productId, algoStream);
}

#endif