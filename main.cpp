#include <iostream>
#include "executionservice.hpp"
#include "historicaldataservice.hpp"
#include "inquiryservice.hpp"
#include "marketdataservice.hpp"
#include "positionservice.hpp"
#include "pricingservice.hpp"
#include "products.hpp"
#include "riskservice.hpp"
#include "soa.hpp"
#include "streamingservice.hpp"
#include "tradebookingservice.hpp"
#include "pricesocketreaderconnector.hpp"
#include "filereaderconnector.hpp"
#include "bondpricingservice.hpp"
#include "bondalgostreamingservice.hpp"
#include "bondstreamingservice.hpp"
#include "guiservice.hpp"
#include "socketreaderconnector.hpp"
#include "bondhistoricaldataservice.hpp"
#include "bondpositionservice.hpp"
#include "bondriskservice.hpp"
#include "bondmarketdataservice.hpp"
#include "bondalgoexecutionservice.hpp"
#include "bondexecutionservice.hpp"
#include "bondinquiryservice.hpp"
#include "marketdatasocketreaderconnector.hpp"
#include "tradesocketreaderconnector.hpp"
#include "bondriskhistoricaldataservice.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <boost/date_time/gregorian/gregorian.hpp>

int main() {

    try {

        // Bond Price.txt Pipeline
        
        BondPricingService bondPricingService;   
        GUIService guiService(&bondPricingService);
        BondAlgoStreamingService bondAlgoStreamingService(&bondPricingService);
        BondStreamingService bondStreamingService(&bondAlgoStreamingService);
        BondHistoricalDataService<AlgoStream<Bond>> bondStreamingHistoricalDataService("streaming.txt", STREAMING);
        BondHistoricalDataServiceListener<AlgoStream<Bond>> bondStreamingHistoricalDataServiceListener(&bondStreamingHistoricalDataService);
        bondStreamingService.AddListener(&bondStreamingHistoricalDataServiceListener);
        PricesSocketReaderConnector pricesSocketReader(8080, &bondPricingService);
        pricesSocketReader.StartListening();
        FileReaderConnector pricesFileReader("miniprices.txt", "127.0.0.1", 8080);

        // Bond Trade.txt Pipeline
        TradeBookingService<Bond> tradeBookingService;
        BondPositionService bondPositionService(&tradeBookingService);
        BondHistoricalDataService<Position<Bond>> bondPositionHistoricalDataService("positions.txt", POSITIONS);
        BondHistoricalDataServiceListener<Position<Bond>> bondPositionHistoricalDataServiceListener(&bondPositionHistoricalDataService);
        bondPositionService.AddListener(&bondPositionHistoricalDataServiceListener);
        BondRiskService bondRiskService(&bondPositionService);
        BondRiskHistoricalDataService bondRiskHistoricalDataService("risk.txt", &bondRiskService);
        BondRiskHistoricalDataServiceListener bondRiskHistoricalDataServiceListener(&bondRiskHistoricalDataService);
        bondRiskService.AddListener(&bondRiskHistoricalDataServiceListener);
        TradesSocketReaderConnector tradeSocketReader(8081, &tradeBookingService);
        tradeSocketReader.StartListening();
        FileReaderConnector tradesFileReader("trades.txt", "127.0.0.1", 8081);    

        
        // Bond MarketData.txt Pipeline with TradeBookingService
        BondMarketDataService bondMarketDataService;    
        BondAlgoExecutionService bondAlgoExecutionService(&bondMarketDataService);
        BondExecutionService bondExecutionService(&bondAlgoExecutionService, &bondMarketDataService);
        BondHistoricalDataService<ExecutionOrder<Bond>> bondHistoricalDataService("executions.txt", EXECUTIONS);
        BondHistoricalDataServiceListener<ExecutionOrder<Bond>> bondHistoricalDataServiceListener(&bondHistoricalDataService);
        bondExecutionService.AddListener(&bondHistoricalDataServiceListener);
        TradeBookingServiceListener<Bond> tradeBookingServiceListener(&tradeBookingService);
        bondExecutionService.AddListener(&tradeBookingServiceListener);
        MarketDataSocketReaderConnector marketDataSocketReader(8082, &bondMarketDataService);
        marketDataSocketReader.StartListening();
        FileReaderConnector marketDataFileReader("mini_market_data.txt", "127.0.0.1", 8082);
        
       
       BondInquiryService bondInquiryService;
       BondHistoricalDataService<Inquiry<Bond>> bondInquiryHistoricalDataService("all_inquiries.txt", INQUIRIES);
       BondHistoricalDataServiceListener<Inquiry<Bond>> bondInquiryHistoricalDataServiceListener(&bondInquiryHistoricalDataService);
       bondInquiryService.AddListener(&bondInquiryHistoricalDataServiceListener);
       InquirySocketReaderConnector inquirySocketReader(8083, &bondInquiryService);
       bondInquiryService.AddClientConnector(&inquirySocketReader);
       inquirySocketReader.StartListening();
       FileReaderConnector inquiriesFileReader("inquiries.txt", "127.0.0.1", 8083);
       
        // Start file reading in a separate thread
        
        
        std::thread pricesFileReaderThread([&pricesFileReader]() {
            try {
                pricesFileReader.ReadFromFileAndPublish();
            }
            catch (const std::exception& e) {
                std::cerr << "Error in file reader thread: " << e.what() << std::endl;
            }
        });
        
        std::thread tradesFileReaderThread([&tradesFileReader]() {
            try {
                tradesFileReader.ReadFromFileAndPublish();
            }
            catch (const std::exception& e) {
                std::cerr << "Error in file reader thread: " << e.what() << std::endl;
            }
        });

        
        std::thread marketDataFileReaderThread([&marketDataFileReader]() {
            try {
                marketDataFileReader.ReadFromFileAndPublish();
            }
            catch (const std::exception& e) {
                std::cerr << "Error in file reader thread: " << e.what() << std::endl;
            }
        });
        
        std::thread inquiriesFileReaderThread([&inquiriesFileReader]() {
            try {
                inquiriesFileReader.ReadFromFileAndPublish();
            }
            catch (const std::exception& e) {
                std::cerr << "Error in file reader thread: " << e.what() << std::endl;
            }
        });
        

        // Add a sleep to keep the main program running
        std::this_thread::sleep_for(std::chrono::seconds(5));  // Adjust time as needed

        
        if (pricesFileReaderThread.joinable()) {
            pricesFileReaderThread.join();
        }

        
        if (tradesFileReaderThread.joinable()) {
            tradesFileReaderThread.join();
        }

        
        if (marketDataFileReaderThread.joinable()) {
            marketDataFileReaderThread.join();
        }
        
        if (inquiriesFileReaderThread.joinable()) {
            inquiriesFileReaderThread.join();
        }
        

        // Keep the program running to process all data
        //std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
} 