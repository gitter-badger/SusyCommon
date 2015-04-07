// ------------------------------------------- //
// Class/Tool to handle trigger information    //
// stored in SusyNt                            //
//                                             //
// author: Daniel Antrim                       //
// date  : April 6 2015                        //
// ------------------------------------------- //


#include <map>

#include "TH1F.h"
#include "TFile.h"
#include "TChain.h"
#include <iostream>

#include "SusyNtuple/SusyDefs.h"
#include "SusyNtuple/SusyNt.h"
#include "SusyNtuple/SusyNtObject.h"

#include "SusyCommon/Trigger.h"


// ------------------------------------------- //
// Constructor                                 //
// ------------------------------------------- //
Trigger::Trigger(TChain* input_chain, bool dbg) :
    m_trigHisto(NULL)
{
    cout << " ------------------ " << endl;
    cout << "Initializing Trigger" << endl;
    cout << " ------------------ " << endl;
    m_trigHisto = static_cast<TH1F*>(input_chain->GetFile()->Get("trig"));
    m_triggerMap.clear();
    buildTriggerMap();
    cout << " ------------------ " << endl;
    m_dbg = dbg;
}
// ------------------------------------------- //
// Build trigger-map 
// ------------------------------------------- //
// BinLabels <---> trigger name
// BinNumber <---> trigger bit number

//void Trigger::buildTriggerMap(TChain* susyNt, bool dbg)
void Trigger::buildTriggerMap()
{
    for(int trigBin = 1; trigBin < m_trigHisto->GetNbinsX(); trigBin++) {
        string triggerChainName = m_trigHisto->GetXaxis()->GetBinLabel(trigBin);
        m_triggerMap[triggerChainName] = trigBin-1;
        
        if(m_dbg) {
            cout << "Trigger " << triggerChainName << " at bit " << m_triggerMap[triggerChainName] << endl;
        }
    }
}

// ------------------------------------------- //
// Test whether a given trigger has fired
// ------------------------------------------- //
bool Trigger::passTrigger(TBits& triggerbits, std::string triggerName)
{
    if(m_triggerMap.find(triggerName)!=m_triggerMap.end()){
        return triggerbits.TestBitNumber(m_triggerMap[triggerName]);
    }
    else {
        std::cout << "Trigger " << triggerName << " not available!!" << std::endl;
        std::cout << "Dumping available triggers and exitting." << std::endl;
        dumpTriggerInfo();
        exit(1);
    }
//    std::map<std::string, int>::const_iterator trig_it = m_triggerMap.begin();
//    std::map<std::string, int>::const_iterator trig_end = m_triggerMap.end(); 
//    return triggerbits.TestBitNumber(m_triggerMap[triggerName]);
}

// ------------------------------------------- //
// Dump information about what triggers are
// stored in the SusyNt
// ------------------------------------------- //
void Trigger::dumpTriggerInfo()
{
   
    // remember: bit is stored as (bin # - 1) 
    cout << " // ---------------------------------- // " << endl;
    cout << "    Available triggers                    " << endl;
    cout << "    Name : Bit                            " << endl;
    for(int trigBin = 1; trigBin < m_trigHisto->GetNbinsX(); trigBin++) {
        string triggerChainName = m_trigHisto->GetXaxis()->GetBinLabel(trigBin);
        cout << "    " << m_trigHisto->GetXaxis()->GetBinLabel(trigBin) << " : " << trigBin-1 << endl;
    }
    cout << " // ---------------------------------- // " << endl;
}
        
    
    





