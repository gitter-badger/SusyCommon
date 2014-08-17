#include <limits>

#include "TSystem.h"

#include "SusyCommon/XaodAnalysis.h"
//#include "SusyCommon/get_object_functions.h"

// #include "xAODTruth/TruthParticleContainer.h"
// #include "xAODTruth/TruthEventContainer.h"
// #include "xAODTruth/TruthEvent.h"
// #include "xAODCore/ShallowCopy.h"
// #include "xAODMissingET/MissingETContainer.h"
// #include "xAODMissingET/MissingETAuxContainer.h"

// #include "egammaAnalysisUtils/egammaTriggerMatching.h"
// #include "D3PDReader/JetD3PDObject.h"

using namespace std;
using susy::XaodAnalysis;

const float GeV = 1000.0;

//----------------------------------------------------------
XaodAnalysis::XaodAnalysis() :
        m_sample(""),
        m_stream(Stream_Unknown),
        m_isAF2(false),
        m_mcProd(MCProd_Unknown),
        m_d3pdTag(D3PD_p1328),
        m_selectPhotons(false),
        m_selectTaus(false),
        m_selectTruth(false),
        m_metFlavor(SUSYMet::Default),
        m_doMetMuCorr(false),
        m_doMetFix(false),
        m_lumi(LUMI_A_E),
        m_sumw(1),
        m_xsec(-1),
        m_errXsec(-1),
        m_mcRun(0),
        m_mcLB(0),
        m_sys(false),
//        m_eleMediumSFTool(0),
//        m_pileup(0),
//        m_pileup_up(0),
//        m_pileup_dn(0),
//        m_susyXsec(0),
//        m_hforTool(),
        m_susyObj("SUSYObjDef_xAOD"),
        m_grl(NULL),
        m_tree(NULL),
        m_entry(0),
        m_dbg(0),
        m_isMC(false),
        m_flagsAreConsistent(false),
    m_flagsHaveBeenChecked(false),
    m_event(xAOD::TEvent::kClassAccess)
{
}
//----------------------------------------------------------
void XaodAnalysis::Init(TTree *tree)
{
   xAOD::Init("susy::XaodAnalysis");
   m_event.readFrom(tree);
   //xAOD::TStore store; // DG-2014-08-15 I think this is needed only to output xaod
   m_isMC = XaodAnalysis::isSimuFromSamplename(m_sample);
   bool isData = XaodAnalysis::isDataFromSamplename(m_sample);
   m_stream = XaodAnalysis::streamFromSamplename(m_sample, isData);
   initSusyTools();
}
//----------------------------------------------------------
XaodAnalysis::~XaodAnalysis()
{
    cout<<"~XaodAnalysis : todo"<<endl;
}
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::SlaveBegin(TTree *tree)
{
  if(m_dbg) cout << "XaodAnalysis::SlaveBegin" << endl;
  bool isData(!m_isMC);

#warning TElectronEfficiencyCorrectionTool not initialized
#warning fakemet_est tool not initialized
  if(isData){ initGrlTool(); }
  if(m_isMC){
#warning susy xsec tool not initialized
#warning pileup rew tool not initialized
  }
}


/*--------------------------------------------------------------------------------*/
// Main process loop function - This is just an example for testing
/*--------------------------------------------------------------------------------*/
Bool_t XaodAnalysis::Process(Long64_t entry)
{
  m_event.getEntry(entry);

  static Long64_t chainEntry = -1;
  chainEntry++;
  if(m_dbg || chainEntry%10000==0)
  {
      const xAOD::EventInfo* eventinfo = xaodEventInfo();
      cout<<"run "<<eventinfo->eventNumber()<<" event "<<eventinfo->runNumber()<<endl;
  }
  // Object selection
  clearObjects();
  selectObjects();
  buildMet();
  return kTRUE;
}

/*--------------------------------------------------------------------------------*/
// The Terminate() function is the last function to be called during
// a query. It always runs on the client, it can be used to present
// the results graphically or save the results to file.
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::Terminate()
{
  if(m_dbg) cout << "XaodAnalysis::Terminate" << endl;

  if(m_isMC){
    // delete m_susyXsec;
    // delete m_pileup;
    // delete m_pileup_up;
    // delete m_pileup_dn;
  }
}
//----------------------------------------------------------
XaodAnalysis& XaodAnalysis::initSusyTools()
{
  bool useLeptonTrigger = false;
  if(true /*m_dbg*/) m_susyObj.msg().setLevel( MSG::DEBUG); // DG-2014-08-15 temporarily toggle dbg always on
  m_susyObj.setProperty("IsData",          static_cast<int>(!m_isMC));
  m_susyObj.setProperty("IsAtlfast",       static_cast<int>(m_isAF2));
  m_susyObj.setProperty("IsMC12b",         static_cast<int>(processingMc12b()));
  m_susyObj.setProperty("UseLeptonTrigger",static_cast<int>(useLeptonTrigger));
  if(m_susyObj.initialize() != StatusCode::SUCCESS){
      cout<<"XaodAnalysis::initSusyTools: cannot intialize SUSYObjDef_xAOD..."<<endl
          <<"Exiting... "<<endl
          <<endl;
      exit(-1);
  }else if(m_dbg){
      cout<<"XaodAnalysis::initSusyTools: SUSYObjDef_xAOD initialized... "<<endl;
  }
  return *this;
}
//----------------------------------------------------------
const xAOD::EventInfo* XaodAnalysis::xaodEventInfo()
{
    const xAOD::EventInfo* evt = NULL;
    m_event.retrieve(evt, "EventInfo");
    if(m_dbg){
        if(evt) cout<<"XaodAnalysis::xaodEventInfo: retrieved"<<endl;
        else    cout<<"XaodAnalysis::xaodEventInfo: failed"<<endl;
    }
    return evt;
}
//----------------------------------------------------------
xAOD::MuonContainer* XaodAnalysis::xaodMuons()
{
    /*const*/ xAOD::MuonContainer* muo = 0;
    m_event.retrieve(muo, "Muons");
    //std::pair< xAOD::MuonContainer*, xAOD::ShallowAuxContainer* > muons_shallowCopy = xAOD::shallowCopyContainer( *muons );
    if(m_dbg){
        if(muo) cout<<"XaodAnalysis::xaodElectrons: retrieved "<<muo->size()<<endl;
        else    cout<<"XaodAnalysis::xaodElectrons: failed"<<endl;
    }
    return muo;
}
//----------------------------------------------------------
xAOD::ElectronContainer* XaodAnalysis::xaodElectrons()
{
    /*const*/ xAOD::ElectronContainer* ele = 0;
    m_event.retrieve(ele, "ElectronCollection");
    if(m_dbg){
        if(ele) cout<<"XaodAnalysis::xaodElectrons: retrieved "<<ele->size()<<endl;
        else    cout<<"XaodAnalysis::xaodElectrons: failed"<<endl;
    }
    return ele;
}
//----------------------------------------------------------
const xAOD::TauJetContainer* XaodAnalysis::xaodTaus()
{
    const xAOD::TauJetContainer* tau = 0;
    m_event.retrieve(tau, "TauRecContainer");
    //std::pair< xAOD::TauJetContainer*, xAOD::ShallowAuxContainer* > taus_shallowCopy = xAOD::shallowCopyContainer( *taus );
    if(m_dbg){
        if(tau) cout<<"XaodAnalysis::xaodTaus: retrieved "<<tau->size()<<endl;
        else    cout<<"XaodAnalysis::xaodTaus: failed"<<endl;
    }
    return tau;
}
//----------------------------------------------------------
const xAOD::JetContainer* XaodAnalysis::xaodJets()
{
    const xAOD::JetContainer* jet = 0;
    m_event.retrieve(jet, "AntiKt4LCTopoJets");
    if(m_dbg){
        if(jet) cout<<"XaodAnalysis::xaodJets: retrieved "<<jet->size()<<endl;
        else    cout<<"XaodAnalysis::xaodJets: failed"<<endl;
    }
    //std::pair< xAOD::JetContainer*, xAOD::ShallowAuxContainer* > jets_shallowCopy = xAOD::shallowCopyContainer( *jets );
    return jet;
}
//----------------------------------------------------------
const xAOD::PhotonContainer* XaodAnalysis::xaodPhothons()
{
    const xAOD::PhotonContainer* photons = 0;
    m_event.retrieve(photons, "PhotonCollection");
    // std::pair< xAOD::PhotonContainer*, xAOD::ShallowAuxContainer* > photons_shallowCopy = xAOD::shallowCopyContainer( *photons );
    if(m_dbg){
        if(photons) cout<<"XaodAnalysis::xaodPhotons: retrieved "<<photons->size()<<endl;
        else        cout<<"XaodAnalysis::xaodPhotons: failed"<<endl;
    }
    return photons;
}
//----------------------------------------------------------
const xAOD::TruthEventContainer* XaodAnalysis::xaodTruthEvent()
{
    const xAOD::TruthEventContainer* truth = 0;
    m_event.retrieve(truth, "TruthEvent");
    if(m_dbg){
        if(truth) cout<<"XaodAnalysis::xaodPhotons: retrieved "<<endl;
        else      cout<<"XaodAnalysis::xaodPhotons: failed"<<endl;
    }
    return truth;
}
//----------------------------------------------------------
const xAOD::TruthParticleContainer* XaodAnalysis::xaodTruthParticles()
{
    const xAOD::TruthParticleContainer* truthP = 0;
    m_event.retrieve(truthP, "TruthParticle");
    if(m_dbg){
        if(truthP) cout<<"XaodAnalysis::xaodTruthParticles: retrieved "<<truthP->size()<<endl;
        else       cout<<"XaodAnalysis::xaodTruthParticles: failed"<<endl;
    }
    return truthP;
}
//----------------------------------------------------------
void XaodAnalysis::clearShallowCopies()
{
/// \todo
/*
    delete jets_shallowCopy.first;
    delete jets_shallowCopy.second;
    delete taus_shallowCopy.first;
    delete taus_shallowCopy.second;
    delete muons_shallowCopy.first;
    delete muons_shallowCopy.second;
    delete electrons_shallowCopy.first;
    delete electrons_shallowCopy.second;
    delete photons_shallowCopy.first;
    delete photons_shallowCopy.second;
*/
}
//----------------------------------------------------------
SystErr::Syste ntsys2systerr(const SusyNtSys &s)
{
    SystErr::Syste sys = SystErr::NONE;    
    switch(s){
    case NtSys_NOM :                                     break; // No need to check needlessly
        //case NtSys_EES_UP : sys = SystErr::EESUP;        break; // E scale up
        //case NtSys_EES_DN : sys = SystErr::EESDOWN;      break; // E scale down
    case NtSys_EES_Z_UP   : sys = SystErr::EGZEEUP;    break; // E scale Zee up
    case NtSys_EES_Z_DN   : sys = SystErr::EGZEEDOWN;  break; // E scale Zee dn
    case NtSys_EES_MAT_UP : sys = SystErr::EGMATUP;    break; // E scale material up
    case NtSys_EES_MAT_DN : sys = SystErr::EGMATDOWN;  break; // E scale material down
    case NtSys_EES_PS_UP  : sys = SystErr::EGPSUP;     break; // E scale presampler up
    case NtSys_EES_PS_DN  : sys = SystErr::EGPSDOWN;   break; // E scale presampler down
    case NtSys_EES_LOW_UP : sys = SystErr::EGLOWUP;    break; // E low pt up
    case NtSys_EES_LOW_DN : sys = SystErr::EGLOWDOWN;  break; // E low pt down
    case NtSys_EER_UP     : sys = SystErr::EGRESUP;    break; // E smear up
    case NtSys_EER_DN     : sys = SystErr::EGRESDOWN;  break; // E smear down
    case NtSys_MS_UP      : sys = SystErr::MMSUP;      break; // MS scale up
    case NtSys_MS_DN      : sys = SystErr::MMSLOW;     break; // MS scale down
    case NtSys_ID_UP      : sys = SystErr::MIDUP;      break; // ID scale up
    case NtSys_ID_DN      : sys = SystErr::MIDLOW;     break; // ID scale down
    case NtSys_JES_UP     : sys = SystErr::JESUP;      break; // JES up
    case NtSys_JES_DN     : sys = SystErr::JESDOWN;    break; // JES down
    case NtSys_JER        : sys = SystErr::JER;        break; // JER (gaussian)        
    case NtSys_TES_UP     : sys = SystErr::TESUP;      break; // TES up
    case NtSys_TES_DN     : sys = SystErr::TESDOWN;    break; // TES down
    }
    return sys;
}
//----------------------------------------------------------
void XaodAnalysis::selectBaselineObjects(SusyNtSys sys)
{
  if(m_dbg>=5) cout << "selectBaselineObjects" << endl;
  vector<int> goodJets;  // What the hell is this??
  SystErr::Syste susySys = ntsys2systerr(sys);
  const xAOD::JetContainer *jets = xaodJets();
  // Container object selection
  //-DG-if(m_selectTaus) m_contTaus = get_taus_baseline(xaodTaus(), m_susyObj, 20.*GeV, 2.47,
  //-DG-                                                SUSYTau::TauNone, SUSYTau::TauNone, SUSYTau::TauNone,
  //-DG-                                                susySys, true);

  // Preselection
  //-DG-m_preElectrons = get_electrons_baseline(xaodElectrons(), &m_event.el_MET_Egamma10NoTau,
  //-DG-                                        !m_isMC, m_event.eventinfo.RunNumber(), m_susyObj,
  //-DG-                                        7.*GeV, 2.47, susySys);
  //-DG-m_preMuons = get_muons_baseline(xaodMuons(), !m_isMC, m_susyObj,
  //-DG-                                6.*GeV, 2.5, susySys);
  // Removing eta cut for baseline jets. This is for the bad jet veto.
  //-DG-m_preJets = get_jet_baseline(jets, &m_event.vxp, &m_event.eventinfo, &m_event.Eventshape, !m_isMC, m_susyObj,
  //-DG-                             20.*GeV, std::numeric_limits<float>::max(), susySys, false, goodJets);

  // Selection for met muons
  // Diff with preMuons is pt selection
  //-DG-m_metMuons = get_muons_baseline(xaodMuons(), !m_isMC, m_susyObj,
  //-DG-                                10.*GeV, 2.5, susySys);

  // Preselect taus
  //-DG-if(m_selectTaus) m_preTaus = get_taus_baseline(xaodTaus(), m_susyObj, 20.*GeV, 2.47,
  //-DG-                                               SUSYTau::TauLoose, SUSYTau::TauLoose, SUSYTau::TauLoose,
  //-DG-                                               susySys, true);
  //-DG-performOverlapRemoval();

  // combine leptons
  //-DG-m_preLeptons    = buildLeptonInfos(xaodElectrons(), m_preElectrons, xaodMuons(), m_preMuons, m_susyObj);
  //-DG-m_baseLeptons   = buildLeptonInfos(xaodElectrons(), m_baseElectrons, xaodMuons(), m_baseMuons, m_susyObj);
}

/*--------------------------------------------------------------------------------*/
// perform overlap
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::performOverlapRemoval()
{
  const xAOD::JetContainer *jets = xaodJets();
//-DG-  // e-e overlap removal
//-DG-  m_baseElectrons = overlap_removal(m_susyObj, xaodElectrons(), m_preElectrons, xaodElectrons(), m_preElectrons,
//-DG-                                    0.05, true, true);
//-DG-  // jet-e overlap removal
//-DG-  m_baseJets      = overlap_removal(m_susyObj, jets, m_preJets, xaodElectrons(), m_baseElectrons,
//-DG-                                    0.2, false, false);
//-DG-
//-DG-  if(m_selectTaus) {
//-DG-    // tau-e overlap removal
//-DG-    m_baseTaus    = overlap_removal(m_susyObj, xaodTaus(), m_preTaus, xaodElectrons(), m_baseElectrons, 0.2, false, false);
//-DG-    // tau-mu overlap removal
//-DG-    m_baseTaus    = overlap_removal(m_susyObj, xaodTaus(), m_baseTaus, xaodMuons(), m_preMuons, 0.2, false, false);
//-DG-  }
//-DG-
//-DG-  // e-jet overlap removal
//-DG-  m_baseElectrons = overlap_removal(m_susyObj, xaodElectrons(), m_baseElectrons, jets, m_baseJets,
//-DG-                                    0.4, false, false);
//-DG-
//-DG-  // m-jet overlap removal
//-DG-  m_baseMuons     = overlap_removal(m_susyObj, xaodMuons(), m_preMuons, jets, m_baseJets, 0.4, false, false);
//-DG-
//-DG-  // e-m overlap removal
//-DG-  vector<int> copyElectrons = m_baseElectrons;
//-DG-  m_baseElectrons = overlap_removal(m_susyObj, xaodElectrons(), m_baseElectrons, xaodMuons(), m_baseMuons,
//-DG-                                    0.01, false, false);
//-DG-  m_baseMuons     = overlap_removal(m_susyObj, xaodMuons(), m_baseMuons, xaodElectrons(), copyElectrons, 0.01, false, false);
//-DG-
//-DG-  // m-m overlap removal
//-DG-  m_baseMuons     = overlap_removal(m_susyObj, xaodMuons(), m_baseMuons, xaodMuons(), m_baseMuons, 0.05, true, false);
//-DG-
//-DG-  // jet-tau overlap removal
//-DG-  m_baseJets      = overlap_removal(m_susyObj, jets, m_baseJets, xaodTaus(), m_baseTaus, 0.2, false, false);
//-DG-
//-DG-  // remove SFOS lepton pairs with Mll < 12 GeV
//-DG-  m_baseElectrons = RemoveSFOSPair(m_susyObj, xaodElectrons(), m_baseElectrons, 12.*GeV);
//-DG-  m_baseMuons     = RemoveSFOSPair(m_susyObj, xaodMuons(), m_baseMuons,     12.*GeV);
//-DG-  //m_baseTaus      = RemoveSFOSPair(m_susyObj, xaodTaus(), m_baseTaus,      12.*GeV);
}

/*--------------------------------------------------------------------------------*/
// Signal object selection - do baseline selection first!
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::selectSignalObjects()
{
//-DG-  if(m_dbg>=5) cout << "selectSignalObjects" << endl;
//-DG-  uint nVtx = getNumGoodVtx();
//-DG-  xAOD::JetContainer *jets =  xaodJets();
//-DG-  m_sigElectrons = get_electrons_signal(xaodElectrons(), m_baseElectrons, xaodMuons(), m_baseMuons,
//-DG-                                        nVtx, !m_isMC, m_susyObj, 10.*GeV, 0.16, 0.18, 5., 0.4);
//-DG-  m_sigMuons     = get_muons_signal(xaodMuons(), m_baseMuons, xaodElectrons(), m_baseElectrons,
//-DG-                                    nVtx, !m_isMC, m_susyObj, 10.*GeV, .12, 3., 1.);
//-DG-  m_sigJets      = get_jet_signal(jets, m_susyObj, m_baseJets, 20.*GeV, 2.5, 0.75);
//-DG-  m_sigTaus      = get_taus_signal(xaodTaus(), m_baseTaus, m_susyObj);
//-DG-
//-DG-  // combine light leptons
//-DG-  m_sigLeptons   = buildLeptonInfos(xaodElectrons(), m_sigElectrons, xaodMuons(), m_sigMuons, m_susyObj);
//-DG-
//-DG-  // photon selection done in separate method, why?
//-DG-  if(m_selectPhotons) selectSignalPhotons();
}

/*--------------------------------------------------------------------------------*/
// Build MissingEt
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::buildMet(SusyNtSys sys)
{
//-DG-  if(m_dbg>=5) cout << "buildMet" << endl;
//-DG-
//-DG-  // Need the proper jet systematic for building systematic
//-DG-  SystErr::Syste susySys = SystErr::NONE;
//-DG-  if(sys == NtSys_NOM);
//-DG-  else if(sys == NtSys_JES_UP)      susySys = SystErr::JESUP;       // JES up
//-DG-  else if(sys == NtSys_JES_DN)      susySys = SystErr::JESDOWN;     // JES down
//-DG-  else if(sys == NtSys_JER)         susySys = SystErr::JER;         // JER (gaussian)
//-DG-  else if(sys == NtSys_SCALEST_UP)  susySys = SystErr::SCALESTUP;   // Met scale sys up
//-DG-  else if(sys == NtSys_SCALEST_DN)  susySys = SystErr::SCALESTDOWN; // Met scale sys down
//-DG-  // Only one of these now?
//-DG-  //else if(sys == NtSys_RESOST_UP)   susySys = SystErr::RESOSTUP;    // Met resolution sys up
//-DG-  //else if(sys == NtSys_RESOST_DN)   susySys = SystErr::RESOSTDOWN;  // Met resolution sys down
//-DG-  else if(sys == NtSys_RESOST)      susySys = SystErr::RESOST;      // Met resolution sys up
//-DG-
//-DG-  // Need electrons with nonzero met weight in order to calculate the MET
//-DG-  vector<int> metElectrons = get_electrons_met(&m_event.el_MET_Egamma10NoTau, m_susyObj);
//-DG-
//-DG-  // Calculate the MET
//-DG-  // We use the metMuons instead of preMuons so that we can have a lower pt cut on preMuons
//-DG-  TVector2 metVector =  m_susyObj.GetMET(m_event.jet_AntiKt4LCTopo_MET_Egamma10NoTau.wet(), m_event.jet_AntiKt4LCTopo_MET_Egamma10NoTau.wpx(),
//-DG-                                         m_event.jet_AntiKt4LCTopo_MET_Egamma10NoTau.wpy(), m_event.jet_AntiKt4LCTopo_MET_Egamma10NoTau.statusWord(),
//-DG-                                         metElectrons,
//-DG-                                         m_event.el_MET_Egamma10NoTau.wet(), m_event.el_MET_Egamma10NoTau.wpx(),
//-DG-                                         m_event.el_MET_Egamma10NoTau.wpy(), m_event.el_MET_Egamma10NoTau.statusWord(),
//-DG-                                         m_event.MET_CellOut_Egamma10NoTau.etx(),
//-DG-                                         m_event.MET_CellOut_Egamma10NoTau.ety(),
//-DG-                                         m_event.MET_CellOut_Egamma10NoTau.sumet(),
//-DG-                                         m_event.MET_CellOut_Eflow_STVF_Egamma10NoTau.etx(),
//-DG-                                         m_event.MET_CellOut_Eflow_STVF_Egamma10NoTau.ety(),
//-DG-                                         m_event.MET_CellOut_Eflow_STVF_Egamma10NoTau.sumet(),
//-DG-                                         m_event.MET_RefGamma_Egamma10NoTau.etx(),
//-DG-                                         m_event.MET_RefGamma_Egamma10NoTau.ety(),
//-DG-                                         m_event.MET_RefGamma_Egamma10NoTau.sumet(),
//-DG-                                         m_metMuons,
//-DG-                                         xaodMuons()->ms_qoverp(),
//-DG-                                         xaodMuons()->ms_theta(),
//-DG-                                         xaodMuons()->ms_phi(),
//-DG-                                         xaodMuons()->charge(),
//-DG-                                         xaodMuons()->energyLossPar(),
//-DG-                                         m_event.eventinfo.averageIntPerXing(),
//-DG-                                         m_metFlavor, susySys);
//-DG-  m_met.SetPxPyPzE(metVector.X(), metVector.Y(), 0, metVector.Mod());
}

/*--------------------------------------------------------------------------------*/
// Signal photons
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::selectSignalPhotons()
{
//-DG-  if(m_dbg>=5) cout << "selectSignalPhotons" << endl;
//-DG-
//-DG-  int phoQual = 2;      // Quality::Tight
//-DG-  uint isoType = 1;     // Corresponds to PTED corrected isolation
//-DG-  float etcone40CorrCut = 3*GeV;
//-DG-
//-DG-  vector<int> base_photons = get_photons_baseline(&m_event.ph, m_susyObj,
//-DG-                                                  20.*GeV, 2.47, SystErr::NONE, phoQual);
//-DG-
//-DG-  // Latest and Greatest
//-DG-  int nPV = getNumGoodVtx();
//-DG-  m_sigPhotons = get_photons_signal(&m_event.ph, base_photons, m_susyObj, nPV,
//-DG-                                    20.*GeV, etcone40CorrCut, isoType);
}
/*--------------------------------------------------------------------------------*/
// Truth object selection
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::selectTruthObjects()
{
//-DG-  if(m_dbg>=5) cout << "selectTruthObjects" << endl;
//-DG-
//-DG-  // ==>> First the truth particles
//-DG-  // Done under SusyNtMaker::fillTruthParticleVars
//-DG-
//-DG-  // ==>> Second the truth jets
//-DG-  for(int index=0; index < m_event.AntiKt4Truth.n(); index++) {
//-DG-      // const xAOD::JetD3PDObjectElement &trueJet = m_event.AntiKt4Truth[index];
//-DG-      // if( trueJet.pt()/GeV > 15. && fabs(trueJet.eta()) < 4.5) m_truJets.push_back(index);
//-DG-#warning truth not implemented
//-DG-  }
//-DG-
//-DG-  // ==>> Third and last the truth met
//-DG-  m_truMet.SetPxPyPzE(m_event.MET_Truth.NonInt_etx(), m_event.MET_Truth.NonInt_ety(), 0, m_event.MET_Truth.NonInt_sumet());
}

/*--------------------------------------------------------------------------------*/
// Clear selected objects
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::clearObjects()
{
  m_preElectrons.clear();
  m_preMuons.clear();
  m_preJets.clear();
  m_preLeptons.clear();
  m_baseElectrons.clear();
  m_baseMuons.clear();
  m_baseLeptons.clear();
  m_baseJets.clear();
  m_sigElectrons.clear();
  m_sigMuons.clear();
  m_sigLeptons.clear();
  m_sigJets.clear();
  m_cutFlags = 0;

  m_sigPhotons.clear();
  m_truParticles.clear();
  m_truJets.clear();

  m_metMuons.clear();
}

/*--------------------------------------------------------------------------------*/
// Count number of good vertices
/*--------------------------------------------------------------------------------*/
uint XaodAnalysis::getNumGoodVtx()
{
#warning getNumGoodVtx not implemented
    return 1;
  // uint nVtx = 0;
  // for(int i=0; i < m_event.vxp.n(); i++){
  //   if(m_event.vxp.nTracks()->at(i) >= 5) nVtx++;
  // }
  // return nVtx;
}

/*--------------------------------------------------------------------------------*/
// Match reco jet to a truth jet
/*--------------------------------------------------------------------------------*/
bool XaodAnalysis::matchTruthJet(int iJet)
{
#warning matchTruthJet not implemented
  // // Loop over truth jets looking for a match
  // const TLorentzVector &jetLV = m_susyObj.GetJetTLV(iJet);
  // for(int i=0; i<m_event.AntiKt4Truth.n(); i++){
  //   // const xAOD::JetD3PDObjectElement &trueJet = m_event.AntiKt4Truth[i];
  //   // TLorentzVector trueJetLV;
  //   // trueJetLV.SetPtEtaPhiE(trueJet.pt(), trueJet.eta(), trueJet.phi(), trueJet.E());
  //   // if(jetLV.DeltaR(trueJetLV) < 0.3) return true;
  //   }
  return false;
}

/*--------------------------------------------------------------------------------*/
// Event trigger flags
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::fillEventTriggers()
{
  if(m_dbg>=5) cout << "fillEventTriggers" << endl;

  m_evtTrigFlags = 0;
//-DG--  if(m_event.triggerbits.EF_e7T_medium1())                m_evtTrigFlags |= TRIG_e7_medium1;
//-DG--  if(m_event.triggerbits.EF_e12Tvh_loose1())              m_evtTrigFlags |= TRIG_e12Tvh_loose1;
//-DG--  if(m_event.triggerbits.EF_e12Tvh_medium1())             m_evtTrigFlags |= TRIG_e12Tvh_medium1;
//-DG--  if(m_event.triggerbits.EF_e24vh_medium1())              m_evtTrigFlags |= TRIG_e24vh_medium1;
//-DG--  if(m_event.triggerbits.EF_e24vhi_medium1())             m_evtTrigFlags |= TRIG_e24vhi_medium1;
//-DG--  if(m_event.triggerbits.EF_2e12Tvh_loose1())             m_evtTrigFlags |= TRIG_2e12Tvh_loose1;
//-DG--  if(m_event.triggerbits.EF_e24vh_medium1_e7_medium1())   m_evtTrigFlags |= TRIG_e24vh_medium1_e7_medium1;
//-DG--  if(m_event.triggerbits.EF_mu8())                        m_evtTrigFlags |= TRIG_mu8;
//-DG--  if(m_event.triggerbits.EF_mu13())                       m_evtTrigFlags |= TRIG_mu13;
//-DG--  if(m_event.triggerbits.EF_mu18_tight())                 m_evtTrigFlags |= TRIG_mu18_tight;
//-DG--  if(m_event.triggerbits.EF_mu24i_tight())                m_evtTrigFlags |= TRIG_mu24i_tight;
//-DG--  if(m_event.triggerbits.EF_2mu13())                      m_evtTrigFlags |= TRIG_2mu13;
//-DG--  if(m_event.triggerbits.EF_mu18_tight_mu8_EFFS())        m_evtTrigFlags |= TRIG_mu18_tight_mu8_EFFS;
//-DG--  if(m_event.triggerbits.EF_e12Tvh_medium1_mu8())         m_evtTrigFlags |= TRIG_e12Tvh_medium1_mu8;
//-DG--  if(m_event.triggerbits.EF_mu18_tight_e7_medium1())      m_evtTrigFlags |= TRIG_mu18_tight_e7_medium1;
//-DG--
//-DG--  if(m_event.triggerbits.EF_tau20_medium1())                   m_evtTrigFlags |= TRIG_tau20_medium1;
//-DG--  if(m_event.triggerbits.EF_tau20Ti_medium1())                 m_evtTrigFlags |= TRIG_tau20Ti_medium1;
//-DG--  if(m_event.triggerbits.EF_tau29Ti_medium1())                 m_evtTrigFlags |= TRIG_tau29Ti_medium1;
//-DG--  if(m_event.triggerbits.EF_tau29Ti_medium1_tau20Ti_medium1()) m_evtTrigFlags |= TRIG_tau29Ti_medium1_tau20Ti_medium1;
//-DG--  if(m_event.triggerbits.EF_tau20Ti_medium1_e18vh_medium1())   m_evtTrigFlags |= TRIG_tau20Ti_medium1_e18vh_medium1;
//-DG--  if(m_event.triggerbits.EF_tau20_medium1_mu15())              m_evtTrigFlags |= TRIG_tau20_medium1_mu15;
//-DG--
//-DG--  if(m_event.triggerbits.EF_e18vh_medium1())              m_evtTrigFlags |= TRIG_e18vh_medium1;
//-DG--  if(m_event.triggerbits.EF_mu15())                       m_evtTrigFlags |= TRIG_mu15;
//-DG--
//-DG--  // EF_2mu8_EFxe40wMu_tclcw trigger only available for data, in periods > B
//-DG--  if(!m_isMC && m_event.eventinfo.RunNumber()>=206248 && m_event.triggerbits.EF_2mu8_EFxe40wMu_tclcw())
//-DG--    m_evtTrigFlags |= TRIG_2mu8_EFxe40wMu_tclcw;
//-DG--
//-DG--  // Triggers requested fro the ISR analysis studies
//-DG--  if(m_event.triggerbits.EF_mu6())                                m_evtTrigFlags |= TRIG_mu6;
//-DG--  if(m_event.triggerbits.EF_2mu6())                               m_evtTrigFlags |= TRIG_2mu6;
//-DG--  if(m_event.triggerbits.EF_e18vh_medium1_2e7T_medium1())         m_evtTrigFlags |= TRIG_e18vh_medium1_2e7T_medium1;
//-DG--  if(m_event.triggerbits.EF_3mu6())                               m_evtTrigFlags |= TRIG_3mu6;
//-DG--  if(m_event.triggerbits.EF_mu18_tight_2mu4_EFFS())               m_evtTrigFlags |= TRIG_mu18_tight_2mu4_EFFS;
//-DG--  if(m_event.triggerbits.EF_2e7T_medium1_mu6())                   m_evtTrigFlags |= TRIG_2e7T_medium1_mu6;
//-DG--  if(m_event.triggerbits.EF_e7T_medium1_2mu6())                   m_evtTrigFlags |= TRIG_e7T_medium1_2mu6;
//-DG--  if(m_event.triggerbits.EF_xe80_tclcw_loose())                   m_evtTrigFlags |= TRIG_xe80_tclcw_loose;
//-DG--  if(m_event.triggerbits.EF_j110_a4tchad_xe90_tclcw_loose())      m_evtTrigFlags |= TRIG_j110_a4tchad_xe90_tclcw_loose;
//-DG--  if(m_event.triggerbits.EF_j80_a4tchad_xe100_tclcw_loose())      m_evtTrigFlags |= TRIG_j80_a4tchad_xe100_tclcw_loose;
//-DG--  if(m_event.triggerbits.EF_j80_a4tchad_xe70_tclcw_dphi2j45xe10())m_evtTrigFlags |= TRIG_j80_a4tchad_xe70_tclcw_dphi2j45xe10;
//-DG--
//-DG--  // Not sure about the availability of these, so just adding some protection
//-DG--  if(m_event.triggerbits.EF_mu4T())                               m_evtTrigFlags |= TRIG_mu4T;
//-DG--  if(m_event.triggerbits.EF_mu24())                               m_evtTrigFlags |= TRIG_mu24;
//-DG--  if(m_event.triggerbits.EF_mu4T_j65_a4tchad_xe70_tclcw_veryloose()) m_evtTrigFlags |= TRIG_mu4T_j65_a4tchad_xe70_tclcw_veryloose;
//-DG--  if(m_event.triggerbits.EF_2mu4T_xe60_tclcw())                   m_evtTrigFlags |= TRIG_2mu4T_xe60_tclcw;
//-DG--  if(m_event.triggerbits.EF_2mu8_EFxe40_tclcw.IsAvailable() && m_event.triggerbits.EF_2mu8_EFxe40_tclcw())
//-DG--    m_evtTrigFlags |= TRIG_2mu8_EFxe40_tclcw;
//-DG--  if(m_event.triggerbits.EF_e24vh_medium1_EFxe35_tclcw())         m_evtTrigFlags |= TRIG_e24vh_medium1_EFxe35_tclcw;
//-DG--  if(m_event.triggerbits.EF_mu24_j65_a4tchad_EFxe40_tclcw())      m_evtTrigFlags |= TRIG_mu24_j65_a4tchad_EFxe40_tclcw;
//-DG--  if(m_event.triggerbits.EF_mu24_j65_a4tchad_EFxe40wMu_tclcw.IsAvailable() && m_event.triggerbits.EF_mu24_j65_a4tchad_EFxe40wMu_tclcw())
//-DG--    m_evtTrigFlags |= TRIG_mu24_j65_a4tchad_EFxe40wMu_tclcw;
}

/*--------------------------------------------------------------------------------*/
// Electron trigger matching
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::matchElectronTriggers()
{
  if(m_dbg>=5) cout << "matchElectronTriggers" << endl;
//-DG--  for(uint i=0; i<m_preElectrons.size(); i++){
//-DG--    int iEl = m_preElectrons[i];
//-DG--    const TLorentzVector &lv = m_susyObj.GetElecTLV(iEl);
//-DG--    // trigger flags
//-DG--    long long flags = 0;
//-DG--    // 2012 triggers only
//-DG--    if(matchElectronTrigger(lv, m_event.trig_EF_el.EF_e7T_medium1()))               { flags |= TRIG_e7_medium1; }
//-DG--    if(matchElectronTrigger(lv, m_event.trig_EF_el.EF_e12Tvh_loose1()))             { flags |= TRIG_e12Tvh_loose1; }
//-DG--    if(matchElectronTrigger(lv, m_event.trig_EF_el.EF_e12Tvh_medium1()))            { flags |= TRIG_e12Tvh_medium1; }
//-DG--    if(matchElectronTrigger(lv, m_event.trig_EF_el.EF_e24vh_medium1()))             { flags |= TRIG_e24vh_medium1; }
//-DG--    if(matchElectronTrigger(lv, m_event.trig_EF_el.EF_e24vhi_medium1()))            { flags |= TRIG_e24vhi_medium1; }
//-DG--    if(matchElectronTrigger(lv, m_event.trig_EF_el.EF_2e12Tvh_loose1()))            { flags |= TRIG_2e12Tvh_loose1; }
//-DG--    if(matchElectronTrigger(lv, m_event.trig_EF_el.EF_e24vh_medium1_e7_medium1()))  { flags |= TRIG_e24vh_medium1_e7_medium1; }
//-DG--    if(matchElectronTrigger(lv, m_event.trig_EF_el.EF_e12Tvh_medium1_mu8()))        { flags |= TRIG_e12Tvh_medium1_mu8; }
//-DG--    if(matchElectronTrigger(lv, m_event.trig_EF_el.EF_e18vh_medium1()))             { flags |= TRIG_e18vh_medium1; }
//-DG--    if(matchElectronTrigger(lv, m_event.trig_EF_el.EF_e18vh_medium1_2e7T_medium1())){ flags |= TRIG_e18vh_medium1_2e7T_medium1; }
//-DG--    if(matchElectronTrigger(lv, m_event.trig_EF_el.EF_2e7T_medium1_mu6()))          { flags |= TRIG_2e7T_medium1_mu6; }
//-DG--    if(matchElectronTrigger(lv, m_event.trig_EF_el.EF_e7T_medium1_2mu6()))          { flags |= TRIG_e7T_medium1_2mu6; }
//-DG--    if(matchElectronTrigger(lv, m_event.trig_EF_el.EF_e24vh_medium1_EFxe35_tclcw())){ flags |= TRIG_e24vh_medium1_EFxe35_tclcw; }
//-DG--    m_eleTrigFlags[iEl] = flags;
//-DG--  }
}
/*--------------------------------------------------------------------------------*/
bool XaodAnalysis::matchElectronTrigger(const TLorentzVector &lv, vector<int>* trigBools)
{
  // matched trigger index - not used
  static int indexEF = -1;
  // Use function defined in egammaAnalysisUtils/egammaTriggerMatching.h
  // return PassedTriggerEF(lv.Eta(), lv.Phi(), trigBools, indexEF, m_event.trig_EF_el.n(),
  //                        m_event.trig_EF_el.eta(), m_event.trig_EF_el.phi());
  return false;
}

/*--------------------------------------------------------------------------------*/
// Muon trigger matching
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::matchMuonTriggers()
{
//-DG--  if(m_dbg>=5) cout << "matchMuonTriggers" << endl;
//-DG--  for(uint i=0; i<m_preMuons.size(); i++){
//-DG--    int iMu = m_preMuons[i];
//-DG--    const TLorentzVector &lv = m_susyObj.GetMuonTLV(iMu);
//-DG--    long long flags = 0;
//-DG--    // 2012 triggers only
//-DG--    if( matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_mu8()) )         { flags |= TRIG_mu8; }
//-DG--    if( matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_mu13()) )        { flags |= TRIG_mu13; }
//-DG--    if( matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_mu18_tight()) )  { flags |= TRIG_mu18_tight; }
//-DG--    if( matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_mu24i_tight()) ) { flags |= TRIG_mu24i_tight; }
//-DG--    if( matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_2mu13()) )       { flags |= TRIG_2mu13; }
//-DG--    if( matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_mu18_tight_mu8_EFFS()) )   { flags |= TRIG_mu18_tight_mu8_EFFS; }
//-DG--    if( matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_mu18_tight_e7_medium1()) ) { flags |= TRIG_mu18_tight_e7_medium1; }
//-DG--    if( matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_mu15()) )                  { flags |= TRIG_mu15; }
//-DG--    if(!m_isMC && m_event.eventinfo.RunNumber()>=206248 &&
//-DG--       matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_2mu8_EFxe40wMu_tclcw()))   { flags |= TRIG_2mu8_EFxe40wMu_tclcw; }
//-DG--    if( matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_mu6()) )                  { flags |= TRIG_mu6; }
//-DG--    if( matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_2mu6()) )                 { flags |= TRIG_2mu6; }
//-DG--    if( matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_mu18_tight_2mu4_EFFS()) ) { flags |= TRIG_mu18_tight_2mu4_EFFS; }
//-DG--    if( matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_mu4T()) )                 { flags |= TRIG_mu4T; }
//-DG--    if( matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_mu24()) )                 { flags |= TRIG_mu24; }
//-DG--    if( matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_mu4T_j65_a4tchad_xe70_tclcw_veryloose()) ) { flags |= TRIG_mu4T_j65_a4tchad_xe70_tclcw_veryloose; }
//-DG--    if( matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_2mu4T_xe60_tclcw()) ) { flags |= TRIG_2mu4T_xe60_tclcw; }
//-DG--    if(m_event.trig_EF_trigmuonef.EF_2mu8_EFxe40_tclcw.IsAvailable() &&
//-DG--       matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_2mu8_EFxe40_tclcw()) ) { flags |= TRIG_2mu8_EFxe40_tclcw; }
//-DG--    if( matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_mu24_j65_a4tchad_EFxe40_tclcw()) ) { flags |= TRIG_mu24_j65_a4tchad_EFxe40_tclcw; }
//-DG--    if(m_event.trig_EF_trigmuonef.EF_mu24_j65_a4tchad_EFxe40wMu_tclcw.IsAvailable() &&
//-DG--       matchMuonTrigger(lv, m_event.trig_EF_trigmuonef.EF_mu24_j65_a4tchad_EFxe40wMu_tclcw()) ) { flags |= TRIG_mu24_j65_a4tchad_EFxe40wMu_tclcw; }
//-DG--    m_muoTrigFlags[iMu] = flags;
//-DG--  }
}
/*--------------------------------------------------------------------------------*/
bool XaodAnalysis::matchMuonTrigger(const TLorentzVector &lv, vector<int>* passTrig)
{
//-DG--  // loop over muon trigger features
//-DG--  for(int iTrig=0; iTrig < m_event.trig_EF_trigmuonef.n(); iTrig++){
//-DG--
//-DG--    // Check to see if this feature passed chain we want
//-DG--    if(passTrig->at(iTrig)){
//-DG--
//-DG--      // Loop over muon EF tracks
//-DG--      TLorentzVector lvTrig;
//-DG--      for(int iTrk=0; iTrk < m_event.trig_EF_trigmuonef.track_n()->at(iTrig); iTrk++){
//-DG--
//-DG--        lvTrig.SetPtEtaPhiM( m_event.trig_EF_trigmuonef.track_CB_pt()->at(iTrig).at(iTrk),
//-DG--                             m_event.trig_EF_trigmuonef.track_CB_eta()->at(iTrig).at(iTrk),
//-DG--                             m_event.trig_EF_trigmuonef.track_CB_phi()->at(iTrig).at(iTrk),
//-DG--                             0 );       // only eta and phi used to compute dR anyway
//-DG--        // Require combined offline track...?
//-DG--        if(!m_event.trig_EF_trigmuonef.track_CB_hasCB()->at(iTrig).at(iTrk)) continue;
//-DG--        float dR = lv.DeltaR(lvTrig);
//-DG--        if(dR < 0.15){
//-DG--          return true;
//-DG--        }
//-DG--
//-DG--      } // loop over EF tracks
//-DG--    } // trigger object passes chain?
//-DG--  } // loop over trigger objects
//-DG--
//-DG--  // matching failed
  return false;
}

/*--------------------------------------------------------------------------------*/
// Tau trigger matching
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::matchTauTriggers()
{
//-DG--  if(m_dbg>=5) cout << "matchTauTriggers" << endl;
//-DG--  for(uint i=0; i<m_preTaus.size(); i++){
//-DG--
//-DG--    int iTau = m_preTaus[i];
//-DG--    const TLorentzVector &lv = m_susyObj.GetTauTLV(iTau);
//-DG--
//-DG--    // trigger flags
//-DG--    long long flags = 0;
//-DG--
//-DG--    // tau20_medium1
//-DG--    if( matchTauTrigger(lv, m_event.trig_EF_tau.EF_tau20_medium1()) ){
//-DG--      flags |= TRIG_tau20_medium1;
//-DG--    }
//-DG--    // tau20Ti_medium1
//-DG--    if( matchTauTrigger(lv, m_event.trig_EF_tau.EF_tau20Ti_medium1()) ){
//-DG--      flags |= TRIG_tau20Ti_medium1;
//-DG--    }
//-DG--    // tau29Ti_medium1
//-DG--    if( matchTauTrigger(lv, m_event.trig_EF_tau.EF_tau29Ti_medium1()) ){
//-DG--      flags |= TRIG_tau29Ti_medium1;
//-DG--    }
//-DG--    // tau29Ti_medium1_tau20Ti_medium1
//-DG--    if( matchTauTrigger(lv, m_event.trig_EF_tau.EF_tau29Ti_medium1_tau20Ti_medium1()) ){
//-DG--      flags |= TRIG_tau29Ti_medium1_tau20Ti_medium1;
//-DG--    }
//-DG--    // tau20Ti_medium1_e18vh_medium1
//-DG--    if( matchTauTrigger(lv, m_event.trig_EF_tau.EF_tau20Ti_medium1_e18vh_medium1()) ){
//-DG--      flags |= TRIG_tau20Ti_medium1_e18vh_medium1;
//-DG--    }
//-DG--    // tau20_medium1_mu15
//-DG--    if( matchTauTrigger(lv, m_event.trig_EF_tau.EF_tau20_medium1_mu15()) ){
//-DG--      flags |= TRIG_tau20_medium1_mu15;
//-DG--    }
//-DG--
//-DG--    // assign the trigger flags for this tau
//-DG--    m_tauTrigFlags[iTau] = flags;
//-DG--  }
}
/*--------------------------------------------------------------------------------*/
bool XaodAnalysis::matchTauTrigger(const TLorentzVector &lv, vector<int>* passTrig)
{
//-DG--  // loop over tau trigger features
//-DG--  for(int iTrig=0; iTrig < m_event.trig_EF_tau.n(); iTrig++){
//-DG--    // Check to see if this feature passed chain we want
//-DG--    if(passTrig->at(iTrig)){
//-DG--      // Now, try to match offline tau to this online tau
//-DG--      static TLorentzVector trigLV;
//-DG--      trigLV.SetPtEtaPhiM(m_event.trig_EF_tau.pt()->at(iTrig), m_event.trig_EF_tau.eta()->at(iTrig),
//-DG--                          m_event.trig_EF_tau.phi()->at(iTrig), m_event.trig_EF_tau.m()->at(iTrig));
//-DG--      float dR = lv.DeltaR(trigLV);
//-DG--      if(dR < 0.15) return true;
//-DG--    }
//-DG--  }
//-DG--  // matching failed
  return false;
}
//----------------------------------------------------------
XaodAnalysis& XaodAnalysis::setGRLFile(TString fileName)
{
    m_grlFileName = fileName; return *this;
}
//----------------------------------------------------------
bool XaodAnalysis::passGRL(const xAOD::EventInfo* eventinfo)
{
    return (m_isMC ||
            m_grl->passRunLB(eventinfo->eventNumber(), eventinfo->runNumber()));
}
//----------------------------------------------------------
bool XaodAnalysis::passTTCVeto()
{
    return true; // DG-2014-08-16 \todo
    //   return (m_event.eventinfo.coreFlags() & 0x40000) == 0;
}
//----------------------------------------------------------
bool XaodAnalysis::passTileErr(const xAOD::EventInfo* eventinfo)
{
	bool eventPassesTileTrip = (m_isMC ||
                                m_susyObj.m_SUSYObjDef->IsTileTrip(eventinfo->runNumber(),
                                                                   eventinfo->lumiBlock(),
                                                                   eventinfo->eventNumber()));
    return eventPassesTileTrip;
}
//----------------------------------------------------------
bool XaodAnalysis::passLarErr()
{
    return true; // DG-2014-08-16 \todo
//    return m_isMC || (m_event.eventinfo.larError()!=2);
}
/*--------------------------------------------------------------------------------*/
// Check event level cleaning cuts like GRL, LarError, etc.
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::assignEventCleaningFlags()
{
    const xAOD::EventInfo* eventinfo = xaodEventInfo();
    if(passGRL(eventinfo))      m_cutFlags |= ECut_GRL;
    if(passTTCVeto())           m_cutFlags |= ECut_TTC;
    if(passLarErr())            m_cutFlags |= ECut_LarErr;
    if(passTileErr(eventinfo))  m_cutFlags |= ECut_TileErr;
    if(passGoodVtx())           m_cutFlags |= ECut_GoodVtx;
    if(passTileTrip())          m_cutFlags |= ECut_TileTrip;
}
//----------------------------------------------------------
void XaodAnalysis::assignObjectCleaningFlags()
{
  if(passTileHotSpot()) m_cutFlags |= ECut_HotSpot;
  if(passBadJet())      m_cutFlags |= ECut_BadJet;
  if(passBadMuon())     m_cutFlags |= ECut_BadMuon;
  if(passCosmic())      m_cutFlags |= ECut_Cosmic;
  if(passLarHoleVeto()) m_cutFlags |= ECut_SmartVeto;
}
//----------------------------------------------------------
bool XaodAnalysis::passLarHoleVeto()
{
  // LAr veto is not used anymore
  return true;
}
//----------------------------------------------------------
bool XaodAnalysis::passTileHotSpot()
{
    return false;
//-DG--  xAOD::JetContainer *jets =  xaodJets();
//-DG--  return !check_jet_tileHotSpot(jets, m_preJets, m_susyObj, !m_isMC, m_event.eventinfo.RunNumber());
}
//----------------------------------------------------------
bool XaodAnalysis::passBadJet()
{
  const xAOD::JetContainer *jets =  xaodJets();
  return false;
//  return !IsBadJetEvent(jets, m_baseJets, 20.*GeV, m_susyObj);
}
//----------------------------------------------------------
bool XaodAnalysis::passGoodVtx()
{
    return true; // DG-2014-08-16 \todo
//  return PrimaryVertexCut(m_susyObj, &m_event.vxp);
}
//----------------------------------------------------------
bool XaodAnalysis::passTileTrip()
{
    return false;
//  return !m_susyObj.IsTileTrip(m_event.eventinfo.RunNumber(), m_event.eventinfo.lbn(), m_event.eventinfo.EventNumber());
}
//----------------------------------------------------------
bool XaodAnalysis::passBadMuon()
{
    return false;
//  return !IsBadMuonEvent(m_susyObj, xaodMuons(), m_preMuons, 0.2);
}
//----------------------------------------------------------
bool XaodAnalysis::passCosmic()
{
    return false;
//  return !IsCosmic(m_susyObj, xaodMuons(), m_baseMuons, 1., 0.2);
}
//----------------------------------------------------------
float XaodAnalysis::getEventWeight(float lumi)
{
  if(!m_isMC) return 1;
  else
      return 1.0;
//  return m_event.eventinfo.mc_event_weight() * getXsecWeight() * getPileupWeight() * lumi / m_sumw;
}
//----------------------------------------------------------
float XaodAnalysis::getXsecWeight()
{
#warning getXsecWeight not implemented
    return 1.0;
  // // Use user cross section if it has been set
  // if(m_xsec > 0) return m_xsec;

  // // Use SUSY cross section file
  // int id = m_event.eventinfo.mc_channel_number();
  // if(m_xsecMap.find(id) == m_xsecMap.end()) {
  //   m_xsecMap[id] = m_susyXsec->process(id);
  // }
  // return m_xsecMap[id].xsect() * m_xsecMap[id].kfactor() * m_xsecMap[id].efficiency();
}
//----------------------------------------------------------
float XaodAnalysis::getLumiWeight()
{ return m_lumi / m_sumw; }
//----------------------------------------------------------
float XaodAnalysis::getPileupWeight()
{
    return 1.0;
  // return m_pileup->GetCombinedWeight(m_event.eventinfo.RunNumber(), m_event.eventinfo.mc_channel_number(), m_event.eventinfo.averageIntPerXing());
}
//----------------------------------------------------------
float XaodAnalysis::getPileupWeightUp()
{
    return 1.0;
  // return m_pileup_up->GetCombinedWeight(m_event.eventinfo.RunNumber(), m_event.eventinfo.mc_channel_number(), m_event.eventinfo.averageIntPerXing());
}
//----------------------------------------------------------
float XaodAnalysis::getPileupWeightDown()
{
    return 1.0;
  // return m_pileup_dn->GetCombinedWeight(m_event.eventinfo.RunNumber(), m_event.eventinfo.mc_channel_number(), m_event.eventinfo.averageIntPerXing());
}
//----------------------------------------------------------
float XaodAnalysis::getPDFWeight8TeV()
{
#warning getPDFWeight8TeV not implemented
    return 1.0;
  // #ifdef USEPDFTOOL
  // float scale = m_event.mcevt.pdf_scale()->at(0);
  // float x1 = m_event.mcevt.pdf_x1()->at(0);
  // float x2 = m_event.mcevt.pdf_x2()->at(0);
  // int id1 = m_event.mcevt.pdf_id1()->at(0);
  // int id2 = m_event.mcevt.pdf_id2()->at(0);

  // // MultLeip function... Not working?
  // //return scaleBeamEnergy(*m_pdfTool, 21000, m_event.mcevt.pdf_scale()->at(0), m_event.mcevt.pdf_x1()->at(0),
  //                        //m_event.mcevt.pdf_x2()->at(0), m_event.mcevt.pdf_id1()->at(0), m_event.mcevt.pdf_id2()->at(0));
  // // Simple scaling
  // //return m_pdfTool->event_weight( pow(scale,2), x1, x2, id1, id2, 21000 );

  // // For scaling to/from arbitrary beam energy
  // m_pdfTool->setEventInfo( scale*scale, x1, x2, id1, id2 );
  // //return m_pdfTool->scale((3.5+4.)/3.5);
  // // possible typo correction?
  // return m_pdfTool->scale(4./3.5);

  // #else
  // return 1;
  // #endif
}
//----------------------------------------------------------
float XaodAnalysis::getLepSF(const vector<LeptonInfo>& leptons)
{
#warning lepton scale factor not implemented
  // TODO: incorporate systematics
  float lepSF = 1;
  if(m_isMC){
    // // Loop over leptons
    // for(uint iLep=0; iLep<leptons.size(); iLep++){
    //   const LeptonInfo &lep = leptons[iLep];
    //   // Electrons
    //   if(lep.isElectron()){
    //       const xAOD::ElectronD3PDObjectElement* el = lep.getElectronElement();
    //     lepSF *= m_susyObj.GetSignalElecSF(el->cl_eta(), lep.lv()->Pt(), true, true, false);
    //   }
    //   // Muons
    //   else{
    //     lepSF *= m_susyObj.GetSignalMuonSF(lep.idx());
    //   }
    // }
  }
  return lepSF;
}
//----------------------------------------------------------
float XaodAnalysis::getBTagSF(const vector<int>& jets)
{
  return 1;
}
//----------------------------------------------------------
void XaodAnalysis::calcRandomRunLB()
{
//-DG--  if(m_pileup){
//-DG--    m_mcRun = m_pileup->GetRandomRunNumber(m_event.eventinfo.RunNumber());
//-DG--    m_mcLB = m_pileup->GetRandomLumiBlockNumber(m_mcRun);
//-DG--  }
}
//----------------------------------------------------------
int XaodAnalysis::getHFORDecision()
{
#warning getHFORDecision not implemented
    return 1;
  // return m_hforTool.getDecision(m_event.eventinfo.mc_channel_number(),
  //                               m_event.mc.n(),
  //                               m_event.mc.pt(),
  //                               m_event.mc.eta(),
  //                               m_event.mc.phi(),
  //                               m_event.mc.m(),
  //                               m_event.mc.pdgId(),
  //                               m_event.mc.status(),
  //                               m_event.mc.vx_barcode(),
  //                               m_event.mc.parent_index(),
  //                               m_event.mc.child_index(),
  //                               HforToolD3PD::ALL); //HforToolD3PD::DEFAULT
}
//----------------------------------------------------------
void XaodAnalysis::setMetFlavor(string metFlav)
{
  if(metFlav=="STVF") m_metFlavor = SUSYMet::STVF;
  else if(metFlav=="STVF_JVF") m_metFlavor = SUSYMet::STVF_JVF;
  else if(metFlav=="Default") m_metFlavor = SUSYMet::Default;
  else{
    cout << "XaodAnalysis::setMetFlavor : ERROR : MET flavor " << metFlav
         << " is not supported!" << endl;
    abort();
  }
}
//----------------------------------------------------------
void XaodAnalysis::dumpEvent()
{
    const xAOD::EventInfo* eventinfo = 0;
    m_event.retrieve(eventinfo, "EventInfo");
    cout<<(*eventinfo)<<endl;
}
//----------------------------------------------------------
void XaodAnalysis::dumpBaselineObjects()
{
  uint nEle = m_baseElectrons.size();
  uint nMu  = m_baseMuons.size();
  //uint nTau = m_baseTaus.size();
  uint nJet = m_baseJets.size();

  #warning dumpBaselineObjects not implemented
  // cout.precision(2);
  // if(nEle){
  //   cout << "Baseline electrons" << endl;
  //   for(uint i=0; i < nEle; i++){
  //     int iEl = m_baseElectrons[i];
  //     const TLorentzVector &lv = m_susyObj.GetElecTLV(iEl);
  //     const xAOD::ElectronD3PDObjectElement &ele = (*d3pdElectrons())[iEl];
  //     cout << "  El : " << fixed
  //          << " q " << setw(2) << (int) ele.charge()
  //          << " pt " << setw(6) << lv.Pt()/GeV
  //          << " eta " << setw(5) << lv.Eta()
  //          << " phi " << setw(5) << lv.Phi();
  //     if(m_isMC) cout << " type " << setw(2) << ele.type() << " origin " << setw(2) << ele.origin();
  //     cout << endl;
  //   }
  // }
  // if(nMu){
  //   cout << "Baseline muons" << endl;
  //   for(uint i=0; i < nMu; i++){
  //     int iMu = m_baseMuons[i];
  //     const TLorentzVector &lv = m_susyObj.GetMuonTLV(iMu);
  //     const xAOD::MuonD3PDObjectElement &muo = (*d3pdMuons())[iMu];
  //     cout << "  Mu : " << fixed
  //          << " q " << setw(2) << (int) muo.charge()
  //          << " pt " << setw(6) << lv.Pt()/GeV
  //          << " eta " << setw(5) << lv.Eta()
  //          << " phi " << setw(5) << lv.Phi();
  //     if(m_isMC) cout << " type " << setw(2) << muo.type() << " origin " << setw(2) << muo.origin();
  //     cout << endl;
  //   }
  // }
  // if(nJet){
  //   cout << "Baseline jets" << endl;
  //   for(uint i=0; i < nJet; i++){
  //     int iJet = m_baseJets[i];
  //     const TLorentzVector &lv = m_susyObj.GetJetTLV(iJet);
  //     const xAOD::JetD3PDObjectElement &jet = (*d3pdJets())[iJet];
  //     cout << "  Jet : " << fixed
  //          << " pt " << setw(6) << lv.Pt()/GeV
  //          << " eta " << setw(5) << lv.Eta()
  //          << " phi " << setw(5) << lv.Phi()
  //          << " mv1 " << jet.flavor_weight_MV1();
  //     cout << endl;
  //   }
  // }
  // cout.precision(6);
  // cout.unsetf(ios_base::fixed);
}
//----------------------------------------------------------
void XaodAnalysis::dumpSignalObjects()
{
#warning dumpSignalObjects not implemented
  // uint nEle = m_sigElectrons.size();
  // uint nMu  = m_sigMuons.size();
  // //uint nTau = m_sigTaus.size();
  // uint nJet = m_sigJets.size();

  // cout.precision(2);
  // if(nEle){
  //   cout << "Signal electrons" << endl;
  //   for(uint i=0; i < nEle; i++){
  //     int iEl = m_sigElectrons[i];
  //     const TLorentzVector &lv = m_susyObj.GetElecTLV(iEl);
  //     const xAOD::ElectronD3PDObjectElement &ele = (*d3pdElectrons())[iEl];
  //     cout << "  El : " << fixed
  //          << " q " << setw(2) << (int) ele.charge()
  //          << " pt " << setw(6) << lv.Pt()/GeV
  //          << " eta " << setw(5) << lv.Eta()
  //          << " phi " << setw(5) << lv.Phi();
  //     if(m_isMC) cout << " type " << setw(2) << ele.type() << " origin " << setw(2) << ele.origin();
  //     cout << endl;
  //   }
  // }
  // if(nMu){
  //   cout << "Signal muons" << endl;
  //   for(uint i=0; i < nMu; i++){
  //     int iMu = m_sigMuons[i];
  //     const TLorentzVector &lv = m_susyObj.GetMuonTLV(iMu);
  //     const xAOD::MuonD3PDObjectElement &muo = (*d3pdMuons())[iMu];
  //     cout << "  Mu : " << fixed
  //          << " q " << setw(2) << (int) muo.charge()
  //          << " pt " << setw(6) << lv.Pt()/GeV
  //          << " eta " << setw(5) << lv.Eta()
  //          << " phi " << setw(5) << lv.Phi();
  //     if(m_isMC) cout << " type " << setw(2) << muo.type() << " origin " << setw(2) << muo.origin();
  //     cout << endl;
  //   }
  // }
  // if(nJet){
  //   cout << "Signal jets" << endl;
  //   for(uint i=0; i < nJet; i++){
  //     int iJet = m_sigJets[i];
  //     const TLorentzVector &lv = m_susyObj.GetJetTLV(iJet);
  //     const xAOD::JetD3PDObjectElement &jet = (*d3pdJets())[iJet];
  //     cout << "  Jet : " << fixed
  //          << " pt " << setw(6) << lv.Pt()/GeV
  //          << " eta " << setw(5) << lv.Eta()
  //          << " phi " << setw(5) << lv.Phi()
  //          << " mv1 " << jet.flavor_weight_MV1();
  //     cout << endl;
  //   }
  // }
  // cout.precision(6);
  // cout.unsetf(ios_base::fixed);
}
//----------------------------------------------------------
bool XaodAnalysis::runningOptionsAreValid()
{
    bool valid=true;
    if(m_isMC && m_mcProd==MCProd_Unknown){
        valid=false;
        if(m_dbg)
            cout<<"XaodAnalysis::runningOptionsAreValid invalid production"
                <<" 'MCProd_Unknown' is not a valid choice for simulated samples."
                <<" You should call XaodAnalysis::setMCProduction()"
                <<endl;
    }
    return valid;
#warning runningOptionsAreValid not complete
    // bool isSimulation = m_event.eventinfo.isSimulation();
    // bool isData = !isSimulation;
    // bool isStreamEgamma = m_event.eventinfo.streamDecision_Egamma();
    // bool isStreamJetEt  = m_event.eventinfo.streamDecision_JetTauEtmiss();
    // bool isStreamMuons  = m_event.eventinfo.streamDecision_Muons();
    // if(m_isMC != isSimulation) {
    //     valid=false;
    //     if(m_dbg)
    //         cout<<"XaodAnalysis::runningOptionsAreValid invalid isMc:"
    //             <<" (m_isMC:"<<m_isMC<<" != isSimulation:"<<isSimulation<<")"
    //             <<endl;
    // }
    // if(isData) {
    //     bool consistentStream = (isStreamMuons  ? m_stream==Stream_Muons :
    //                              isStreamEgamma ? m_stream==Stream_Egamma :
    //                              isStreamJetEt  ? m_stream==Stream_JetTauEtmiss :
    //                              false);
    //     if(!consistentStream) {
    //         valid=false;
    //         if(m_dbg)
    //             cout<<"XaodAnalysis::runningOptionsAreValid: inconsistent stream"
    //                 <<" m_stream: "<<(m_stream==Stream_Muons        ? "Stream_Muons":
    //                                   m_stream==Stream_Egamma       ? "Stream_Egamma":
    //                                   m_stream==Stream_JetTauEtmiss ? "Stream_JetTauEtmiss":
    //                                   "unknown")
    //                 <<" eventinfo: "<<(isStreamMuons ? "Muons":
    //                                    isStreamEgamma ? "Egamma":
    //                                    isStreamJetEt ? "JetTauEtmiss":
    //                                    "unknown")
    //                 <<endl;

    //     }
    // }
    // if(m_dbg)
    //     cout<<"XaodAnalysis::runningOptionsAreValid(): "<<(valid?"true":"false")<<endl;
    // return valid;
}
//----------------------------------------------------------
std::string XaodAnalysis::defauldGrlFile()
{
    return std::string( "$ROOTCOREBIN/../SUSYTools/data/GRL/Summer2013/"
                        "data12_8TeV.periodAllYear_DetStatus-v61-pro14-02"
                        "_DQDefects-00-01-00_PHYS_StandardGRL_All_Good.xml");
}
//----------------------------------------------------------
bool XaodAnalysis::initGrlTool()
{
    bool success = false;
    m_grl = new GoodRunsListSelectionTool("GoodRunsListSelectionTool");
    std::vector<std::string> grl_files;
    grl_files.push_back(XaodAnalysis::defauldGrlFile());
    m_grl->setProperty("GoodRunsListVec", grl_files);
    m_grl->setProperty("PassThrough", false);
    success = m_grl->initialize(); // DG any check we should do here? (file_exists?)
    return success;
}
//----------------------------------------------------------
DataStream XaodAnalysis::streamFromSamplename(const TString &sample, bool isdata)
{
    bool ismc(!isdata);
//    TString sample(s.c_str());
    DataStream stream = Stream_Unknown;
    if(ismc) stream = Stream_MC;
    else if(sample.Contains("muons",        TString::kIgnoreCase)) stream = Stream_Muons;
    else if(sample.Contains("egamma",       TString::kIgnoreCase)) stream = Stream_Egamma;
    else if(sample.Contains("jettauetmiss", TString::kIgnoreCase)) stream = Stream_JetTauEtmiss;
    else
        cout<<"XaodAnalysis::streamFromSamplename('"<<sample<<"',isdata="<<(isdata?"true":"false")<<")"
            <<" : cannot determine the stream, returning "<<streamName(stream)<<endl;
    return stream;
}
//----------------------------------------------------------
bool XaodAnalysis::isDataFromSamplename(const TString &sample)
{
    return sample.Contains("data", TString::kIgnoreCase);
}
//----------------------------------------------------------
bool XaodAnalysis::isSimuFromSamplename(const TString &s)
{
    return !XaodAnalysis::isDataFromSamplename(s);
}
//----------------------------------------------------------