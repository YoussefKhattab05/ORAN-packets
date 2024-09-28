#include "EthernetPacket.h"
#include "Utils.h"
#include "eCPRIPacket.h"
#include "ORANPacket.h"
#include "IQSampleReader.h"
#include <sstream>
#include <iostream>
#include <iomanip> 

// Ethernet frame constants
const std::string PREAMBLE_SFD = "FB555555555555D5";
const uint32_t CRC_WIDTH = 8;
int packetnumber = 1;
EthernetPacket::EthernetPacket(const PacketConfig& config, FileWriter& fileWriter)
    : config(config), fileWriter(fileWriter), iqReader(config.payloadFile), seqId(0) {}

void EthernetPacket::startGenerating() {
    uint32_t totalPackets = (config.lineRate * 1000000) * config.captureSizeMs / (config.maxPacketSize * 8);
    std::cout << "Total packets to generate: " << totalPackets << std::endl;

    for (uint32_t i = 0; i < totalPackets; ++i) {
        generateSinglePacket();
    }
}

void EthernetPacket::generateSinglePacket() {
    
    std::stringstream packetStream;
    std::stringstream forCRC;
    packetStream << PREAMBLE_SFD;
    packetStream << config.destAddress.substr(2) << config.srcAddress.substr(2);
    packetStream << "0800";  // Ethernet type (customized for eCPRI)
    
    // Generate payload that includes the eCPRI header and O-RAN data
    std::string payload = generatePayload();
    packetStream << payload;

    forCRC << config.destAddress.substr(2) << config.srcAddress.substr(2);
    forCRC << "0800"; 
    forCRC << payload;
    std::string packetData = packetStream.str();
    std:: string Data_CRC  = forCRC.str();
    
    uint32_t crc = calculateCRC32(Data_CRC);
    packetStream << intToHex(crc, CRC_WIDTH);
    
    for (uint8_t i= 0; i<config.minNumOfIFGs; i++)
    {
        packetStream << "07";
    }

    std::string finalPacket = padTo4ByteAlignment(packetStream.str());

    // Debug output
    std::cout << "Packet number "<< packetnumber<< ": " << finalPacket << std::endl;
    
    // Write packet to file
    fileWriter.write(finalPacket);
    packetnumber++;
}

std::string EthernetPacket::generatePayload() {
    // Read IQ samples for O-RAN packet generation
    std::vector<std::pair<int, int>> iqSamples;
    std::string iqSampleLine;

int samplesNeeded = config.nrbPerPacket; // Number of IQ samples needed for this packet

// Read exactly the number of IQ samples required
for (int i = 0; i < samplesNeeded; ++i) {
    iqSampleLine = iqReader.getNextIqSample();

    // If we reach EOF or no more data, reset the reader to loop back to the beginning
    if (iqSampleLine == "00") {
        iqReader.reset(); // This will rewind the file (make sure the reset method is in IQSampleReader class)
        iqSampleLine = iqReader.getNextIqSample(); // Read again after resetting
    }

    std::istringstream iss(iqSampleLine);
    int iSample, qSample;
    if (iss >> iSample >> qSample) {
        iqSamples.emplace_back(iSample, qSample);  // Add sample to the vector
    }
}

// Now proceed with creating the ORAN packet and eCPRI header as usual

    // Create ORAN packet with the IQ samples
    ORANPacket oranPacket(config.nrbPerPacket); // Ensure nrbPerPacket is defined in your config
    std::vector<uint8_t> oranPayload = oranPacket.generatePacket(iqSamples);
    
    // Create eCPRI packet including ORAN payload
    eCPRIPacket eCPRI;
    std::vector<uint8_t> eCPRIHeader = eCPRI.generatePacket(oranPayload, seqId++); // Increment sequence ID
    
    // Convert eCPRIHeader to string for Ethernet payload
    std::stringstream payloadStream;
    for (uint8_t byte : eCPRIHeader) {
        payloadStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    std::string payload = payloadStream.str();
    return payload;
}
