#include "TSystem.h"

#include "SusyCommon/XaodAnalysis.h"
//#include "SusyCommon/get_object_functions.h"

// #include "egammaAnalysisUtils/egammaTriggerMatching.h"
// #include "D3PDReader/JetD3PDObject.h"
#include "xAODBase/IParticleHelpers.h" // setOriginalObjectLink

#include <limits>
#include <algorithm> // copy_if, transform
#include <iterator> // back_inserter
#include <numeric> // accumulate


// trigger
#include "SusyCommon/TriggerMap.h"


using namespace std;

using susy::XaodAnalysis;


#undef CHECK
#define CHECK( ARG )                                                \
    do {                                                            \
        const bool result = ARG;                                    \
        if( ! result ) {                                            \
            ::Error( "XaodAnalysis", "Failed to execute: \"%s\"",   \
                     #ARG );                                        \
            exit(-1);                                               \
        }                                                           \
    } while( false )


//----------------------------------------------------------
XaodAnalysis::XaodAnalysis() :
    m_sample(""),
    m_stream(Stream_Unknown),
    m_isDerivation(false), // dantrim event shape
    m_isAF2(false),
    m_mcProd(MCProd_Unknown),
    m_d3pdTag(D3PD_p1328),
    m_selectPhotons(false),
    m_selectTaus(false),
    m_selectTruth(false),
    // m_metFlavor(SUSYMet::Default),
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
    m_grl(NULL),
    m_tree(NULL),
    m_entry(0),
    m_dbg(0),
    m_isMC(false),
    m_flagsAreConsistent(false),
    m_flagsHaveBeenChecked(false),
    //m_event(xAOD::TEvent::kClassAccess),
    m_event(xAOD::TEvent::kBranchAccess),
    m_store(),
//	m_eleIDDefault(Medium), ///> not likelihood
        m_eleIDDefault(TightLLH),
	m_electronEfficiencySFTool(0),
	m_pileupReweightingTool(0),
	m_muonEfficiencySFTool(0),
	m_tauTruthMatchingTool(0),
	m_tauTruthTrackMatchingTool(0),
        //dantrim trig
        m_evtTrigBits(m_nTriggerBits),
        m_configTool(NULL),
        m_trigTool(NULL),
        m_escopier(NULL)
{
    clearContainerPointers();
    clearOutputObjects();

}
//----------------------------------------------------------
void XaodAnalysis::Init(TTree *tree)
{
    xAOD::Init("susy::XaodAnalysis").ignore();


    m_event.readFrom(tree);
    m_isMC = XaodAnalysis::isSimuFromSamplename(m_sample);
    m_isDerivation = XaodAnalysis::isDerivationFromSamplename(m_sample); // dantrim event shape
    bool isData = XaodAnalysis::isDataFromSamplename(m_sample);
    m_stream = XaodAnalysis::streamFromSamplename(m_sample, isData);
    initSusyTools();
    initLocalTools();
    if(m_isMC && m_sys) getSystematicList();
    else{
        ST::SystInfo infodef;
        infodef.affectsKinematics = false;
        infodef.affectsWeights = false;
        infodef.affectsType = ST::Unknown;
        systInfoList.push_back(infodef);
    }
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
    static Long64_t chainEntry = -1;
    chainEntry++;
    m_event.getEntry(entry);
    retrieveCollections();
    if(m_dbg || chainEntry%10000==0)
        {
            const xAOD::EventInfo* eventinfo = xaodEventInfo();
            cout<<"run "<<eventinfo->eventNumber()<<" event "<<eventinfo->runNumber()<<endl;
        }
    // Object selection
    // SusyNtSys sys = NtSys_NOM;
    // selectObjects(sys);
    // buildMet();
    deleteShallowCopies();
    clearOutputObjects();
    clearContainerPointers();
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

    delete m_electronEfficiencySFTool;
    delete m_pileupReweightingTool;
    delete m_muonEfficiencySFTool;
    delete m_tauTruthMatchingTool;
    delete m_tauTruthTrackMatchingTool;


    for(int i=Medium; i<eleIDInvalid; i++){
        delete m_susyObj[i];
    }

    // dantrim trig
    delete m_trigTool;
    delete m_configTool;

    delete m_escopier;
    
    

}
//----------------------------------------------------------
XaodAnalysis& XaodAnalysis::initSusyTools()
{
    for(int i=Medium; i<eleIDInvalid; i++){
        string name = "SUSYObjDef_xAOD_" + eleIDNames[i];
        m_susyObj[i] = new ST::SUSYObjDef_xAOD(name);
        cout << "---------------------------------------" << endl;
        cout << "XaodAnalysis::initSusyTools:           " << name <<endl;
        cout << "---------------------------------------" << endl;

        m_susyObj[i]->msg().setLevel(m_dbg ? MSG::DEBUG : MSG::WARNING);
        m_susyObj[i]->setProperty("IsData",          static_cast<int>(!m_isMC));
        m_susyObj[i]->setProperty("IsAtlfast",       static_cast<int>(m_isAF2));
        m_susyObj[i]->setProperty("EleId", eleIDNames[i]);

        CHECK( m_susyObj[i]->SUSYToolsInit() );
    }

    return *this;
}
//----------------------------------------------------------
XaodAnalysis& XaodAnalysis::initLocalTools()
{
    char *tmparea=getenv("ROOTCOREBIN");
    char* TestArea = getenv("TestArea");

    if (tmparea != NULL) {
        maindir = tmparea;
        maindir = maindir + "/data/";
    }
    else if (TestArea != NULL ) {/// Athena
        tmparea = TestArea;
        maindir = tmparea;
        maindir = maindir + "/";
    } else {
        cout << " RootCore area not set up " << endl
             <<"Exiting... "<<endl << endl;
        exit(-1);
    }


    initPileupTool();
    initMuonTools();
    initTauTools();


    // dantrim trig
    m_configTool = new TrigConf::xAODConfigTool("xAODConfigTool");
    ToolHandle<TrigConf::ITrigConfigTool> configHandle(m_configTool);
    CHECK( configHandle->initialize() );
    
    m_trigTool = new Trig::TrigDecisionTool("TrigDecTool");
    m_trigTool->setProperty("ConfigTool", configHandle);
    m_trigTool->setProperty("TrigDecisionKey", "xTrigDecision");
    m_trigTool->setProperty("OutputLevel", MSG::ERROR).ignore(); // dantrim Mar 16 2015 -- tool outputs extraneous errors, ignore them
    CHECK( m_trigTool->initialize() );


    m_escopier = new EventShapeCopier("Kt4LCCopier");
//    if ( m_isDerivation ) {
//        m_escopier->renameEventDensities();
//    }


    return *this;
}
//----------------------------------------------------------
void XaodAnalysis::initPileupTool()
{
    m_pileupReweightingTool = new CP::PileupReweightingTool("PileupReweightingTool");
    m_pileupReweightingTool->setProperty("Input","EventInfo");

    std::vector<std::string> prwFiles;
    std::vector<std::string> lumicalcFiles;

    prwFiles.push_back("PileupReweighting/mc14v1_defaults.prw.root");
    CHECK (m_pileupReweightingTool->setProperty("ConfigFiles",prwFiles));

    lumicalcFiles.push_back(maindir+"SUSYTools/susy_data12_avgintperbx.root");
    CHECK( m_pileupReweightingTool->setProperty("LumiCalcFiles",lumicalcFiles) );
    //AT-2014-10-31 For systematic instanciate two more tools with difference SF
    //CHECK( m_pileupReweightingTool->setProperty("DataScaleFactors",1/1.08) );
    //CHECK( m_pileupReweightingTool->setProperty("DataScaleFactors",1/1.11) );
    CHECK( m_pileupReweightingTool->initialize() );

}
//----------------------------------------------------------
void XaodAnalysis::initMuonTools()
{
    m_muonEfficiencySFTool = new CP::MuonEfficiencyScaleFactors("MuonEfficiencyScaleFactors");
    CHECK( m_muonEfficiencySFTool->setProperty("WorkingPoint","CBandST") );
    CHECK( m_muonEfficiencySFTool->setProperty("DataPeriod","2012") );
    CHECK( m_muonEfficiencySFTool->initialize() );

    cout << "ASM :: MuonEffTool is initialized correctly..." << endl;
}
//----------------------------------------------------------
void XaodAnalysis::initTauTools()
{
    m_tauTruthMatchingTool = new TauAnalysisTools::TauTruthMatchingTool("TauTruthMatchingTool");
    m_tauTruthMatchingTool->msg().setLevel(m_dbg ? MSG::DEBUG : MSG::WARNING);
    CHECK(m_tauTruthMatchingTool->initialize());

    m_tauTruthTrackMatchingTool = new TauAnalysisTools::TauTruthTrackMatchingTool("TauTruthTrackMatchingTool");
    m_tauTruthTrackMatchingTool->msg().setLevel(m_dbg ? MSG::DEBUG : MSG::WARNING);
    CHECK(m_tauTruthTrackMatchingTool->initialize());

}


/*--------------------------------------------------------------------------------*/
// Get the list of recommended systematics from CP
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::getSystematicList()
{
    if(m_dbg>=5) cout << "getSystematicList" << endl;
    //Get from SUSYTools the list of systematics and
    //what each systematics affects: weight/kin and object type
    systInfoList = m_susyObj[m_eleIDDefault]->getSystInfoList();
}

//----------------------------------------------------------
const xAOD::EventInfo* XaodAnalysis::retrieveEventInfo(xAOD::TEvent &e, bool dbg)
{
    const xAOD::EventInfo* evt = NULL;
    e.retrieve(evt, "EventInfo");
    if(dbg){
        if(evt) cout<<"XaodAnalysis::retrieveEventInfo: retrieved"<<endl;
        else    cout<<"XaodAnalysis::retrieveEventInfo: failed"<<endl;
    }
    return evt;
}
//----------------------------------------------------------
const xAOD::EventInfo* XaodAnalysis::xaodEventInfo()
{
    if(m_xaodEventInfo==NULL){
        m_xaodEventInfo = retrieveEventInfo(m_event, m_dbg);
    }
    return m_xaodEventInfo;
}
//----------------------------------------------------------
xAOD::MuonContainer* XaodAnalysis::xaodMuons(ST::SystInfo sysInfo, SusyNtSys sys)
{
    const float minPt=20000;
    bool syst_affectsMuons     = ST::testAffectsObject(xAOD::Type::Muon, sysInfo.affectsType);
    if(sys!=NtSys::NOM && syst_affectsMuons){
        if(m_xaodMuons==NULL){
            //m_susyObj[m_eleIDDefault]->GetMuons(m_xaodMuons, m_xaodMuonsAux, false, minPt);
            m_susyObj[m_eleIDDefault]->GetMuons(m_xaodMuons, m_xaodMuonsAux);
        }
        if(m_dbg>=5) cout << "xaodMuo "<< m_xaodMuons->size() << endl;
        return m_xaodMuons;
    }
    else{
        if(m_xaodMuons_nom==NULL){
            //m_susyObj[m_eleIDDefault]->GetMuons(m_xaodMuons_nom, m_xaodMuonsAux_nom, false, minPt);
            m_susyObj[m_eleIDDefault]->GetMuons(m_xaodMuons_nom, m_xaodMuonsAux_nom);
        }
        if(m_dbg>=5) cout << "xaodMuo_nom " << m_xaodMuons_nom->size() << endl;
        return m_xaodMuons_nom;
    }
    return NULL;
}
//----------------------------------------------------------
xAOD::ElectronContainer* XaodAnalysis::xaodElectrons(ST::SystInfo sysInfo, SusyNtSys sys)
{
    const float minPt=20000;
    bool syst_affectsElectrons = ST::testAffectsObject(xAOD::Type::Electron, sysInfo.affectsType);
    if(sys!=NtSys::NOM && syst_affectsElectrons){
        if(m_xaodElectrons==NULL){
            //m_susyObj[m_eleIDDefault]->GetElectrons(m_xaodElectrons, m_xaodElectronsAux, false, minPt);
            m_susyObj[m_eleIDDefault]->GetElectrons(m_xaodElectrons, m_xaodElectronsAux);
        }
        if(m_dbg>=5) cout << "xaodEle " << m_xaodElectrons->size() << endl;
        return m_xaodElectrons;
    }
    else{
        if(m_xaodElectrons_nom==NULL){
            //m_susyObj[m_eleIDDefault]->GetElectrons(m_xaodElectrons_nom, m_xaodElectronsAux_nom, false, minPt);
            m_susyObj[m_eleIDDefault]->GetElectrons(m_xaodElectrons_nom, m_xaodElectronsAux_nom);
        }
        if(m_dbg>=5) cout << "xaodEle_nom " << m_xaodElectrons_nom->size() << endl;
        return m_xaodElectrons_nom;
    }
    return NULL;
}
//----------------------------------------------------------
xAOD::TauJetContainer* XaodAnalysis::xaodTaus(ST::SystInfo sysInfo, SusyNtSys sys)
{
    bool syst_affectsTaus      = ST::testAffectsObject(xAOD::Type::Tau, sysInfo.affectsType);
    if(sys!=NtSys::NOM && syst_affectsTaus){
        if(m_xaodTaus==NULL){
            m_susyObj[m_eleIDDefault]->GetTaus(m_xaodTaus, m_xaodTausAux);
        }
        if(m_dbg>=5) cout << "xaodTaus " << m_xaodTaus->size() << endl;
        return m_xaodTaus;
    }
    else{
        if(m_xaodTaus_nom==NULL){
            m_susyObj[m_eleIDDefault]->GetTaus(m_xaodTaus_nom, m_xaodTausAux_nom);
        }
        if(m_dbg>=5) cout << "xaodTaus_nom " << m_xaodTaus_nom->size() << endl;
        return m_xaodTaus_nom;
    }

    return NULL;
}
//----------------------------------------------------------
xAOD::JetContainer* XaodAnalysis::xaodJets(ST::SystInfo sysInfo, SusyNtSys sys)
{
    bool syst_affectsJets      = ST::testAffectsObject(xAOD::Type::Jet, sysInfo.affectsType);
    if(sys!=NtSys::NOM && syst_affectsJets){
        if(m_xaodJets==NULL){
            // dantrim event shape
            if ( m_isDerivation ) m_escopier->renameEventDensities();
            m_susyObj[m_eleIDDefault]->GetJets(m_xaodJets, m_xaodJetsAux);
        }
        if(m_dbg>=5) cout << "xaodJets " << m_xaodJets->size() << endl;
        return m_xaodJets;
    }
    else{
        if(m_xaodJets_nom==NULL){
            // dantrim event shape
            if ( m_isDerivation ) m_escopier->renameEventDensities();
            m_susyObj[m_eleIDDefault]->GetJets(m_xaodJets_nom, m_xaodJetsAux_nom);
        }
        if(m_dbg>=5) cout << "xaodJets_nom " << m_xaodJets_nom->size() << endl;
        return m_xaodJets_nom;
    }
    return NULL;
}
//----------------------------------------------------------
xAOD::PhotonContainer* XaodAnalysis::xaodPhotons(ST::SystInfo sysInfo, SusyNtSys sys)
{
    bool syst_affectsPhotons   = ST::testAffectsObject(xAOD::Type::Photon, sysInfo.affectsType);
    if(sys!=NtSys::NOM && syst_affectsPhotons){
        if(m_xaodPhotons==NULL){
            m_susyObj[m_eleIDDefault]->GetPhotons(m_xaodPhotons, m_xaodPhotonsAux);
        }
        if(m_dbg>=5) cout << "xaodPho " << m_xaodPhotons->size()  << endl;
        return m_xaodPhotons;
    }
    else{
        if(m_xaodPhotons_nom==NULL){
            m_susyObj[m_eleIDDefault]->GetPhotons(m_xaodPhotons_nom, m_xaodPhotonsAux_nom);
        }
        if(m_dbg>=5 && m_xaodPhotons_nom) cout << "xaodPho_nom " << m_xaodPhotons_nom->size() << endl;
        return m_xaodPhotons_nom;
    }
    return NULL;
}
//----------------------------------------------------------
const xAOD::TruthEventContainer* XaodAnalysis::retrieveTruthEvent(xAOD::TEvent &e, bool dbg)
{
    const xAOD::TruthEventContainer* truth = NULL;
    e.retrieve(truth, "TruthEvent");
    if(dbg){
        if(truth) cout<<"XaodAnalysis::retrieveTruthEvent: retrieved "<<endl;
        else      cout<<"XaodAnalysis::retrieveTruthEvent: failed"<<endl;
    }
    return truth;
}
//----------------------------------------------------------
const xAOD::TruthEventContainer* XaodAnalysis::xaodTruthEvent()
{
    if(m_xaodTruthEvent==NULL && m_isMC){
        m_xaodTruthEvent = retrieveTruthEvent(m_event, m_dbg);
    }
    return m_xaodTruthEvent;
}
//----------------------------------------------------------
const xAOD::TruthParticleContainer* XaodAnalysis::retrieveTruthParticles(xAOD::TEvent &e, bool dbg)
{
    const xAOD::TruthParticleContainer* truthP = NULL;
    e.retrieve(truthP, "TruthParticle");
    if(dbg){
        if(truthP) cout<<"XaodAnalysis::retrieveTruthParticles: retrieved "<<truthP->size()<<endl;
        else       cout<<"XaodAnalysis::retrieveTruthParticles: failed"<<endl;
    }
    return truthP;
}
//----------------------------------------------------------
const xAOD::TruthParticleContainer* XaodAnalysis::xaodTruthParticles()
{
    if(m_xaodTruthParticles==NULL && m_isMC){
        m_xaodTruthParticles = retrieveTruthParticles(m_event, m_dbg);
    }
    return m_xaodTruthParticles;
}
//----------------------------------------------------------
/// temporary patch, see SUSYToolsTester.cxx @ SUSYTools-00-05-00-14
bool muon_is_safe_for_met(const xAOD::Muon_v1 *mu)
{
    return (mu->muonType()==xAOD::Muon::Combined ||
            mu->muonType()==xAOD::Muon::SegmentTagged ||
            mu->muonType()==xAOD::Muon::MuonStandAlone);
}
/*--------------------------------------------------------------------------------*/
// Build MissingEt
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::retrieveXaodMet( ST::SystInfo sysInfo, SusyNtSys sys)
{
    if(m_dbg>=5) cout << "retrieveXaodMet " << SusyNtSysNames[sys] << endl;

    if(m_metContainer==NULL && sys == NtSys::NOM){
        // DG 2014-09-01 : todo: define 'MySelJets' collection and use it to rebuild 'MET_MyRefFinal'.
        // These placeholder labels are currently hardcoded in SUSYObjDef_xAOD::GetMET()
        // std::pair< xAOD::JetContainer*, xAOD::ShallowAuxContainer* > jets_shallowCopy = xAOD::shallowCopyContainer( *jets );

        m_metContainer = new xAOD::MissingETContainer();
        m_metAuxContainer = new xAOD::MissingETAuxContainer();
        m_metContainer->setStore( m_metAuxContainer );
        //AT 12/12/14: don't need these anymore
        //m_store.record(m_metContainer, "MET_MyRefFinal");
        //m_store.record(m_metAuxContainer, "MET_MyRefFinalAux.");
    }

    xAOD::ElectronContainer* electrons = xaodElectrons(sysInfo,sys);
    xAOD::MuonContainer*     muons     = xaodMuons(sysInfo,sys);
    xAOD::JetContainer*      jets      = xaodJets(sysInfo,sys);
    xAOD::TauJetContainer*   taus      = xaodTaus(sysInfo,sys);
    xAOD::PhotonContainer*   photons   = xaodPhotons(sysInfo,sys);

    //AT 12/16/14: obsolete - done in GetMet
    //xAOD::MuonContainer muons_copy_met(SG::VIEW_ELEMENTS);
    //std::copy_if(muons->begin(), muons->end(), std::back_inserter(muons_copy_met), muon_is_safe_for_met);

    m_susyObj[m_eleIDDefault]->GetMET(*m_metContainer,
                                      jets,
                                      electrons,
                                      muons,
                                      photons,
                                      taus);

}
//----------------------------------------------------------
const xAOD::VertexContainer* XaodAnalysis::retrieveVertices(xAOD::TEvent &e, bool dbg)
{
    const xAOD::VertexContainer* vtx = NULL;
    e.retrieve(vtx, "PrimaryVertices");
    if(dbg){
        if(!vtx) cout<<"XaodAnalysis::retrieveVertices: failed"<<endl;
    }
    return vtx;
}
//----------------------------------------------------------
const xAOD::VertexContainer* XaodAnalysis::xaodVertices()
{
    if(m_xaodVertices==NULL){
        m_xaodVertices = retrieveVertices(m_event, m_dbg);
    }
    return m_xaodVertices;
}

//----------------------------------------------------------
void XaodAnalysis::selectBaselineObjects(SusyNtSys sys, ST::SystInfo sysInfo)
{
    if(m_dbg>=5) cout << "selectBaselineObjects with sys=" <<  SusyNtSysNames[sys] << endl;

    xAOD::ElectronContainer* electrons = xaodElectrons(sysInfo,sys);
    xAOD::MuonContainer* muons =xaodMuons(sysInfo,sys);
    xAOD::JetContainer* jets = xaodJets(sysInfo,sys);
  //  m_susyObj[m_eleIDDefault]->OverlapRemoval(electrons, muons, jets, false);
    m_susyObj[m_eleIDDefault]->OverlapRemoval(xaodElectrons(sysInfo,sys), xaodMuons(sysInfo, sys), xaodJets(sysInfo, sys), false);
  //  xAOD::ElectronContainer* electrons = xaodElectrons(sysInfo,sys);
    int iEl = -1;
    for(const auto& el : *electrons) {
        iEl++;
       // m_susyObj[m_eleIDDefault]->IsSignalElectron(*el);
        m_susyObj[m_eleIDDefault]->IsSignalElectronExp(*el, ST::SignalIsoExp::TightIso);
        if(m_dbg>=5) cout<<"El "
                         <<" pt " << el->pt()
                         <<" eta " << el->eta()
                         <<" phi " << el->phi()
                         <<endl;
      //  if(!el->auxdata< bool >("baseline")) continue;
        //AT:12/16/14 TO UPDATE Base Obj should be after overlap removal
        if( el->auxdata< bool >("baseline") && 
            el->auxdata< bool >("passOR")   &&
            !(fabs(el->eta())>=2.47) ) m_baseElectrons.push_back(iEl);

        if(m_dbg>=5) cout<<"\t El passing"
                         <<" baseline? "<< bool(el->auxdata< bool >("baseline"))
                         <<" signal? "<< bool(el->auxdata< bool >("signal"))
                         <<endl;
        m_preElectrons.push_back(iEl);
    }
    if(m_dbg) cout<<"preElectrons["<<m_preElectrons.size()<<"]"<<endl;

    int iMu = -1;
 //   xAOD::MuonContainer* muons =xaodMuons(sysInfo,sys);
    for(const auto& mu : *muons){
        iMu++;
        m_preMuons.push_back(iMu);
        //m_susyObj[m_eleIDDefault]->IsSignalMuon(*mu);
        m_susyObj[m_eleIDDefault]->IsSignalMuonExp(*mu, ST::SignalIsoExp::TightIso);
        m_susyObj[m_eleIDDefault]->IsCosmicMuon(*mu);
        if(m_dbg>=5) cout<<"Mu passing"
                         <<" baseline? "<< bool(mu->auxdata< bool >("baseline"))
                         <<" signal? "<< bool(mu->auxdata< bool >("signal"))
                         <<" pt " << mu->pt()
                         <<" eta " << mu->eta()
                         <<" phi " << mu->phi()
                         <<endl;
        if( mu->auxdata< bool >("baseline") &&
            mu->auxdata< bool >("passOR")   &&
            !(fabs(mu->eta())>=2.4) ) m_baseMuons.push_back(iMu);
        // if(signal) m_sigMuons.push_back(iMu);
    }
    if(m_dbg) cout<<"preMuons["<<m_preMuons.size()<<"]"<<endl;

    int iJet=-1;
  //  xAOD::JetContainer* jets = xaodJets(sysInfo,sys);
    for(const auto& jet : *jets){
        iJet++;
        m_preJets.push_back(iJet);
        m_susyObj[m_eleIDDefault]->IsBJet(*jet);
        if(m_dbg>=5) cout<<"Jet passing"
                         <<" baseline? "<< bool(jet->auxdata< bool >("baseline"))
                         <<" signal? "<< bool(jet->auxdata< bool >("signal"))
                         <<" pt " << jet->pt()
                         <<" eta " << jet->eta()
                         <<" phi " << jet->phi()
                         <<endl;
        if(jet->auxdata< bool >("baseline") ) m_baseJets.push_back(iJet);
    }
    if(m_dbg) cout<<"preJets["<<m_preJets.size()<<"]"<<endl;

    // overlap removal and met (need to build 'MyJet' coll?)
    //AT:: Depending of what container is affected by systematic, feed the correct set for the met computation
    // dantrim March 2 2015 -- setting "doHarmonization" to False, in accord with Ximo & Fabio
    //m_susyObj[m_eleIDDefault]->OverlapRemoval(xaodElectrons(sysInfo,sys), xaodMuons(sysInfo, sys), xaodJets(sysInfo, sys), false);


    int iTau=-1;
    xAOD::TauJetContainer* taus = xaodTaus(sysInfo,sys);
    for(const auto& tau : *taus){
        iTau++;
        m_susyObj[m_eleIDDefault]->IsSignalTau(*tau);
        if(m_dbg>=5) cout<<"Tau passing"
                         <<" signal? "<< bool(tau->auxdata< bool >("signal"))
                         <<" pt " << tau->pt()
                         <<" eta " << tau->eta()
                         <<" phi " << tau->phi()
                         <<endl;

        if(tau->auxdata< bool >("baseline")) m_preTaus.push_back(iTau);
        //tau->pt()>20*GeV && abs(tau->eta())<2.47
    }
    if(m_dbg) cout<<"m_preTaus["<<m_preTaus.size()<<"]"<<endl;


    //
    //If Nom systematics keep track of the pre_object indices
    //
    if(sys==NtSys::NOM){
        m_preElectrons_nom = m_preElectrons;
        m_preMuons_nom     = m_preMuons;
        m_preJets_nom      = m_preJets;
        m_preTaus_nom      = m_preTaus;
        m_preLeptons_nom   = m_preLeptons;
    }

/**/
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
void XaodAnalysis::selectSignalObjects(SusyNtSys sys, ST::SystInfo sysInfo)
{
    if(m_dbg>=5) cout << "selectSignalObjects with sys=" <<  SusyNtSysNames[sys] << endl;

    int iEl = 0;
    xAOD::ElectronContainer* electrons = xaodElectrons(sysInfo,sys);
    for(const auto& el : *electrons) {
        if( el->auxdata< bool >("signal") &&
            el->auxdata< bool >("passOR") )   m_sigElectrons.push_back(iEl);
            iEl++;
    }
    if(m_dbg) cout<<"m_sigElectrons["<<m_sigElectrons.size()<<"]"<<endl;

    int iMu = 0;
    xAOD::MuonContainer* muons = xaodMuons(sysInfo,sys);
    for(const auto& mu : *muons){
        if( mu->auxdata< bool >("signal") &&
            mu->auxdata< bool >("passOR") )  m_sigMuons.push_back(iMu);
            iMu++;
    }
    if(m_dbg) cout<<"m_sigMuons["<<m_sigMuons.size()<<"]"<<endl;

    int iJet=0;
    xAOD::JetContainer* jets = xaodJets(sysInfo,sys);
    for(const auto& jet : *jets){
        if(jet->pt()*MeV2GeV >20.0 &&
           jet->auxdata< bool >("passOR") &&
           !jet->auxdata< bool >("bad") //AT: Added 12/13/14
           // DG tmp-2014-11-02 (!jet->isAvailable("bad") || !jet->auxdata< bool >("bad"))
            )
            m_sigJets.push_back(iJet);
        iJet++;
    }
    if(m_dbg) cout<<"m_sigJets["<<m_sigJets.size()<<"]"<<endl;

    int iTau=0;
    xAOD::TauJetContainer* taus = xaodTaus(sysInfo,sys);
    for(const auto& tau : *taus){
        if(tau->pt() * MeV2GeV >20.0 &&
           tau->auxdata< bool >("signal"))
            // tau->auxdata< int >("passOR") && // tau not involved in OR?
            m_sigTaus.push_back(iTau);
        iTau++;
    }
    if(m_dbg) cout<<"m_sigTaus["<<m_sigTaus.size()<<"]"<<endl;

    int iPh=0;
    xAOD::PhotonContainer* photons = xaodPhotons(sysInfo,sys);
 //   if(photons) {
    for(const auto& ph : *photons){
        //m_susyObj[m_eleIDDefault]->FillPhoton(ph);
        if(ph->auxdata< bool >("baseline"))
            m_sigPhotons.push_back(iPh);
        iPh++;
    }
 //   }
    if(m_dbg && photons) cout<<"m_sigPhotons["<<m_sigPhotons.size()<<"]"<<endl;

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
//----------------------------------------------------------
void XaodAnalysis::clearOutputObjects(bool deleteNominal)
{
    m_preElectrons.clear();
    m_preMuons.clear();
    m_preJets.clear();
    m_preTaus.clear();
    m_preLeptons.clear();
    m_baseElectrons.clear();
    m_baseMuons.clear();
    m_baseTaus.clear();
    m_baseLeptons.clear();
    m_baseJets.clear();
    m_sigElectrons.clear();
    m_sigMuons.clear();
    m_sigLeptons.clear();
    m_sigJets.clear();
    m_sigTaus.clear();
    m_cutFlags = 0;

    m_sigPhotons.clear();
    m_truParticles.clear();
    m_truJets.clear();

    m_metMuons.clear();

    if(deleteNominal){
        m_preElectrons_nom.clear();
        m_preMuons_nom.clear();
        m_preJets_nom.clear();
        m_preTaus_nom.clear();
        m_preLeptons_nom.clear();
    }


}

/*--------------------------------------------------------------------------------*/
// Count number of good vertices
/*--------------------------------------------------------------------------------*/
uint XaodAnalysis::getNumGoodVtx()
{
    xAOD::VertexContainer::const_iterator pv_itr = m_xaodVertices->begin();
    //AT-2014-10-31: Run2 harmonisation - we don't need to cut on nTrack for run@
    //https://cds.cern.ch/record/1700874/files/ATL-COM-PHYS-2014-451.pdf

    uint nVtx = 0;
    for(auto it=m_xaodVertices->begin(), end=m_xaodVertices->end(); it!=end; ++it){
        const xAOD::Vertex &vtx = **it;
        if(vtx.nTrackParticles() >=5 ) nVtx++;
    }
    return nVtx;

    //AT:2014-10-31: To be change to this for run2 : 2
    //return  m_xaodVertices-size();

}

/*--------------------------------------------------------------------------------*/
// Count number of good vertices
/*--------------------------------------------------------------------------------*/
const xAOD::Vertex* XaodAnalysis::getPV()
{
    xAOD::Vertex* vtx = NULL;
    const xAOD::VertexContainer* vertices = xaodVertices();
    for(auto it=vertices->begin(), end=vertices->end(); it!=end; ++it){
        if((**it).vertexType()==1)  vtx = *it;
    }
    return vtx;
}

/*--------------------------------------------------------------------------------*/
// Match reco jet to a truth jet
/*--------------------------------------------------------------------------------*/
bool XaodAnalysis::matchTruthJet(int iJet)
{
#warning matchTruthJet not implemented
    // // Loop over truth jets looking for a match
    // const TLorentzVector &jetLV = m_susyObj[m_eleIDDefault]->GetJetTLV(iJet);
    // for(int i=0; i<m_event.AntiKt4Truth.n(); i++){
    //   // const xAOD::JetD3PDObjectElement &trueJet = m_event.AntiKt4Truth[i];
    //   // TLorentzVector trueJetLV;
    //   // trueJetLV.SetPtEtaPhiE(trueJet.pt(), trueJet.eta(), trueJet.phi(), trueJet.E());
    //   // if(jetLV.DeltaR(trueJetLV) < 0.3) return true;
    //   }
    return false;
}

/*--------------------------------------------------------------------------------*/
// Return electron type
/*--------------------------------------------------------------------------------*/
bool XaodAnalysis::eleIsOfType(const xAOD::Electron &in, eleID id)
{
    bool type;
    if(in.passSelection(type,eleIDNames[id])) return true;
    return false;
}
/*--------------------------------------------------------------------------------*/
// Event trigger flags
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::fillEventTriggers()
{
    if(m_dbg>=5) cout << "fillEventTriggers" << endl;

    // dantrim trig
    m_evtTrigBits.ResetAllBits();
    for( unsigned int iTrig=0; iTrig < triggerNames.size(); iTrig++) {
        if(m_trigTool->isPassed(triggerNames[iTrig])) m_evtTrigBits.SetBitNumber(iTrig, true);
    }

}

/*--------------------------------------------------------------------------------*/
// Electron trigger matching
/*--------------------------------------------------------------------------------*/
void XaodAnalysis::matchElectronTriggers()
{
    if(m_dbg>=5) cout << "matchElectronTriggers" << endl;
//-DG--  for(uint i=0; i<m_preElectrons.size(); i++){
//-DG--    int iEl = m_preElectrons[i];
//-DG--    const TLorentzVector &lv = m_susyObj[m_eleIDDefault]->GetElecTLV(iEl);
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
    //static int indexEF = -1;
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
//-DG--    const TLorentzVector &lv = m_susyObj[m_eleIDDefault]->GetMuonTLV(iMu);
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
//-DG--    const TLorentzVector &lv = m_susyObj[m_eleIDDefault]->GetTauTLV(iTau);
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
            m_grl->passRunLB(eventinfo->runNumber(), eventinfo->lumiBlock()));
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
                                true); // SUSYToolsTester: move to xAOD tool
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
void XaodAnalysis::assignObjectCleaningFlags(ST::SystInfo sysInfo, SusyNtSys sys)
{
    //AT check if m_cutFalgs save at each systematics ?
    if(passTileHotSpot()) m_cutFlags |= ECut_HotSpot;
    if(passBadJet())      m_cutFlags |= ECut_BadJet;
    if(passBadMuon(sysInfo,sys))     m_cutFlags |= ECut_BadMuon;
    if(passCosmic(sysInfo,sys))      m_cutFlags |= ECut_Cosmic;
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
    //const xAOD::JetContainer *jets =  xaodJets();
    return false;
//  return !IsBadJetEvent(jets, m_baseJets, 20.*GeV, m_susyObj);
}
//----------------------------------------------------------
bool XaodAnalysis::passGoodVtx()
{
    // dantrim-2015-02-14 : following Ximo's method of checking if there is at least one fulfilling...
    bool at_least_one = false;
    for(auto it=m_xaodVertices->begin(), end=m_xaodVertices->end(); it!=end; ++it){
        const xAOD::Vertex *vtx = *it;
        if(vtx->vertexType() == xAOD::VxType::PriVtx) at_least_one = true;
    }
    return at_least_one;

//    return true; // DG-2014-08-16 \todo
//  return PrimaryVertexCut(m_susyObj, &m_event.vxp);
}
//----------------------------------------------------------
bool XaodAnalysis::passTileTrip()
{
    return false;
//  return !m_susyObj[m_eleIDDefault]->IsTileTrip(m_event.eventinfo.RunNumber(), m_event.eventinfo.lbn(), m_event.eventinfo.EventNumber());
}
//----------------------------------------------------------
bool XaodAnalysis::passBadMuon(ST::SystInfo sysInfo, SusyNtSys sys)
{
    xAOD::MuonContainer* muons = xaodMuons(sysInfo, sys);
    for(auto it=muons->begin(), end=muons->end(); it!=end; ++it){
        const xAOD::Muon &mu = **it;
        //AT-2014-10-30  should be done only for preMuons ? any more cuts to applied ?
        if(m_susyObj[m_eleIDDefault]->IsBadMuon(mu)) return false;
    }
    return true;
    //  return !IsBadMuonEvent(m_susyObj, xaodMuons(), m_preMuons, 0.2);
}
//----------------------------------------------------------
bool XaodAnalysis::passCosmic(ST::SystInfo sysInfo, SusyNtSys sys)
{
    xAOD::MuonContainer* muons = xaodMuons(sysInfo, sys);
    for(auto it=muons->begin(), end=muons->end(); it!=end; ++it){
        const xAOD::Muon &mu = **it;
  //      if(!mu.auxdata< bool >("baseline")) continue; // dantrim -- removing this, cutflow is in line with Maria : Feb 14 2015
 //   //    if(!mu.auxdata< bool >("passOR")) continue; // dantrim -- in line with selectSignalObjects : Feb 14 2015
        if(m_susyObj[m_eleIDDefault]->IsCosmicMuon(mu)) return false;

     //   return(mu.auxdata<bool>("baseline") && mu.auxdata<bool>("passOR") && !m_susyObj[m_eleIDDefault]->IsCosmicMuon(mu));
    }
     return true;
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
float XaodAnalysis::getPileupWeight(const xAOD::EventInfo* eventinfo)
{
    if(!m_isMC) return 1;
    if(eventinfo->runNumber() == 222222) return 1; //Cannot yet reweight mc14_13TeV

    m_pileupReweightingTool->execute();
    return xaodEventInfo()->auxdata< double >( "PileupWeight" );
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
        //     lepSF *= m_susyObj[m_eleIDDefault]->GetSignalElecSF(el->cl_eta(), lep.lv()->Pt(), true, true, false);
        //   }
        //   // Muons
        //   else{
        //     lepSF *= m_susyObj[m_eleIDDefault]->GetSignalMuonSF(lep.idx());
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
    //AT-2014-10-30 Not yet implemented for DC14
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
    // if(metFlav=="STVF") m_metFlavor = SUSYMet::STVF;
    // else if(metFlav=="STVF_JVF") m_metFlavor = SUSYMet::STVF_JVF;
    // else if(metFlav=="Default") m_metFlavor = SUSYMet::Default;
    // else{
    //   cout << "XaodAnalysis::setMetFlavor : ERROR : MET flavor " << metFlav
    //        << " is not supported!" << endl;
    //   abort();
    // }
}
//----------------------------------------------------------
void XaodAnalysis::dumpEvent()
{
    const xAOD::EventInfo* eventinfo = xaodEventInfo();
    cout<<(*eventinfo)<<endl;
}
//----------------------------------------------------------
void XaodAnalysis::dumpBaselineObjects()
{
    //uint nEle = m_baseElectrons.size();
    //uint nMu  = m_baseMuons.size();
    //uint nTau = m_baseTaus.size();
    //uint nJet = m_baseJets.size();

#warning dumpBaselineObjects not implemented
    // cout.precision(2);
    // if(nEle){
    //   cout << "Baseline electrons" << endl;
    //   for(uint i=0; i < nEle; i++){
    //     int iEl = m_baseElectrons[i];
    //     const TLorentzVector &lv = m_susyObj[m_eleIDDefault]->GetElecTLV(iEl);
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
    //     const TLorentzVector &lv = m_susyObj[m_eleIDDefault]->GetMuonTLV(iMu);
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
    //     const TLorentzVector &lv = m_susyObj[m_eleIDDefault]->GetJetTLV(iJet);
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
    //     const TLorentzVector &lv = m_susyObj[m_eleIDDefault]->GetElecTLV(iEl);
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
    //     const TLorentzVector &lv = m_susyObj[m_eleIDDefault]->GetMuonTLV(iMu);
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
    //     const TLorentzVector &lv = m_susyObj[m_eleIDDefault]->GetJetTLV(iJet);
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
    bool isSimulation = xaodEventInfo()->eventType( xAOD::EventInfo::IS_SIMULATION );
    bool isData = !isSimulation;
    if(m_isMC != isSimulation) {
        valid=false;
        if(m_dbg)
            cout<<"XaodAnalysis::runningOptionsAreValid invalid isMc:"
                <<" (m_isMC:"<<m_isMC<<" != isSimulation:"<<isSimulation<<")"
                <<endl;
    }
    if(isData) { // check stream
        const std::vector< xAOD::EventInfo::StreamTag > &streams= xaodEventInfo()->streamTags();
        vector<string> streamnames(streams.size());
        std::transform(streams.begin(), streams.end(), streamnames.begin(),
                       [](const xAOD::EventInfo::StreamTag &s) {
                           cout << "AT:  stream " << s.name()<< endl;
                           return s.name();
                       });
        bool isEgamma = (find(streamnames.begin(), streamnames.end(), "Egamma") != streamnames.end());
        bool isJetEt  = (find(streamnames.begin(), streamnames.end(), "JetTauEtmiss") != streamnames.end());
        bool isMuons  = (find(streamnames.begin(), streamnames.end(), "Muons") != streamnames.end());
        bool consistentStream = (isMuons  ? m_stream==Stream_Muons :
                                 isEgamma ? m_stream==Stream_Egamma :
                                 isJetEt  ? m_stream==Stream_JetTauEtmiss :
                                 false);
        if(!consistentStream) {
            valid=false;
            if(m_dbg)
                cout<<"XaodAnalysis::runningOptionsAreValid: inconsistent stream"
                    <<" m_stream: "
                    <<(m_stream==Stream_Muons        ? "Stream_Muons":
                       m_stream==Stream_Egamma       ? "Stream_Egamma":
                       m_stream==Stream_JetTauEtmiss ? "Stream_JetTauEtmiss":
                       "unknown")
                    <<" eventinfo: "
                    <<accumulate(streamnames.begin(), streamnames.end(), std::string(),
                                 [](const std::string& a, const std::string& b) -> std::string {
                                     return a + (a.length() > 0 ? "," : "") + b;
                                 })
                    <<endl;

        }
    } // isData
    if(m_dbg)
        cout<<"XaodAnalysis::runningOptionsAreValid(): "<<(valid?"true":"false")<<endl;
    return valid;
}
//----------------------------------------------------------
std::string XaodAnalysis::defauldGrlFile()
{
    return std::string( "$ROOTCOREBIN/data/SUSYTools/GRL/Summer2013/"
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
    cout<<"isSimu: ("<<s<<") "<<(!XaodAnalysis::isDataFromSamplename(s))<<endl;
    return !XaodAnalysis::isDataFromSamplename(s);
}
//----------------------------------------------------------
bool XaodAnalysis::isDerivationFromSamplename(const TString &sample)
{
    bool is_derived = false;
    if ( sample.Contains("derived", TString::kIgnoreCase) ) { 
        is_derived = true; 
        cout << "This file is a derivation" << endl;
    }
    
    return is_derived;
}
//----------------------------------------------------------
void XaodAnalysis::selectObjects(SusyNtSys sys, ST::SystInfo sysInfo)
{
    selectBaselineObjects(sys, sysInfo);
    selectSignalObjects(sys,sysInfo);
//--DG-- todo     if(m_selectTruth) selectTruthObjects();
}
//----------------------------------------------------------
XaodAnalysis& XaodAnalysis::deleteShallowCopies(bool deleteNominal)
{
    if(m_dbg>5) cout << "deleteShallowCopies " << deleteNominal << endl;

    if(m_xaodMuons        ) delete m_xaodMuons;
    if(m_xaodMuonsAux     ) delete m_xaodMuonsAux;
    if(m_xaodElectrons    ) delete m_xaodElectrons;
    if(m_xaodElectronsAux ) delete m_xaodElectronsAux;
    if(m_xaodTaus         ) delete m_xaodTaus;
    if(m_xaodTausAux      ) delete m_xaodTausAux;
    if(m_xaodJets         ) delete m_xaodJets;
    if(m_xaodJetsAux      ) delete m_xaodJetsAux;
    if(m_xaodPhotons      ) delete m_xaodPhotons;
    if(m_xaodPhotonsAux   ) delete m_xaodPhotonsAux;

    if(m_dbg>5) cout << "Check delete shallowCopied mu " << m_xaodMuons
                     << " ele " << m_xaodElectrons
                     << " pho " << m_xaodPhotons
                     << " jets " << m_xaodJets
                     << " taus " << m_xaodTaus << endl;


    if(deleteNominal){
    //    m_store.print();  // annoying print out - dantrim : Feb 14 2015
        m_store.clear(); // this clears m_metContainer and the objs recorded with TStore - AT: 12/12/14- not needed anymore
    //    m_store.print();

        if(m_xaodMuons_nom       ) delete m_xaodMuons_nom;
        if(m_xaodMuonsAux_nom    ) delete m_xaodMuonsAux_nom;
        if(m_xaodElectrons_nom   ) delete m_xaodElectrons_nom;
        if(m_xaodElectronsAux_nom) delete m_xaodElectronsAux_nom;
        if(m_xaodTaus_nom        ) delete m_xaodTaus_nom;
        if(m_xaodTausAux_nom     ) delete m_xaodTausAux_nom;
        if(m_xaodJets_nom        ) delete m_xaodJets_nom;
        if(m_xaodJetsAux_nom     ) delete m_xaodJetsAux_nom;
        if(m_xaodPhotons_nom     ) delete m_xaodPhotons_nom;
        if(m_xaodPhotonsAux_nom  ) delete m_xaodPhotonsAux_nom;

        if(m_metContainer    ) delete m_metContainer;
        if(m_metAuxContainer ) delete m_metAuxContainer;
    }


    return *this;
}
//----------------------------------------------------------
XaodAnalysis& XaodAnalysis::clearContainerPointers(bool deleteNominal)
{
    m_xaodEventInfo      = NULL;
    m_xaodMuons          = NULL;
    m_xaodMuonsAux       = NULL;
    m_xaodElectrons      = NULL;
    m_xaodElectronsAux   = NULL;
    m_xaodTaus           = NULL;
    m_xaodTausAux        = NULL;
    m_xaodJets           = NULL;
    m_xaodJetsAux        = NULL;
    m_xaodPhotons        = NULL;
    m_xaodPhotonsAux     = NULL;
    m_xaodTruthEvent     = NULL;
    m_xaodTruthParticles = NULL;
    m_metJets            = NULL;
    m_metJetsAux         = NULL;
    m_xaodVertices       = NULL;

    if(deleteNominal){
        m_xaodMuons_nom          = NULL;
        m_xaodMuonsAux_nom       = NULL;
        m_xaodElectrons_nom      = NULL;
        m_xaodElectronsAux_nom   = NULL;
        m_xaodTaus_nom           = NULL;
        m_xaodTausAux_nom        = NULL;
        m_xaodJets_nom           = NULL;
        m_xaodJetsAux_nom        = NULL;
        m_xaodPhotons_nom        = NULL;
        m_xaodPhotonsAux_nom     = NULL;
        m_metContainer           = NULL;
        m_metAuxContainer        = NULL;
    }


    return *this;
}
//----------------------------------------------------------
XaodAnalysis& XaodAnalysis::retrieveCollections()
{
    if(m_dbg) cout << "XaodAnalysis::retrieveCollections " << endl;
    xaodEventInfo();
    const xAOD::EventInfo* eventinfo = XaodAnalysis::xaodEventInfo();
    if(m_dbg) cout << " run " << setw(6) << eventinfo->runNumber()
                   << " event " << setw(7) << eventinfo->eventNumber() << endl;

    //Retrieve containers at nominal scale
    xaodElectrons(systInfoList[0]);
    xaodMuons(systInfoList[0]);
    xaodJets(systInfoList[0]);
    xaodTaus(systInfoList[0]);
    xaodPhotons(systInfoList[0]);
    xaodTruthEvent();
    xaodTruthParticles();
    retrieveXaodMet(systInfoList[0]);//nominal
    xaodVertices();
    return *this;
}
