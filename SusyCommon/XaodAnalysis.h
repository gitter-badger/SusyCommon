#ifndef SusyCommon_XaodAnalysis_h
#define SusyCommon_XaodAnalysis_h


// Infrastructure include(s):
#ifdef ROOTCORE
#   include "xAODRootAccess/Init.h"
#   include "xAODRootAccess/TEvent.h"
#endif // ROOTCORE

#include "GoodRunsLists/GoodRunsListSelectionTool.h"
#include "SUSYTools/SUSYObjDef_xAOD.h"
#include "LeptonTruthTools/RecoTauMatch.h"

#include "SusyCommon/LeptonInfo.h"
#include "SusyNtuple/SusyNt.h"
#include "SusyNtuple/SusyDefs.h"
#include "SusyNtuple/SusyNtSys.h"

//xAOD
#include "xAODEventInfo/EventInfo.h"
#include "xAODMuon/MuonContainer.h"
#include "xAODEgamma/ElectronContainer.h"
#include "xAODEgamma/PhotonContainer.h"
#include "xAODTau/TauJetContainer.h"
#include "xAODJet/JetContainer.h"
#include "xAODMissingET/MissingETContainer.h"
#include "xAODMissingET/MissingETAuxContainer.h"
#include "xAODTruth/TruthParticleContainer.h"
#include "xAODTruth/TruthEventContainer.h"
#include "TauAnalysisTools/TauTruthMatchingTool.h"
#include "TauAnalysisTools/TauTruthTrackMatchingTool.h"

#include "xAODTruth/TruthEvent.h"
#include "xAODCore/ShallowCopy.h"

//CP systematics
#include "PATInterfaces/SystematicList.h"
#include "PATInterfaces/SystematicVariation.h"
#include "PATInterfaces/SystematicRegistry.h"
#include "PATInterfaces/SystematicCode.h"


//Tools
#include "ElectronEfficiencyCorrection/AsgElectronEfficiencyCorrectionTool.h"
#include "PileupReweighting/PileupReweightingTool.h"
#include "MuonEfficiencyCorrections/MuonEfficiencyScaleFactors.h"
//#include "EventShapeTools/EventShapeCopier.h" // >> needed in rel20??

//Trigger
#include "TBits.h"
#include "TrigConfxAOD/xAODConfigTool.h"
#include "TrigDecisionTool/TrigDecisionTool.h"
#include "xAODTrigger/TrigNavigation.h"
#include "TrigConfHLTData/HLTTriggerElement.h"
#include "xAODTrigEgamma/TrigElectron.h"
#include "xAODTrigEgamma/TrigElectronContainer.h"

#include "SusyCommon/XaodAnalysis_types.h"

// Trigger
#include "TrigConfxAOD/xAODConfigTool.h"
#include "TrigDecisionTool/TrigDecisionTool.h"
#include "xAODTrigEgamma/TrigElectron.h"
#include "xAODTrigEgamma/TrigElectronContainer.h"

#include "SusyCommon/Trigger.h"


#include "TBits.h"

#include "TSelector.h"
#include "TTree.h"

#include <iostream>

using namespace Susy;
using namespace NtSys;

// dantrim trig
namespace TrigConf {
    class xAODConfigTool;
}
namespace Trig {
    class TrigDecisionTool;
    class FeatureContainer;
}


namespace susy {
  
  const double MeV2GeV=1.0e-3;

  enum eleID{
    Medium
    ,Tight
    ,LooseLLH
    ,MediumLLH
    ,TightLLH
    ,eleIDInvalid
  };
  
  const std::string eleIDNames[] = {
    "Medium"
    ,"Tight"
    ,"LooseLLH"
    ,"MediumLLH"
    ,"TightLLH"
    ,"eleIDInvalid"
  };

  ///  a class for performing object selections and event cleaning on xaod
  class XaodAnalysis : public TSelector
  {

  public:
    XaodAnalysis(); // original
    virtual ~XaodAnalysis();

    virtual Bool_t  Process(Long64_t entry);
    virtual void    Terminate();
    virtual void    Init(TTree *tree); ///< Init is called every time a new TTree is attached
    virtual void    SlaveBegin(TTree *tree);

    virtual Bool_t  Notify() { return kTRUE; } /// Called at the first entry of a new file in a chain
    virtual void    SlaveTerminate(){};
    /// Due to ROOT's stupid design, need to specify version >= 2 or the tree will not connect automatically
    virtual Int_t   Version() const { return 2; }
    virtual XaodAnalysis& setDebug(int debugLevel) { m_dbg = debugLevel; return *this; }
    void setTriggerSet(int set) { m_triggerSet = set; }
    XaodAnalysis& initSusyTools(); ///< initialize SUSYObjDef_xAOD
    bool processingMc12b() const { return m_mcProd == MCProd_MC12b; }
    
    /**
       Performance Tools 
    **/
    XaodAnalysis& initLocalTools(); ///< initialize performance tools
    void          initPileupTool();
    void          initMuonTools(); 
    void          initTauTools(); 
    void          initTrigger();
    
    // Systematic Methods
    void getSystematicList();

    /// access the event info
    /**
       \todo: all these xaod getters should be cached (if they are a bottleneck) DG-2014-08-16 to be checked
    */
    static const xAOD::EventInfo* retrieveEventInfo(xAOD::TEvent &e, bool dbg);
    /// wrapper of retrieveEventInfo; store result as datamember ptrs
    virtual const xAOD::EventInfo* xaodEventInfo();
    /// access the default collection of muons from SUSYObjDef_xAOD
    /**
       By default this function returns a pointer to
       mu_staco. However, if we always call this function (rather than
       accessing directly m_event.mu_staco), one can decide to
       override this member function, and easily switch to another
       muon collection.
    */
    virtual xAOD::MuonContainer* xaodMuons(ST::SystInfo sysInfo, SusyNtSys sys = NtSys::NOM);
    /// access the default collection of electrons from SUSYObjDef_xAOD
    /**
       By default returns m_event.el; for its motivation, see XaodAnalysis::xaodMuons().
    */
    virtual xAOD::ElectronContainer* xaodElectrons(ST::SystInfo sysInfo, SusyNtSys sys = NtSys::NOM);
    /// access the default collection of taus from the SUSYObjDef_xAOD
    /**
       By default returns m_event.tau; for its motivation, see XaodAnalysis::xaodMuons().
    */
    virtual xAOD::TauJetContainer* xaodTaus(ST::SystInfo sysInfo, SusyNtSys sys = NtSys::NOM);
    /// access the default collection of jets from the SUSYObjDef_xAOD
    /**
       By default returns m_event.jet_AntiKt4LCTopo; for its motivation, see XaodAnalysis::xaodMuons().
    */
    virtual std::vector<std::string> xaodTriggers();
    virtual xAOD::JetContainer* xaodJets(ST::SystInfo sysInfo, SusyNtSys sys = NtSys::NOM);
    /// access the default collection of photons from SUSYObjDef_xAOD
    virtual xAOD::PhotonContainer* xaodPhotons(ST::SystInfo sysInfo, SusyNtSys sys = NtSys::NOM);
    /// access the truth event
    static const xAOD::TruthEventContainer* retrieveTruthEvent(xAOD::TEvent &e, bool dbg);
    /// wrapper of retrieveTruthEvent; store outputs as datamembers
    virtual const xAOD::TruthEventContainer* xaodTruthEvent();

    /// access the truth particles
    static const xAOD::TruthParticleContainer* retrieveTruthParticles(xAOD::TEvent &e, bool dbg);
    /// wrapper of retrieveTruthParticles; store outputs as datamembers
    virtual const xAOD::TruthParticleContainer* xaodTruthParticles();

    /// retrieve & build met
    virtual void retrieveXaodMet(ST::SystInfo sysInfo, SusyNtSys sys = NtSys::NOM);

    /// access the vertices
    static const xAOD::VertexContainer* retrieveVertices(xAOD::TEvent &e, bool dbg);
    /// wrapper of retrieveVertices; store outputs as datamembers
    virtual const xAOD::VertexContainer* xaodVertices();

    virtual const xAOD::Vertex* getPV();

    /// retrieve all the input collections and cache pointers
    XaodAnalysis& retrieveCollections();

    /// fill collections of baseline+signal+truth objects
    /**
       Object selection
       Selected leptons have kinematic and cleaning cuts (no overlap removal)
       Baseline leptons = selected + overlap removed
    */
    void selectObjects(SusyNtSys sys, ST::SystInfo sysInfo);
    void selectBaselineObjects(SusyNtSys sys, ST::SystInfo sysInfo);
    void selectSignalObjects(SusyNtSys sys, ST::SystInfo sysInfo);
    void performOverlapRemoval();
    void selectSignalPhotons();
    void selectTruthObjects();


    // Clear object selection
    void clearOutputObjects(bool deleteNominal=true);


    //
    // Trigger - check matching for all baseline leptons
    //
    void resetTriggers(){
      m_evtTrigBits.ResetAllBits(); // dantrim trig
      m_eleTrigBits.clear();
      m_muoTrigBits.clear();
      m_evtTrigFlags = 0;
      m_eleTrigFlags.clear();
      m_muoTrigFlags.clear();
      m_tauTrigFlags.clear();
    }
    void matchTriggers(){
      fillEventTriggers();
//      matchElectronTriggers();
      matchMuonTriggers();
//      matchTauTriggers();
    }
    
    void fillEventTriggers();
    void matchElectronTriggers();
    bool matchElectronTrigger(const TLorentzVector* lv, std::string chain);
 //   bool matchElectronTrigger(const TLorentzVector &lv, std::vector<int>* trigBools);
    void matchMuonTriggers();
    bool matchMuonTrigger(const TLorentzVector& lv, std::string chain);
//    bool matchMuonTrigger(const TLorentzVector &lv, std::vector<int>* trigBools);
    void matchTauTriggers();
    bool matchTauTrigger(const TLorentzVector &lv, std::vector<int>* trigBools);



    //
    // Event cleaning
    //

    // grl
    XaodAnalysis& setGRLFile(TString fileName);
    static std::string defauldGrlFile();
    bool initGrlTool();
    bool passGRL(const xAOD::EventInfo* eventinfo); ///< good run list
    bool passTTCVeto(const xAOD::EventInfo* eventinfo); ///< incomplete event 
    bool passTileErr(const xAOD::EventInfo* eventinfo); ///< Tile error
    bool passLarErr(const xAOD::EventInfo* eventinfo); ///< lar error

    bool passLarHoleVeto(); ///< lar hole veto
    bool passTileHotSpot(); ///< tile hot spot
    bool passBadJet(ST::SystInfo sysInfo, SusyNtSys sys = NtSys::NOM); ///< bad jet
    bool passGoodVtx(); ///< good vertex
    bool passTileTrip(); ///< tile trip
    bool passBadMuon(ST::SystInfo sysInfo, SusyNtSys sys = NtSys::NOM); ///< bad muon veto
    bool passCosmic(ST::SystInfo sysInfo, SusyNtSys sys = NtSys::NOM); ///< cosmic veto

    void assignEventCleaningFlags(); ///< Event level cleaning cuts
    void assignObjectCleaningFlags(ST::SystInfo sysInfo, SusyNtSys sys = NtSys::NOM);// Object level cleaning cuts;  t/<hese depend on sys

    // Event weighting

    /// Full event weight includes generator, xsec, pileup, and lumi weights.
    /** Default weight uses A-E lumi.
        You can supply a different integrated luminosity,
        but the the pileup weights will still correspond to A-E.
    */
    float getEventWeight(float lumi = LUMI_A_E);
    float getXsecWeight(); ///< event weight (xsec*kfac)
    float getLumiWeight(); ///< lumi weight (lumi/sumw) normalized to 4.7/fb
    void setLumi(float lumi) { m_lumi = lumi; } ///< luminosity to normalize to (in 1/pb)
    void setSumw(float sumw) { m_sumw = sumw;  } ///< sum of mc weights for sample
    void setXsec(float xsec) { m_xsec = xsec;  } ///< user cross section, overrides susy cross section
    void setErrXsec(float err) { m_errXsec = err;  } ///< user cross section uncert
    float getPileupWeight(const xAOD::EventInfo* eventinfo); ///< pileup weight for full dataset: currently A-L
    float getPileupWeightUp();
    float getPileupWeightDown();
    float getPDFWeight8TeV(); ///< PDF reweighting of 7TeV -> 8TeV
    float getLepSF(const std::vector<LeptonInfo>& leptons); ///< Lepton efficiency SF
    float getBTagSF(const std::vector<int>& jets); ///< BTag efficiency SF

    // Utility methods
    void calcRandomRunLB(); ///< calculate random run/lb numbers for MC
    int getHFORDecision(); ///< HF overlap removal decision (DG obsolete?)
    uint getNumGoodVtx(); ///< Count number of good vertices
    bool matchTruthJet(int iJet); ///< Match a reco jet to a truth jet

    bool eleIsOfType(const xAOD::Electron &in, eleID id=Medium);


    // Running conditions
    TString sample() { return m_sample; } ///< Sample name - used to set isMC flag
    XaodAnalysis& setSample(TString s) { m_sample = s; return *this; }
    void setAF2(bool isAF2=true) { m_isAF2 = isAF2; } ///< AF2 flag
    void setMCProduction(MCProduction prod) { m_mcProd = prod; } ///< Set MC Production flag
    void setD3PDTag(D3PDTag tag) { m_d3pdTag = tag; } ///< Set SUSY D3PD tag to know which branches are ok
    void setSys(bool sysOn){ m_sys = sysOn; }; ///< Set sys run
    void setSelectPhotons(bool doIt) { m_selectPhotons = doIt; } ///< Toggle photon selection
    void setSelectTaus(bool doIt) { m_selectTaus = doIt; } ///< Toggle tau selection and overlap removal
    void setSelectTruthObjects(bool doIt) { m_selectTruth = doIt; } ///< Set-Get truth selection
    bool getSelectTruthObjects(         ) { return m_selectTruth; }
    void setMetFlavor(std::string metFlav); ///< only STVF and STVF_JVF are available (anything else will raise an error)
    void setDoMetMuonCorrection(bool doMetMuCorr) { m_doMetMuCorr = doMetMuCorr; }
    void setDoMetFix(bool doMetFix) { m_doMetFix = doMetFix; }
    /// whether the options specified by the user are consistent with the event info
    /**
       This function should be called when the first event is being
       read. Leave it up to the user to decide whether aborting in
       case of inconsistent options (need to check that all the input
       branches from the D3PD are filled in correctly).
       In the class inheriting from XaodAnalysis, one should have:
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
    void dumpBaselineObjects();
    void dumpSignalObjects();
    // helpers
    static DataStream streamFromSamplename(const TString &s, bool isdata); ///< guess data stream from sample name
    static bool isDataFromSamplename(const TString &s); ///< guess from sample name whether it's data sample
    static bool isSimuFromSamplename(const TString &s); ///< guess from sample name whether it's a simulated sample
    static bool isDerivationFromSamplename(const TString &sample); ///< guess from sample name whether it's a DxAOD


  protected:


    TString                     m_sample;       // sample name
    int                         m_triggerSet;   // trigger set to store
    std::vector<std::string>    m_triggerNames; 
    DataStream                  m_stream;       // data stream enum, taken from sample name
    bool                        m_isDerivation; // flag for derived xAOD (DxAOD)
    bool                        m_isAF2;        // flag for ATLFastII samples
    MCProduction                m_mcProd;       // MC production campaign

    bool                        m_isSusySample; // is susy grid sample
    int                         m_susyFinalState;// susy subprocess

    D3PDTag                     m_d3pdTag;      // SUSY D3PD tag

    bool                        m_selectPhotons;// Toggle photon selection
    bool                        m_selectTaus;   // Toggle tau selection and overlap removal
    bool                        m_selectTruth;  // Toggle truth selection

    /* SUSYMet::met_definition     m_metFlavor;    // MET flavor enum (e.g. STVF, STVF_JVF) */
    bool                        m_doMetMuCorr;  // Control MET muon Eloss correction in SUSYTools
    bool                        m_doMetFix;     // Control MET Egamma-jet overlap fix in SUSYTools
    //bool                      m_useMetMuons;  // Use appropriate muons for met

    //
    // Object collections (usually just vectors of indices)
    //

    // "container" objects pass minimal selection cuts
    std::vector<int>            m_contTaus;     // container taus

    // "selected" objects pass kinematic cuts, but no overlap removal applied
    // Store idx of object in xAOD container
    // AT-2014-11-5: Need to add check to make sure idx don't change when doing systematics.
    std::vector<int>            m_preElectrons; // selected electrons
    std::vector<int>            m_preMuons;     // selected muons
    std::vector<LeptonInfo>     m_preLeptons;   // selected leptons
    std::vector<int>            m_preJets;      // selected jets
    std::vector<int>            m_preTaus;      // selected taus
    std::vector<int>            m_metMuons;     // selected muons with larger eta cut for met calc.

    // "baseline" objects pass selection + overlap removal
    std::vector<int>            m_baseElectrons;// baseline electrons
    std::vector<int>            m_baseMuons;    // baseline muons
    std::vector<LeptonInfo>     m_baseLeptons;  // baseline leptonInfos
    std::vector<int>            m_baseTaus;     // baseline taus
    std::vector<int>            m_baseJets;     // baseline jets

    // "signal" objects pass baseline + signal selection (like iso)
    std::vector<int>            m_sigElectrons; // signal electrons
    std::vector<int>            m_sigMuons;     // signal muons
    std::vector<LeptonInfo>     m_sigLeptons;   // signal leptonInfos
    std::vector<int>            m_sigTaus;      // signal taus
    std::vector<int>            m_sigPhotons;   // signal photons
    std::vector<int>            m_sigJets;      // signal jets


    //Keep track of the nominal pre_object selected 
    //Use when doing systematics
    std::vector<int>            m_preElectrons_nom; // selected electrons
    std::vector<int>            m_preMuons_nom;     // selected muons
    std::vector<LeptonInfo>     m_preLeptons_nom;   // selected leptons
    std::vector<int>            m_preJets_nom;      // selected jets
    std::vector<int>            m_preTaus_nom;      // selected taus


    // MET
    TLorentzVector              m_met;          // fully corrected MET

    // Truth Objects
    std::vector<int>            m_truParticles; // selected truth particles
    std::vector<int>            m_truJets;      // selected truth jets
    TLorentzVector              m_truMet;       // Truth MET

    long long                   m_evtTrigFlags; // Event trigger flags


    // Trigger object matching maps
    // Key: d3pd index, Val: trig bit word
    std::map<int, TBits>        m_eleTrigBits;
    std::map<int, TBits>        m_muoTrigBits;
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

    /* Root::TElectronEfficiencyCorrectionTool* m_eleMediumSFTool; */

    TString                     m_grlFileName;  // grl file name
    GoodRunsListSelectionTool*   m_grl;         // good runs list

    /* Root::TPileupReweighting*   m_pileup;       // pileup reweighting */
    /* Root::TPileupReweighting*   m_pileup_up;    // pileup reweighting */
    /* Root::TPileupReweighting*   m_pileup_dn;    // pileup reweighting */

    // The SUSY CrossSectionDB has its own map for retrieving xsec info, but
    // it has a lot of entries so lookup is slow.  Save our own xsec map
    /* SUSY::CrossSectionDB*                       m_susyXsec;     // SUSY cross section database */
    /* std::map<int,SUSY::CrossSectionDB::Process> m_xsecMap;      // our own xsec map for faster lookup times */

    RecoTauMatch                m_recoTruthMatch;       // Lepton truth matching tool

    TTree* m_tree;              // Current tree
    Long64_t m_entry;           // Current entry in the current tree (not chain index!)
    int m_dbg;                  // debug level
    bool m_isMC;                // is MC flag
    bool m_flagsAreConsistent;  ///< whether the cmd-line flags are consistent with the event
    bool m_flagsHaveBeenChecked;///< whether the cmd-line have been checked

    xAOD::TEvent m_event;
    xAOD::TStore m_store;
    ST::SUSYObjDef_xAOD* m_susyObj[eleIDInvalid];      // SUSY object definitions
    eleID m_eleIDDefault;

    std::vector<CP::SystematicSet> sysList;  //CP Systematic list
    std::vector<ST::SystInfo> systInfoList;  //SUSYTools simplify version

    /// internal pointers to the xaod containers
    /**
       should proably make these private and force the user to call
       the @c xaod* functions.
    */
    const xAOD::EventInfo* m_xaodEventInfo;
    /**
       Note: one can only access collections as const. One can make
       modifiable shallow copies, but you have to remember to delete
       them at each event. (see SUSYToolsTester.cxx)
    */
    //Shallow copy containers
    //Contain the calibrated objects either at the nominal calibrated scale or after systematic variation
    xAOD::MuonContainer*                m_xaodMuons;
    xAOD::ShallowAuxContainer*          m_xaodMuonsAux; ///< muon aux info
    xAOD::ElectronContainer*            m_xaodElectrons;
    xAOD::ShallowAuxContainer*          m_xaodElectronsAux; ///< electron aux info
    xAOD::TauJetContainer*              m_xaodTaus;
    xAOD::ShallowAuxContainer*          m_xaodTausAux; ///< tau aux info
    xAOD::JetContainer*                 m_xaodJets;
    xAOD::ShallowAuxContainer*          m_xaodJetsAux; ///< jet aux info
    xAOD::JetContainer*                 m_metJets;     ///< calibrated jets used for met rebuilding //AT:11/12/14: Do we need this ???
    xAOD::ShallowAuxContainer*          m_metJetsAux;
    xAOD::PhotonContainer*              m_xaodPhotons;
    xAOD::ShallowAuxContainer*          m_xaodPhotonsAux; ///< photon aux info
    const xAOD::TruthEventContainer*    m_xaodTruthEvent;
    const xAOD::TruthParticleContainer* m_xaodTruthParticles;
    /// met container
    /**
       DG, note to self: not clear whether this is needed also when we
       don't want to write it out (just to recompute the met).
    */
    xAOD::MissingETContainer*           m_metContainer;
    xAOD::MissingETAuxContainer*        m_metAuxContainer;

    //VertexContainer
    const xAOD::VertexContainer*        m_xaodVertices; 

    //
    //Containers at the nominal scale. Needed when performing systematics variations
    // May be there is a nicer way of doing this code wise.
    //
    xAOD::MuonContainer*                m_xaodMuons_nom;
    xAOD::ShallowAuxContainer*          m_xaodMuonsAux_nom; 
    xAOD::ElectronContainer*            m_xaodElectrons_nom;
    xAOD::ShallowAuxContainer*          m_xaodElectronsAux_nom; 
    xAOD::TauJetContainer*              m_xaodTaus_nom;
    xAOD::ShallowAuxContainer*          m_xaodTausAux_nom; 
    xAOD::JetContainer*                 m_xaodJets_nom;
    xAOD::ShallowAuxContainer*          m_xaodJetsAux_nom;
    xAOD::PhotonContainer*              m_xaodPhotons_nom;
    xAOD::ShallowAuxContainer*          m_xaodPhotonsAux_nom; 


    /// cleanup shallow copies and aux containers
    /**
       They are created when retrieving the collections with
       XaodAnalysis::xaodMuons etc; it's the user's responsibility to
       clean things up.
    */
    XaodAnalysis& deleteShallowCopies(bool deleteNominal=true);
    /// clear the internal pointers
    /**
       Note that for those containers for which we created shallow
       copies we need to call delete (see
       XaodAnalysis::deleteShallowCopies), but for the other ones we
       just need to reset the chached pointer.
    */
    XaodAnalysis& clearContainerPointers(bool deleteNominal=true);



    // Performance tools
    std::string maindir;

    AsgElectronEfficiencyCorrectionTool *m_electronEfficiencySFTool;
    CP::PileupReweightingTool           *m_pileupReweightingTool;

    CP::MuonEfficiencyScaleFactors      *m_muonEfficiencySFTool; 

    //Tau truth matchong tools
    TauAnalysisTools::TauTruthMatchingTool       *m_tauTruthMatchingTool;
    TauAnalysisTools::TauTruthTrackMatchingTool  *m_tauTruthTrackMatchingTool;

    // dantrim trig
    TBits                       m_evtTrigBits;
    static const size_t         m_nTriggerBits=64;
    TH1F*                       hLevelPassed;
    TrigConf::xAODConfigTool*   m_configTool;
    Trig::TrigDecisionTool*     m_trigTool;

    // event shape copier
//    EventShapeCopier*           m_escopier; // >> needed in rel20??

  };

} // susy

#endif
