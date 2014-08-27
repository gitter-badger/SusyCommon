#ifndef SusyCommon_D3PDAna_h
#define SusyCommon_D3PDAna_h


#include <iostream>

#include "TSelector.h"
#include "TTree.h"

#include "GoodRunsLists/TGoodRunsList.h"
#include "GoodRunsLists/TGoodRunsListReader.h"
#include "SUSYTools/SUSYObjDef.h"
#include "SUSYTools/FakeMetEstimator.h"
#include "SUSYTools/SUSYCrossSection.h"
#include "SUSYTools/HforToolD3PD.h"
#include "PileupReweighting/TPileupReweighting.h"
#include "LeptonTruthTools/RecoTauMatch.h"
#include "ElectronPhotonSelectorTools/TElectronLikelihoodTool.h"


/* #ifdef USEPDFTOOL */
/* #include "MultiLep/PDFTool.h" */
/* #endif */

#include "SusyCommon/LeptonInfo.h"
#include "SusyNtuple/SusyDefs.h"
#include "D3PDReader/Event.h"

namespace Root
{
  class TElectronLikelihoodTool;
}

namespace D3PDReader
{
  class MuonD3PDObject;
  class ElectronD3PDObject;
  class TauD3PDObject;
  class JetD3PDObjectl;
}

/**

    D3PDAna - a class for performing object selections and event cleaning on susy d3pds

*/

namespace susy {
class D3PDAna : public TSelector
{

  public:
    D3PDAna();
    virtual ~D3PDAna();
    
    virtual Bool_t  Process(Long64_t entry);
    virtual void    Terminate();


    // Init is called every time a new TTree is attached
    virtual void    Init(TTree *tree) { m_event.ReadFrom(tree); }
    virtual void    SlaveBegin(TTree *tree);
    
    virtual Bool_t  Notify() { return kTRUE; } /// Called at the first entry of a new file in a chain
    virtual void    SlaveTerminate(){};
    /// Due to ROOT's stupid design, need to specify version >= 2 or the tree will not connect automatically
    virtual Int_t   Version() const { return 2; }
    virtual D3PDAna& setDebug(int debugLevel) { m_dbg = debugLevel; return *this; }
    /// access the default collection of muons from the D3PDReader
    /**
       By default this function returns a pointer to
       mu_staco. However, if we always call this function (rather than
       accessing directly m_event.mu_staco), one can decide to
       override this member function, and easily switch to another
       muon collection.
     */
    virtual D3PDReader::MuonD3PDObject* d3pdMuons();
    /// access the default collection of electrons from the D3PDReader
    /**
       By default returns m_event.el; for its motivation, see D3PDAna::d3pdMuons().
       \todo In this case there might be some ambiguity to be sorted out when calling SUSYObjDef::GetMET().
     */
    virtual D3PDReader::ElectronD3PDObject* d3pdElectrons();
    /// access the default collection of taus from the D3PDReader
    /**
       By default returns m_event.tau; for its motivation, see D3PDAna::d3pdMuons().
     */
    virtual D3PDReader::TauD3PDObject* d3pdTaus();
    /// access the default collection of jets from the D3PDReader
    /**
       By default returns m_event.jet_AntiKt4LCTopo; for its motivation, see D3PDAna::d3pdMuons().
       \todo In this case there might be some ambiguity to be sorted out when calling SUSYObjDef::GetMET().
     */
    virtual D3PDReader::JetD3PDObject* d3pdJets();


    //
    // Object selection
    // Selected leptons have kinematic and cleaning cuts (no overlap removal)
    // Baseline leptons = selected + overlap removed
    // 

    // Full object selection
    void selectObjects(SusyNtSys sys = NtSys_NOM){
      selectBaselineObjects(sys);
      selectSignalObjects();
      if(m_selectTruth) selectTruthObjects();
    }
    void selectBaselineObjects(SusyNtSys sys = NtSys_NOM);
    void selectSignalObjects();
    void performOverlapRemoval();
    void selectSignalPhotons();
    void selectTruthObjects();

    // MissingEt
    void buildMet(SusyNtSys sys = NtSys_NOM);

    // Clear object selection
    void clearObjects();


    //
    // Trigger - check matching for all baseline leptons
    //
    void resetTriggers(){
      m_evtTrigFlags = 0;
      m_eleTrigFlags.clear();
      m_muoTrigFlags.clear();
      m_tauTrigFlags.clear();
    }
    void matchTriggers(){
      fillEventTriggers();
      matchElectronTriggers();
      matchMuonTriggers();
      matchTauTriggers();
    }
    void fillEventTriggers();
    void matchElectronTriggers();
    bool matchElectronTrigger(const TLorentzVector &lv, std::vector<int>* trigBools);
    void matchMuonTriggers();
    bool matchMuonTrigger(const TLorentzVector &lv, std::vector<int>* trigBools);
    void matchTauTriggers();
    bool matchTauTrigger(const TLorentzVector &lv, std::vector<int>* trigBools);



    //
    // Event cleaning
    //

    // grl
    void setGRLFile(TString fileName) { m_grlFileName = fileName; }
    bool passGRL() { return m_isMC || m_grl.HasRunLumiBlock(m_event.eventinfo.RunNumber(), m_event.eventinfo.lbn()); }
    // incomplete TTC event veto
    bool passTTCVeto() { return (m_event.eventinfo.coreFlags() & 0x40000) == 0; }
    // Tile error
    bool passTileErr() { return m_isMC || (m_event.eventinfo.tileError()!=2); }
    // lar error
    bool passLarErr() { return m_isMC || (m_event.eventinfo.larError()!=2); }
    // lar hole veto
    bool passLarHoleVeto();
    // tile hot spot
    bool passTileHotSpot();
    // bad jet
    bool passBadJet();
    // good vertex
    bool passGoodVtx();
    // tile trip
    bool passTileTrip();
    // bad muon veto
    bool passBadMuon();
    // cosmic veto
    bool passCosmic();

    // Event level cleaning cuts
    void checkEventCleaning();
    // Object level cleaning cuts; these depend on sys
    void checkObjectCleaning();

    //
    // Event weighting
    //

    // Full event weight includes generator, xsec, pileup, and lumi weights.
    // Default weight uses A-E lumi.
    // You can supply a different integrated luminosity, 
    // but the the pileup weights will still correspond to A-E.
    float getEventWeight(float lumi = LUMI_A_E);

    // event weight (xsec*kfac) 
    float getXsecWeight();
    // lumi weight (lumi/sumw) normalized to 4.7/fb
    float getLumiWeight();
    // luminosity to normalize to (in 1/pb)
    void setLumi(float lumi) { m_lumi = lumi; }
    // sum of mc weights for sample
    void setSumw(float sumw) { m_sumw = sumw; }
    // user cross section, overrides susy cross section
    void setXsec(float xsec) { m_xsec = xsec; }
    // user cross section uncert
    void setErrXsec(float err) { m_errXsec = err; }

    // pileup weight for full dataset: currently A-L
    float getPileupWeight();
    float getPileupWeightUp();
    float getPileupWeightDown();
    // PDF reweighting of 7TeV -> 8TeV
    float getPDFWeight8TeV();

    // Lepton efficiency SF
    float getLepSF(const std::vector<LeptonInfo>& leptons);

    // BTag efficiency SF
    float getBTagSF(const std::vector<int>& jets);


    //
    // Utility methods
    //

    // calculate random run/lb numbers for MC
    void calcRandomRunLB();

    // Mass helpers
    //float Mll();
    //bool isZ();
    //bool hasZ();

    // HF overlap removal decision
    int getHFORDecision();

    // Count number of good vertices
    uint getNumGoodVtx();
    uint getNumGoodVtx2();

    // Match a reco jet to a truth jet
    bool matchTruthJet(int iJet);

    //
    // Running conditions
    //

    // Sample name - used to set isMC flag
    TString sample() { return m_sample; }
    void setSample(TString s) { m_sample = s; }

    // AF2 flag
    void setAF2(bool isAF2=true) { m_isAF2 = isAF2; }

    // Set MC Production flag
    void setMCProduction(MCProduction prod) { m_mcProd = prod; }

    // Set SUSY D3PD tag to know which branches are ok
    void setD3PDTag(D3PDTag tag) { m_d3pdTag = tag; }

    // Set sys run
    void setSys(bool sysOn){ m_sys = sysOn; };
    
    // Toggle photon selection
    void setSelectPhotons(bool doIt) { m_selectPhotons = doIt; }

    // Toggle tau selection and overlap removal
    void setSelectTaus(bool doIt) { m_selectTaus = doIt; }

    // Set-Get truth selection
    void setSelectTruthObjects(bool doIt) { m_selectTruth = doIt; }
    bool getSelectTruthObjects(         ) { return m_selectTruth; }

    // Set MET flavor - at the moment, only STVF and STVF_JVF are available.
    // Anything else will raise an error.
    void setMetFlavor(std::string metFlav);
    void setDoMetMuonCorrection(bool doMetMuCorr) { m_doMetMuCorr = doMetMuCorr; }
    void setDoMetFix(bool doMetFix) { m_doMetFix = doMetFix; }
    /// whether the options specified by the user are consistent with the event info
    /**
       This function should be called when the first event is being
       read. Leave it up to the user to decide whether aborting in
       case of inconsistent options (need to check that all the input
       branches from the D3PD are filled in correctly).
       In the class inheriting from D3PDAna, one should have:
       \code{.cpp}
       Process() {
           GetEntry(entry);
           if(!m_flagsHaveBeenChecked) {
               m_flagsAreConsistent = runningOptionsAreValid();
               m_flagsHaveBeenChecked=true;
           }
       ...
       }
       \endcode
     */
    bool runningOptionsAreValid();
    //void setUseMetMuons(bool useMetMu) { m_useMetMuons = useMetMu; }

    //
    // Event dumps
    //
    void dumpEvent();
    void dumpPreObjects();
    void dumpBaselineObjects();
    void dumpSignalObjects();


  protected:

    TString                     m_sample;       // sample name
    DataStream                  m_stream;       // data stream enum, taken from sample name
    bool                        m_isAF2;        // flag for ATLFastII samples
    MCProduction                m_mcProd;       // MC production campaign

    bool                        m_isSusySample; // is susy grid sample
    int                         m_susyFinalState;// susy subprocess

    D3PDTag                     m_d3pdTag;      // SUSY D3PD tag

    bool                        m_selectPhotons;// Toggle photon selection
    bool                        m_selectTaus;   // Toggle tau selection and overlap removal
    bool                        m_selectTruth;  // Toggle truth selection

    SUSYMet::met_definition     m_metFlavor;    // MET flavor enum (e.g. STVF, STVF_JVF)
    bool                        m_doMetMuCorr;  // Control MET muon Eloss correction in SUSYTools
    bool                        m_doMetFix;     // Control MET Egamma-jet overlap fix in SUSYTools
    //bool                      m_useMetMuons;  // Use appropriate muons for met

    //
    // Object collections (usually just vectors of indices)
    //

    // "container" objects pass minimal selection cuts
    std::vector<int>            m_contTaus;     // container taus

    // "selected" objects pass kinematic cuts, but no overlap removal applied
    std::vector<int>            m_preElectrons; // selected electrons
    std::vector<int>            m_preMuons;     // selected muons
    std::vector<LeptonInfo>     m_preLeptons;   // selected leptons
    std::vector<int>            m_preJets;      // selected jets
    std::vector<int>            m_preTaus;      // selected taus
    std::vector<int>            m_metMuons;     // selected muons with larger eta cut for met calc.
    // no pre muons because we save all without overlap removal. So we skip this vector.
    
    // "baseline" objects pass selection + overlap removal
    std::vector<int>            m_baseElectrons;// baseline electrons
    std::vector<int>            m_baseMuons;    // baseline muons
    std::vector<LeptonInfo>     m_baseLeptons;  // baseline leptonInfos
    std::vector<int>            m_baseTaus;     // baseline taus
    std::vector<int>            m_basePhotons;  // baseline photons
    std::vector<int>            m_baseJets;     // baseline jets

    // "signal" objects pass baseline + signal selection (like iso)
    std::vector<int>            m_sigElectrons; // signal electrons
    std::vector<int>            m_sigMuons;     // signal muons
    std::vector<LeptonInfo>     m_sigLeptons;   // signal leptonInfos
    std::vector<int>            m_sigTaus;      // signal taus
    std::vector<int>            m_sigPhotons;   // signal photons
    std::vector<int>            m_sigJets;      // signal jets

    // MET
    TLorentzVector              m_met;          // fully corrected MET

    // Truth Objects
    std::vector<int>            m_truParticles; // selected truth particles
    std::vector<int>            m_truJets;      // selected truth jets
    TLorentzVector              m_truMet;       // Truth MET

    long long                   m_evtTrigFlags; // Event trigger flags
    
    // Trigger object matching maps
    // Key: d3pd index, Val: trig bit word
    std::map<int, long long>    m_eleTrigFlags; // electron trigger matching flags
    std::map<int, long long>    m_muoTrigFlags; // muon trigger matching flags
    std::map<int, long long>    m_tauTrigFlags; // tau trigger matching flags
    
    //
    // Event quantities
    //

    float                       m_lumi;         // normalized luminosity (defaults to 4.7/fb)
    float                       m_sumw;         // sum of mc weights for normalization, must be set by user
    float                       m_xsec;         // optional user cross section, to override susy xsec usage
    float                       m_errXsec;      // user cross section uncertainty

    uint                        m_mcRun;        // Random run number for MC from pileup tool
    uint                        m_mcLB;         // Random lb number for MC from pileup tool

    bool                        m_sys;          // True if you want sys for MC, must be set by user. 

    uint                        m_cutFlags;     // Event cleaning cut flags

    //
    // Tools
    //

    SUSYObjDef                  m_susyObj;      // SUSY object definitions
    Root::TElectronEfficiencyCorrectionTool* m_eleMediumSFTool;
    Root::TElectronLikelihoodTool* m_electron_lh_tool;

    TString                     m_grlFileName;  // grl file name
    Root::TGoodRunsList         m_grl;          // good runs list

    FakeMetEstimator            m_fakeMetEst;   // fake met estimator for lar hole veto

    Root::TPileupReweighting*   m_pileup;       // pileup reweighting
    Root::TPileupReweighting*   m_pileup_up;    // pileup reweighting
    Root::TPileupReweighting*   m_pileup_dn;    // pileup reweighting

    // The SUSY CrossSectionDB has its own map for retrieving xsec info, but
    // it has a lot of entries so lookup is slow.  Save our own xsec map

    SUSY::CrossSectionDB*                       m_susyXsec;     // SUSY cross section database
    std::map<int,SUSY::CrossSectionDB::Process> m_xsecMap;      // our own xsec map for faster lookup times

    HforToolD3PD                m_hforTool;     // heavy flavor overlap removal tool

    #ifdef USEPDFTOOL
    PDFTool*                    m_pdfTool;      // PDF reweighting tool (In MultiLep pkg)
    #endif

    RecoTauMatch                m_recoTruthMatch;       // Lepton truth matching tool

    // stuff imported from SusyD3PDInterface
    TTree* m_tree;              // Current tree
    Long64_t m_entry;           // Current entry in the current tree (not chain index!)
    int m_dbg;                  // debug level
    bool m_isMC;                // is MC flag
    bool m_flagsAreConsistent;  ///< whether the cmd-line flags are consistent with the event
    bool m_flagsHaveBeenChecked;///< whether the cmd-line have been checked

    D3PDReader::Event m_event;

};

} // susy

#endif
