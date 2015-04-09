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


#include "SusyCommon/Trigger.h"

using namespace std;

/////////////////////////////////////////////////
/////////////////////////////////////////////////
/////////////////////////////////////////////////

/* Trigger "containers"                        */
/*   Used at the SusyNt writing stage to set   */
/*   which triggers are stored in the final    */
/*   output ntuple.                            */

/////////////////////////////////////////////////
/////////////////////////////////////////////////
/////////////////////////////////////////////////


enum trigger {
    RUN_1=0,
    RUN_2=1
};

////////////
// Run 1
////////////
const std::vector<std::string> triggerNames_Run1 = {
    // 2012
    // electron triggers
    "EF_e7_medium1",
    "EF_e12Tvh_loose1",
    "EF_e12Tvh_medium1",
    "EF_e24vh_medium1",
    "EF_e24vhi_medium1",
    "EF_2e12Tvh_loose1",
    "EF_e24vh_medium1_e7_medium1",

    // muon triggers
    "EF_mu4T",
    "EF_mu6",
    "EF_mu8",
    "EF_mu13",
    "EF_mu18_tight",
    "EF_mu24",
    "EF_mu24i_tight",
    "EF_2mu6",
    "EF_2mu13",
    "EF_mu18_tight_mu8_EFFS",
    "EF_e12Tvh_medium1_mu8",
    "EF_mu18_tight_e7_medium1",
    "EF_3mu6",
    "EF_e18vh_medium1_2e7T_medium1",
    "EF_mu18_tight_2mu4_EFFS",
    "EF_2e7T_medium1_mu6",
    "EF_e7T_medium1_2mu6",
    
    // photon triggers
    "EF_g20_loose",
    "EF_g40_loose",
    "EF_g60_loose",
    "EF_g80_loose",
    "EF_g100_loose",
    "EF_g120_loose",

    // tau triggers
    "EF_tau20_medium1",
    "EF_tau20Ti_medium1",
    "EF_tau29Ti_medium1",
    "EF_tau29Ti_medium1_tau20Ti_medium1",
    "EF_tau20Ti_medium1_e18vh_medium1",
    "EF_tau20_medium1_mu15",

    // lep-tau matching
    "EF_e18vh_medium1",
    "EF_mu15",

    // MET trigger
    "EF_2mu8_EFxe40wMu_tclcw",
    "EF_xe80_tclcw_loose",
    
    // jet + met
    "EF_j110_aftchad_xe90_tclcw_loose",
    "EF_j80_a4tchad_xe100_tclcw_loose",
    "EF_j80_a4tchad_xe70_tclcw_dphi2j45xe10",

    // more triggers for ISR analysis
    "EF_mu4T_j65_a4tchad_xe70_tclcw_veryloose",
    "EF_2mu4T_xe60_tclcw",
    "EF_2mu8_EFxe40_tclcw",
    "EF_e24vh_medium1_EFxe35_tclcw",
    "EF_mu24_j65_a4tchad_EFxe40_tclcw",
    "EF_mu24_j65_a4tchad_EFxe40wMu_tclcw"
};

////////////
// Run 2
////////////
const std::vector<std::string> triggerNames_Run2 = {
    // DC14
    // electron triggers
    "HLT_e24_medium1_iloose",
    "HLT_e24_loose1",
    "HLT_e26_lhtight_iloose",
    "HLT_e28_tight1_iloose",
    "HLT_e60_loose1",
    "HLT_e60_medium1",
    "HLT_e60_lhmedium",

    // dielectron triggers
    "HLT_2e17_lhloose",

    // muon triggers
    "HLT_mu26_imedium",
    "HLT_mu50",
    "HLT_mu60_0eta105_msonly",
    "HLT_2mu4",
    "HLT_2mu6",
    "HLT_2mu10",
    "HLT_2mu14",
    "HLT_3mu6",
    
    // dimuon triggers
    "HLT_2mu14",

    // photon triggers
    "HLT_g120_loose1",
    "HLT_g140_loose1",
    
    // tau triggers
    "HLT_e18_loose1_tau25_medium1_calo",
    "HLT_e18_lhloose1_tau25_medium1_calo",
    "HLT_mu14_tau25_medium1_calo",
    "HLT_tau35_medium1_calo_tau25_medium1_calo",

    // met triggers
    "HLT_xe100",

    // jet triggers
    "HLT_j400",
    "HLT_3j175"
};

std::vector<std::string> getTrigNames(int run)
{ 
    if(run==0) { 
        std::cout << std::endl;
        std::cout << " ------------------------ " << std::endl;
        std::cout << " Storing Run1 trigger set " << std::endl;
        std::cout << " ------------------------ " << std::endl;
        std::cout << std::endl;
        return triggerNames_Run1;
    }
    else if(run==1) {
        std::cout << std::endl;
        std::cout << " ------------------------ " << std::endl;
        std::cout << " Storing Run2 trigger set " << std::endl;
        std::cout << " ------------------------ " << std::endl;
        std::cout << std::endl;
        return triggerNames_Run2;
    }
    else {
        std::cout << "getTrigNames -- Trigger names for requested run not available. Exitting." << std::endl;
        exit(1);
    }
}

// ---------- !! PARADIGM SHIFT !! ----------- //
// ---------- !! PARADIGM SHIFT !! ----------- //
// ---------- !! PARADIGM SHIFT !! ----------- //



/////////////////////////////////////////////////
/////////////////////////////////////////////////
/////////////////////////////////////////////////

/* Trigger tool                                */
/*    To be used at the analysis level of      */
/*    SusyNt                                   */

/////////////////////////////////////////////////
/////////////////////////////////////////////////
/////////////////////////////////////////////////

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
        
    
    





