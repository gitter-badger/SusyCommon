//#include "egammaAnalysisUtils/CaloIsoCorrection.h"

//#include "TauCorrections/TauCorrections.h"
#include "TauCorrUncert/TauSF.h"
//#include "SUSYTools/MV1.h"

#include "SusyCommon/SusyNtMaker.h"
#include "SusyCommon/TruthTools.h"
#include "SusyCommon/SusyObjId.h"
#include "SusyNtuple/SusyNtTools.h"
#include "SusyNtuple/WhTruthExtractor.h"
#include "SusyNtuple/mc_truth_utils.h"
#include "SusyNtuple/RecoTruthClassification.h"

#include "ElectronEfficiencyCorrection/TElectronEfficiencyCorrectionTool.h"

#include "xAODPrimitives/IsolationType.h"
#include "xAODTracking/TrackParticle.h"
#include "xAODTracking/TrackParticlexAODHelpers.h"  // xAOD::TrackingHelpers
#include "xAODEgamma/EgammaxAODHelpers.h"

// Amg include
#include "EventPrimitives/EventPrimitivesHelpers.h"

#include "SusyNtuple/TriggerTools.h"


#include <algorithm> // max_element
#include <iomanip> // setw
#include <sstream> // std::ostringstream
#include <string>
#include <iostream>

using namespace std;
namespace smc =Susy::mc;


using Susy::SusyNtMaker;

//----------------------------------------------------------
SusyNtMaker::SusyNtMaker() :
    m_outTreeFile(NULL),
    m_outTree(NULL),
    m_susyNt(0),
    m_fillNt(true),
    m_filter(true),
    m_nLepFilter(0),
    m_nLepTauFilter(2),
    m_filterTrigger(false),
    m_saveContTaus(false),
    m_isWhSample(false),
    m_hDecay(0),
    m_hasSusyProp(false),
    h_rawCutFlow(NULL),
    h_genCutFlow(NULL),
    m_cutstageCounters(SusyNtMaker::cutflowLabels().size(), 0),
    h_passTrigLevel(NULL)
{
    n_pre_ele=0;
    n_pre_muo=0;
    n_pre_tau=0;
    n_pre_jet=0;
    n_base_ele=0;
    n_base_muo=0;
    n_base_tau=0;
    n_base_jet=0;
    n_sig_ele=0;
    n_sig_muo=0;
    n_sig_tau=0;
    n_sig_jet=0;

    //AT-2014-11-05: Correct place to put this ?
    CP::SystematicCode::enableFailure();
}
//----------------------------------------------------------
SusyNtMaker::~SusyNtMaker()
{
}
//----------------------------------------------------------
void SusyNtMaker::SlaveBegin(TTree* tree)
{
    XaodAnalysis::SlaveBegin(tree);
    if(m_dbg)
        cout<<"SusyNtMaker::SlaveBegin"<<endl;
    if(m_fillNt || true)
        initializeOuputTree();
    m_isWhSample = guessWhetherIsWhSample(m_sample);
    initializeCutflowHistograms();

    m_timer.Start();
}
//----------------------------------------------------------
const std::vector< std::string > SusyNtMaker::cutflowLabels()
{
    vector<string> labels;
    labels.push_back("Initial"        );
    labels.push_back("GRL"            );
    labels.push_back("error flags"    );
    labels.push_back("good pvx"       );
    labels.push_back("bad muon"       );
    labels.push_back("pass cosmic"    );
    labels.push_back("jet cleaning"   );
    labels.push_back("base lepton >= 1"   );
    labels.push_back("sig. lepton >= 1"   );
  //  labels.push_back("1 == base jet"  );
  //  labels.push_back("1 == sig. jet"  );
  //  labels.push_back("SusyProp Veto"  );
  //  labels.push_back("GRL"            );
  //  labels.push_back("LAr Error"      );
  //  labels.push_back("Tile Error"     );
  //  labels.push_back("TTC Veto"       );
  //  labels.push_back("Good Vertex"    );
  //  labels.push_back("Buggy WWSherpa" );
  //  labels.push_back("Hot Spot"       );
  //  labels.push_back("Bad Jet"        );
  //  labels.push_back("Bad Muon"       );
  //  labels.push_back("Cosmic"         );
  //  labels.push_back(">=1 lep"        );
  //  labels.push_back(">=2 base lep"   );
  //  labels.push_back(">=2 lep"        );
  //  labels.push_back("==3 lep"        );
    return labels;
}
//----------------------------------------------------------
TH1F* SusyNtMaker::makeCutFlow(const char* name, const char* title)
{
    vector<string> labels = SusyNtMaker::cutflowLabels();
    int nCuts = labels.size();
    TH1F* h = new TH1F(name, title, nCuts, 0., static_cast<float>(nCuts));
    for(int iCut=0; iCut<nCuts; ++iCut)
        h->GetXaxis()->SetBinLabel(iCut+1, labels[iCut].c_str());
    return h;
}
//----------------------------------------------------------
TH1F* SusyNtMaker::getProcCutFlow(int signalProcess)
{
    // Look for it on the map
    map<int,TH1F*>::const_iterator it = m_procCutFlows.find(signalProcess);
    // New process
    if(it == m_procCutFlows.end()){
        stringstream stream;
        stream << signalProcess;
        string name = "procCutFlow" + stream.str();
        return m_procCutFlows[signalProcess] = makeCutFlow(name.c_str(),
                                                           (name+";Cuts;Events").c_str());
    }
    // Already saved process
    else{
        return it->second;
    }
}
//----------------------------------------------------------
Bool_t SusyNtMaker::Process(Long64_t entry)
{
    static Long64_t chainEntry = -1;
    chainEntry++;
    m_event.getEntry(chainEntry); // DG 2014-09-19 TEvent wants the chain entry, not the tree entry (?)

    retrieveCollections();
    const xAOD::EventInfo* eventinfo = XaodAnalysis::xaodEventInfo();
    if(m_dbg || chainEntry%5000==0){
        cout << "***********************************************************" << endl;
        cout << "**** Processing entry " << setw(6) << chainEntry
             << " run " << setw(6) << eventinfo->runNumber()
             << " event " << setw(7) << eventinfo->eventNumber() << " ****" << endl;
        cout << "***********************************************************" << endl;
    }
    if(!m_flagsHaveBeenChecked) {
        m_flagsAreConsistent = runningOptionsAreValid();
        m_flagsHaveBeenChecked=true;
        if(!m_flagsAreConsistent) {
            cout<<"ERROR: Inconsistent options. Stopping here."<<endl;
            abort();
        }
    }

    fillTriggerHisto();
    if(selectEvent() && m_fillNt){
        matchTriggers();
        // dantrim Jul 2 2015
     //   if(m_isMC) { 
     //       //xaodTruthParticles();
     //       m_tauTruthMatchingTool->setTruthParticleContainer(xaodTruthParticles());
     //       m_tauTruthMatchingTool->initializeEvent();
     //   }

        //if(m_isMC){
        //    //m_tauTruthMatchingTool->setTruthParticleContainer(xaodTruthParticles()); // comment out for memory leak check
        //    //m_tauTruthMatchingTool->createTruthTauContainer(); // DA: gets called automatically when calling setTruthParticleContainer
        //}
        fillNtVars();
        if(m_isMC && m_sys) doSystematic();
        int bytes = m_outTree->Fill();
        if(bytes==-1){
            cout << "SusyNtMaker ERROR filling tree!  Abort!" << endl;
            abort();
        }
    }
    clearOutputObjects();
    deleteShallowCopies();
    return kTRUE;
}
//----------------------------------------------------------
void SusyNtMaker::Terminate()
{
    XaodAnalysis::Terminate();
    m_timer.Stop();
    if(m_dbg) cout<<"SusyNtMaker::Terminate"<<endl;
    cout<<counterSummary()<<endl;
    cout<<timerSummary()<<endl;
    if(m_fillNt) saveOutputTree();
}
//----------------------------------------------------------
bool isSimplifiedModel(const TString &sampleName)
{
    return sampleName.Contains("simplifiedModel");
}
//----------------------------------------------------------
bool SusyNtMaker::selectEvent()
{
    if(m_dbg>=5) cout << "selectEvent" << endl;
    clearOutputObjects();
    m_susyNt.clear();
    bool pass_event_and_object_sel = (passEventlevelSelection() && passObjectlevelSelection());
    bool keep_all_events(!m_filter);
    return (pass_event_and_object_sel || keep_all_events);
}
//----------------------------------------------------------
void SusyNtMaker::fillNtVars()
{
    fillEventVars();
    fillElectronVars();
    fillMuonVars();
    fillTauVars();
    fillJetVars();
    fillMetVars();
    fillTrackMetVars();
    fillPhotonVars();
    if(m_isMC && getSelectTruthObjects() ) {
        fillTruthParticleVars();
        fillTruthJetVars();
        fillTruthMetVars();
    }
}
//----------------------------------------------------------
void SusyNtMaker::fillEventVars()
{
    if(m_dbg>=5) cout << "fillEventVars" << endl;
    Susy::Event* evt = m_susyNt.evt();
    const xAOD::EventInfo* eventinfo = XaodAnalysis::xaodEventInfo();

    evt->run              = eventinfo->runNumber();
    evt->eventNumber      = eventinfo->eventNumber();
    evt->lb               = eventinfo->lumiBlock();
    evt->stream           = m_stream;

    evt->isMC             = m_isMC;
    evt->mcChannel        = m_isMC? eventinfo->mcChannelNumber() : 0;
    evt->w                = m_isMC? eventinfo->mcEventWeight()   : 1; // \todo DG this now has an arg, is 0 the right one?
    
    // pre-skimmed counters (i.e. prior to derivation framework)
    evt->initialNumberOfEvents    = m_nEventsProcessed;
    evt->sumOfEventWeights        = m_sumOfWeights;
    evt->sumOfEventWeightsSquared = m_sumOfWeightsSquared;


    evt->nVtx             = getNumGoodVtx();

    evt->hfor             = m_isMC? getHFORDecision() : -1;

    // SUSY final state
    evt->susyFinalState   = m_susyFinalState;
    // \todo for later (DG could become obsolete?)
    // evt->susySpartId1     = m_event.SUSY.Spart1_pdgId.IsAvailable()? m_event.SUSY.Spart1_pdgId() : 0;
    // evt->susySpartId2     = m_event.SUSY.Spart2_pdgId.IsAvailable()? m_event.SUSY.Spart2_pdgId() : 0;

    float mZ = -1.0, mZtruthMax = 40.0;
    if(m_isMC){
        // int dsid = m_event.eventinfo.mc_channel_number();
        // if(IsAlpgenLowMass(dsid) || IsAlpgenPythiaZll(dsid)) mZ = MllForAlpgen(&m_event.mc);
        // else if(IsSherpaZll(dsid)) mZ = MllForSherpa(&m_event.mc);
    }
    evt->mllMcTruth       = mZ;
    evt->passMllForAlpgen = m_isMC ? (mZ < mZtruthMax) : true;
    evt->hDecay           = m_hDecay;
    evt->eventWithSusyProp= m_hasSusyProp;
    
    evt->trigBits         = m_evtTrigBits;

    evt->wPileup          = m_isMC ? m_susyObj[m_eleIDDefault]->GetPileupWeight() : 1;

    // in recent versions of PRW tool the 'getLumiBlockMu' should
    // be able to return the MC mu
    evt->avgMu            = m_isMC ? eventinfo->averageInteractionsPerCrossing() : m_pileupReweightingTool->getLumiBlockMu(*eventinfo);

    // Pileup systematic variations, varying data mu up/down and getting the resulting pupw
    if(m_isMC && m_sys) {
        for(const auto& sysInfo : systInfoList) {
            if(!(sysInfo.affectsType == ST::SystObjType::EventWeight && sysInfo.affectsWeights)) continue;
            const CP::SystematicSet& sys = sysInfo.systset;
            SusyNtSys ourSys = CPsys2sys((sys.name()).c_str());
            if(!(ourSys==NtSys::PILEUP_UP || ourSys==NtSys::PILEUP_DN)) continue;
            bool do_down = ourSys==NtSys::PILEUP_DN;
            // configure the tools
            if ( m_susyObj[m_eleIDDefault]->applySystematicVariation(sys) != CP::SystematicCode::Ok) {
                cout << "SusyNtMaker::fillEventVars    cannot configure SUSYTools for systematic " << sys.name() << " (" << SusyNtSysNames[ourSys] << ")" << endl;
                continue;
            }
            // get and store
            if(do_down) { evt->wPileup_dn = m_susyObj[m_eleIDDefault]->GetPileupWeight(); }
            else { evt->wPileup_up = m_susyObj[m_eleIDDefault]->GetPileupWeight(); }
        } // sysInfo
        if( m_susyObj[m_eleIDDefault]->resetSystematics() != CP::SystematicCode::Ok) {
            cout << "SusyNtMaker::fillEventVars    cannot rest SUSYTools systematics. Aborting." << endl;
            abort();
        }
    }

    if(m_isMC){
        xAOD::TruthEventContainer::const_iterator truthE_itr = xaodTruthEvent()->begin();
/*
  AT: test 05-08-15: still crashes
        ( *truthE_itr )->pdfInfoParameter(evt->pdf_id1   , xAOD::TruthEvent::PDGID1); // not available for some samples
        ( *truthE_itr )->pdfInfoParameter(evt->pdf_id2   , xAOD::TruthEvent::PDGID2);
        ( *truthE_itr )->pdfInfoParameter(evt->pdf_x1    , xAOD::TruthEvent::X1);
        ( *truthE_itr )->pdfInfoParameter(evt->pdf_x2    , xAOD::TruthEvent::X2);
        ( *truthE_itr )->pdfInfoParameter(evt->pdf_scale , xAOD::TruthEvent::SCALE);
*/
        // DG what are these two?
        //( *truthE_itr )->pdfInfoParameter(evt->pdf_x1   , xAOD::TruthEvent::x1);
        //( *truthE_itr )->pdfInfoParameter(evt->pdf_x2   , xAOD::TruthEvent::x2);
    }
    evt->pdfSF            = m_isMC? getPDFWeight8TeV() : 1;
    m_susyNt.evt()->cutFlags[NtSys::NOM] = m_cutFlags;
}
//----------------------------------------------------------
void SusyNtMaker::fillElectronVars()
{
    if(m_dbg>=5) cout<<"fillElectronVars"<<endl;
    xAOD::ElectronContainer* electrons = XaodAnalysis::xaodElectrons(systInfoList[0]);
    for(auto &i : m_preElectrons){
        storeElectron(*(electrons->at(i)));
    }
}
//----------------------------------------------------------
void SusyNtMaker::fillMuonVars()
{
    if(m_dbg>=5) cout<<"fillMuonVars"<<endl;
    xAOD::MuonContainer* muons = XaodAnalysis::xaodMuons(systInfoList[0]);
    for(auto &i : m_preMuons){
        storeMuon(*(muons->at(i)));
    }
}
//----------------------------------------------------------
void SusyNtMaker::fillJetVars()
{
    if(m_dbg>=5) cout<<"fillJetVars"<<endl;
    xAOD::JetContainer* jets = XaodAnalysis::xaodJets(systInfoList[0]);
    /////////////////////
    // Not super smart but tag along for the moment - ASM 25/5/2015
    if(m_isMC) m_susyObj[m_eleIDDefault]->BtagSF(jets); // Decorate the jets w/ effscalefact
    /////////////////////
    for(auto &i : m_preJets){
            storeJet(*(jets->at(i)));
    }
}
//----------------------------------------------------------
void SusyNtMaker::fillTauVars()
{
    if(m_dbg>=5) cout<<"fillTauVars"<<endl;
    xAOD::TauJetContainer* taus =  XaodAnalysis::xaodTaus(systInfoList[0]);
    // vector<int>& saveTaus = m_saveContTaus? m_contTaus : m_preTaus; // container taus are meant to be used only for background estimates?
    vector<int>& saveTaus = m_preTaus;
    for(auto &i : saveTaus){
        storeTau(*(taus->at(i)));
    }
}
//----------------------------------------------------------
void SusyNtMaker::fillPhotonVars()
{
    if(m_dbg>=5) cout<<"fillPhotonVars"<<endl;
    const xAOD::PhotonContainer* photons = XaodAnalysis::xaodPhotons(systInfoList[0]);
    for(auto &i : m_sigPhotons){
        storePhoton(*(photons->at(i)));
    }
}
//----------------------------------------------------------
bool isMcAtNloTtbar(const int &channel) { return channel==105200; }
//----------------------------------------------------------
void SusyNtMaker::fillTruthParticleVars()
{
    if(m_dbg>=5) cout<<"fillTruthParticleVars"<<endl;
    // DG-2014-08-29 todo
    // // Retrieve indicies -- should go elsewhere
    // m_truParticles        = m_recoTruthMatch.LepFromHS_McIdx();
    // vector<int> truthTaus = m_recoTruthMatch.TauFromHS_McIdx();
    // m_truParticles.insert( m_truParticles.end(), truthTaus.begin(), truthTaus.end() );
    // if(m_isMC && isMcAtNloTtbar(m_event.eventinfo.mc_channel_number())){
    //   vector<int> ttbarPart(WhTruthExtractor::ttbarMcAtNloParticles(m_event.mc.pdgId(),
    //                                                                 m_event.mc.child_index()));
    //   m_truParticles.insert(m_truParticles.end(), ttbarPart.begin(), ttbarPart.end());
    // }
    const xAOD::TruthParticleContainer* particles = xaodTruthParticles();
    for(auto &i : m_truParticles){
        storeTruthParticle(*(particles->at(i)));
    }
}

//----------------------------------------------------------
void SusyNtMaker::storeElectron(const xAOD::Electron &in)
{
    if(m_dbg>=15) cout<<"SusyNtMaker::storeElectron pT "<< in.pt()*MeV2GeV <<endl;
    const xAOD::EventInfo* eventinfo = XaodAnalysis::xaodEventInfo();

    Susy::Electron out;
    //////////////////////////////////////
    // 4-vector
    //////////////////////////////////////
    double pt(in.pt()*MeV2GeV), eta(in.eta()), phi(in.phi()), m(in.m()*MeV2GeV);
    out.SetPtEtaPhiM(pt, eta, phi, m);
    out.pt  = pt;
    out.eta = eta;
    out.phi = phi;
    out.m   = m;
    out.q   = in.charge();

    //////////////////////////////////////
    // SUSYTools flags (flags from default
    // SUSYTools object)
    //////////////////////////////////////
    out.isBaseline = in.auxdata< char >("baseline");
    out.isSignal   = in.auxdata< char >("signal");
    bool all_available=true;

    //////////////////////////////////////
    // Electron LH ID 
    //////////////////////////////////////
    out.veryLooseLH   = eleIsOfType(in, ElectronId::VeryLooseLH);
    out.looseLH       = eleIsOfType(in, ElectronId::LooseLH);
    out.mediumLH      = eleIsOfType(in, ElectronId::MediumLH);
    out.tightLH       = eleIsOfType(in, ElectronId::TightLH);
    out.looseLH_nod0  = eleIsOfType(in, ElectronId::LooseLH_nod0);
    out.mediumLH_nod0 = eleIsOfType(in, ElectronId::MediumLH_nod0);
    out.tightLH_nod0  = eleIsOfType(in, ElectronId::TightLH_nod0);

    //////////////////////////////////////
    // Isolation flags (IsolationSelectionTool)
    //////////////////////////////////////
    out.isoGradientLoose  = in.auxdata<char>("isol");
   //out.isoGradientLoose  = m_isoToolGradientLooseCone40Calo->accept(in) ? true : false;
    out.isoGradient       = m_isoToolGradientCone40->accept(in) ? true : false;
    out.isoLooseTrackOnly = m_isoToolLooseTrackOnlyCone20->accept(in) ? true : false;
    out.isoLoose          = m_isoToolLoose->accept(in) ? true : false;
    out.isoTight          = m_isoToolTight->accept(in) ? true : false;

    //////////////////////////////////////
    // Isolation variables
    //////////////////////////////////////
    out.etconetopo20 = in.isolationValue(xAOD::Iso::topoetcone20) * MeV2GeV;
    out.etconetopo30 = in.isolationValue(xAOD::Iso::topoetcone30) * MeV2GeV;
    out.ptcone20 = in.isolationValue(xAOD::Iso::ptcone20) * MeV2GeV;
    out.ptcone30 = in.isolationValue(xAOD::Iso::ptcone30) * MeV2GeV;
    out.ptvarcone20 = in.auxdataConst<float>("ptvarcone20") * MeV2GeV;
    out.ptvarcone30 = in.auxdataConst<float>("ptvarcone30") * MeV2GeV;
    //Update for Rel 20
    //out.ptvarcone20 = in.isolationValue(xAOD::Iso::ptvarcone20) * MeV2GeV;
    //out.ptvarcone30 = in.isolationValue(xAOD::Iso::ptvarcone30) * MeV2GeV;
    
    if(m_dbg>=10) 
        cout << "AT: storing in susyNt electron Et "
             << out.pt
             << " LH type "
             << out.veryLooseLH << " "  
             << out.looseLH << " "  
             << out.mediumLH << " "  
             << out.tightLH 
             << endl;

    // toggles for which SF to include in electron SF 
    bool recoSF=true;
    bool idSF=true;
    bool trigSF=false;

    if(m_isMC){
        //////////////////////////////////////
        // Lepton SF
        // - one for each electron LH WP
        // - (only Loose, Medium, Tight for now)
        //////////////////////////////////////
        out.eleEffSF[ElectronId::TightLH] = m_susyObj[SusyObjId::eleTightLH]->GetSignalElecSF(in, recoSF, idSF, trigSF);
        out.eleEffSF[ElectronId::MediumLH] = m_susyObj[SusyObjId::eleMediumLH]->GetSignalElecSF(in, recoSF, idSF, trigSF);
        out.eleEffSF[ElectronId::LooseLH] = m_susyObj[SusyObjId::eleLooseLH]->GetSignalElecSF(in, recoSF, idSF, trigSF);

        //////////////////////////////////////
        // Truth matching/info
        //////////////////////////////////////
        out.mcType = xAOD::TruthHelpers::getParticleTruthType(in);
        out.mcOrigin = xAOD::TruthHelpers::getParticleTruthOrigin(in);  
        const xAOD::TruthParticle* truthEle = xAOD::TruthHelpers::getTruthParticle(in);
        out.matched2TruthLepton   = truthEle ? true : false;
        int matchedPdgId = truthEle ? truthEle->pdgId() : -999;
        out.truthType  = isFakeLepton(out.mcOrigin, out.mcType, matchedPdgId); 
        //AT: 05-02-15: Issue accessing Aux of trackParticle in truthElectronCharge. Info not in derived AOD ?
        //if(eleIsOfType(in, eleID::LooseLH))
        // crash p1874 out.isChargeFlip  = m_isMC ? isChargeFlip(in.charge(),truthElectronCharge(in)) : false;
    }

    //////////////////////////////////////
    // Systematic variation on electron SF
    //////////////////////////////////////
    if(m_isMC && m_sys && (out.veryLooseLH || out.looseLH || out.mediumLH || out.tightLH)) {
        for(const auto& sysInfo : systInfoList) {
            if(!(sysInfo.affectsType == ST::SystObjType::Electron && sysInfo.affectsWeights)) continue;

            const CP::SystematicSet& sys = sysInfo.systset;
            SusyNtSys ourSys = CPsys2sys((sys.name()).c_str());
            for(int i=SusyObjId::eleTightLH; i<=SusyObjId::eleLooseLH; i++){
                if(m_susyObj[i]->applySystematicVariation(sys) != CP::SystematicCode::Ok) {
                    cout << "SusyNtMaker::storeElectron    cannot configure SUSYTools for systematic " << sys.name() << endl;
                    continue;
                }
            } // i 
            vector<float> sf;
            sf.assign(ElectronId::ElectronIdInvalid, 1);
            sf[ElectronId::TightLH]  = m_susyObj[SusyObjId::eleTightLH] ->GetSignalElecSF(in, recoSF, idSF, trigSF);
            sf[ElectronId::MediumLH] = m_susyObj[SusyObjId::eleMediumLH]->GetSignalElecSF(in, recoSF, idSF, trigSF);
            sf[ElectronId::LooseLH]  = m_susyObj[SusyObjId::eleLooseLH] ->GetSignalElecSF(in, recoSF, idSF, trigSF);

            for(int i=ElectronId::TightLH; i<ElectronIdInvalid; i++){
                if     (ourSys == NtSys::EL_EFF_ID_TotalCorrUncertainty_UP)   out.errEffSF_id_corr_up[i]   = sf[i] - out.eleEffSF[i];
                else if(ourSys == NtSys::EL_EFF_ID_TotalCorrUncertainty_DN)   out.errEffSF_id_corr_dn[i]   = sf[i] - out.eleEffSF[i];
                else if(ourSys == NtSys::EL_EFF_Reco_TotalCorrUncertainty_UP) out.errEffSF_reco_corr_up[i] = sf[i] - out.eleEffSF[i];
                else if(ourSys == NtSys::EL_EFF_Reco_TotalCorrUncertainty_DN) out.errEffSF_reco_corr_dn[i] = sf[i] - out.eleEffSF[i];
            }
        } // sysInfo
        for(int i=SusyObjId::eleTightLH; i<=SusyObjId::eleLooseLH; i++){
            if(m_susyObj[i]->resetSystematics() != CP::SystematicCode::Ok){
                cout << "SusyNtMaker::storeElectron    cannot reset SUSYTools systematics. Aborting." << endl;
                abort();
            }
        }
    } // if isMC
    else {
        for(int i=ElectronId::TightLH; i<ElectronIdInvalid; i++){
            out.errEffSF_id_corr_up[i] = out.errEffSF_id_corr_dn[i] = 0;
            out.errEffSF_reco_corr_up[i] = out.errEffSF_reco_corr_dn[i] = 0;
        }
    }

    //////////////////////////////////////
    // Electron cluster information
    //////////////////////////////////////
    if(const xAOD::CaloCluster* c = in.caloCluster()) {
        out.clusE   = c->e()*MeV2GeV;
        out.clusEta = c->eta();
        out.clusPhi = c->phi();
    } else {
        all_available = false;
    }
    //////////////////////////////////////
    // Electron track information
    //////////////////////////////////////
    if(const xAOD::TrackParticle* t = in.trackParticle()){
        out.trackPt = t->pt()*MeV2GeV;
        out.trackEta = t->eta();
        out.d0      = t->d0();
        out.d0sigBSCorr = xAOD::TrackingHelpers::d0significance( t, eventinfo->beamPosSigmaX(),
                                        eventinfo->beamPosSigmaY(), eventinfo->beamPosSigmaXY() );

        const xAOD::Vertex* PV = getPV();
        double  primvertex_z = (PV) ? PV->z() : -999;
        out.z0 = t->z0() + t->vz() - primvertex_z;

        out.errD0         = Amg::error(t->definingParametersCovMatrix(),0);
        out.errZ0         = Amg::error(t->definingParametersCovMatrix(),1);
    } else {
        all_available = false;
    }

    //////////////////////////////////////
    // Trigger matching
    //////////////////////////////////////
    // I swear it's been 6 months and the ability to grab the trigger features still doesn't work
//    out.trigBits = matchElectronTriggers(in);
//    cout << "testing electron trigBits" << endl;
//    int nbins = h_passTrigLevel->GetXaxis()->GetNbins();
//    for(int iTrig=1; iTrig<26; iTrig++){
//        bool bit = out.trigBits.TestBitNumber(iTrig);
//        string trigger = h_passTrigLevel->GetXaxis()->GetBinLabel(iTrig);
//        cout << "\t passed trigger " << trigger << "? " << (bit ? "yes" : "no") << endl;
//    }
//    cout << endl;

 
    if(m_dbg && !all_available) cout<<"missing some electron variables"<<endl;
    m_susyNt.ele()->push_back(out);
}
//----------------------------------------------------------
// // match muon to muon truth, returns element object if success, else NULL
// D3PDReader::TruthMuonD3PDObjectElement* getMuonTruth(D3PDReader::MuonD3PDObject* muons, int muIdx, D3PDReader::TruthMuonD3PDObject* truthMuons)
// {
//     D3PDReader::TruthMuonD3PDObjectElement* result = NULL;
//     int bc = muons->truth_barcode()->at(muIdx);
//     if(bc==0){ // if barcode is zero then matching has already failed
//         return result;
//     }
//     // loop over truth muons, comparing barcode
//     for(int matchIdx=0; matchIdx < truthMuons->n(); matchIdx++){
//         if(bc == truthMuons->barcode()->at(matchIdx)){
//             result = & (*truthMuons)[matchIdx];
//             break;
//         }
//     }
//     return result;
// }
//----------------------------------------------------------
void SusyNtMaker::storeMuon(const xAOD::Muon &in)
{
    if(m_dbg>=15) cout<<"SusyNtMaker::storeMuon pT "<< in.pt()*MeV2GeV <<endl;
    const xAOD::EventInfo* eventinfo = XaodAnalysis::xaodEventInfo();

    Susy::Muon out;
    //////////////////////////////////////
    // 4-vector
    //////////////////////////////////////
    double pt(in.pt()*MeV2GeV), eta(in.eta()), phi(in.phi()), m(in.m()*MeV2GeV);
    out.SetPtEtaPhiM(pt, eta, phi, m);
    out.pt  = pt;
    out.eta = eta;
    out.phi = phi;
    out.m   = m;
    out.q   = in.charge();
    //////////////////////////////////////
    // SUSYTools flags (flags from default
    // SUSYTools object)
    //////////////////////////////////////
    out.isBaseline = (bool)in.auxdata< char >("baseline");
    out.isSignal   = (bool)in.auxdata< char >("signal");
    out.isCombined = in.muonType()==xAOD::Muon::Combined;
    out.isCosmic   = (bool)in.auxdata< char >("cosmic");  // note: this depends on definition of baseline and OR!
    out.isBadMuon  = (bool)in.auxdata<char>("bad");       // note: independent of definition of baseline/OR

    //////////////////////////////////////
    // MuonId flags (MuonSelectionTool)
    //////////////////////////////////////
    out.veryLoose   = muIsOfType(in, MuonId::VeryLoose);
    out.loose       = muIsOfType(in, MuonId::Loose);
    out.medium      = muIsOfType(in, MuonId::Medium);
    out.tight       = muIsOfType(in, MuonId::Tight);

    //////////////////////////////////////
    // Isolation flags (IsolationSelectionTool)
    //////////////////////////////////////
    out.isoGradientLoose = in.auxdata<char>("isol");
    //out.isoGradientLoose = m_isoToolGradientLooseCone40Calo->accept(in) ? true : false;
    out.isoGradient = m_isoToolGradientCone40->accept(in) ? true : false;
    out.isoLooseTrackOnly = m_isoToolLooseTrackOnlyCone20->accept(in) ? true : false;
    out.isoLoose = m_isoToolLoose->accept(in) ? true : false;
    out.isoTight = m_isoToolTight->accept(in) ? true : false;

    bool all_available=true;

    //////////////////////////////////////
    // Isolation variables
    //////////////////////////////////////
    //For Rel 20
    /*
    out.ptcone20 = in.isolation(xAOD::Iso::ptcone20) * MeV2GeV;
    out.ptcone30 = in.isolation(xAOD::Iso::ptcone30) * MeV2GeV;
    out.ptvarcone20 = in.auxdataConst<float>("ptvarcone20") * MeV2GeV;
    out.ptvarcone30 = in.auxdataConst<float>("ptvarcone30") * MeV2GeV;
    */
    //Update for Rel 20
    //out.ptvarcone20 = in.isolationValue(xAOD::Iso::ptvarcone20) * MeV2GeV;
    //out.ptvarcone30 = in.isolationValue(xAOD::Iso::ptvarcone30) * MeV2GeV;

    //all_available &= in.isolation(out.etconetopo20, xAOD::Iso::topoetcone20); out.etconetopo20 *= MeV2GeV;
    //all_available &= in.isolation(out.etconetopo30, xAOD::Iso::topoetcone30); out.etconetopo30 *= MeV2GeV;
    all_available &= in.isolation(out.ptcone20, xAOD::Iso::ptcone20); out.ptcone20 *= MeV2GeV;
    all_available &= in.isolation(out.ptcone30, xAOD::Iso::ptcone30); out.ptcone30 *= MeV2GeV;
    out.ptvarcone20 = in.auxdataConst<float>("ptvarcone20") * MeV2GeV;
    out.ptvarcone30 = in.auxdataConst<float>("ptvarcone30") * MeV2GeV;
    out.etconetopo20 = in.isolation(xAOD::Iso::topoetcone20) * MeV2GeV;
    out.etconetopo30 = in.isolation(xAOD::Iso::topoetcone30) * MeV2GeV;
    

    //////////////////////////////////////
    // Muon track particle
    //////////////////////////////////////
    if(const xAOD::TrackParticle* t = in.primaryTrackParticle()){
        const xAOD::Vertex* PV = getPV();
        double  primvertex_z = (PV) ? PV->z() : 0.;
        out.d0             = t->d0();
        out.errD0          = Amg::error(t->definingParametersCovMatrix(),0); 
        out.d0sigBSCorr = xAOD::TrackingHelpers::d0significance( t, eventinfo->beamPosSigmaX(),
                                    eventinfo->beamPosSigmaY(), eventinfo->beamPosSigmaXY() );
        out.z0             = t->z0() + t->vz() - primvertex_z;
        out.errZ0          = Amg::error(t->definingParametersCovMatrix(),1); 
    }
    //////////////////////////////////////
    // Muon ID track particle (if)
    //////////////////////////////////////
    if(const xAOD::TrackParticle* idtrack = in.trackParticle( xAOD::Muon::InnerDetectorTrackParticle )){
        out.idTrackPt      = idtrack->pt()*MeV2GeV;  
        out.idTrackEta     = idtrack->eta();  
        out.idTrackPhi     = idtrack->phi(); 
        out.idTrackQ       = idtrack->qOverP() < 0 ? -1 : 1;
        out.idTrackQoverP  = idtrack->qOverP()*MeV2GeV;
        out.idTrackTheta   = idtrack->theta();
    }
    //////////////////////////////////////
    // Muon MS track particle (if)
    //////////////////////////////////////
    if(const xAOD::TrackParticle* mstrack = in.trackParticle( xAOD::Muon::MuonSpectrometerTrackParticle )){
        out.msTrackPt      = mstrack->pt()*MeV2GeV;
        out.msTrackEta     = mstrack->eta();
        out.msTrackPhi     = mstrack->phi();
        out.msTrackQ       = mstrack->qOverP() < 0 ? -1 : 1;
        out.msTrackQoverP  = mstrack->qOverP()*MeV2GeV;
        out.msTrackTheta   = mstrack->theta();
    }
    //////////////////////////////////////
    // Muon truth matching/info
    //////////////////////////////////////
    if(m_isMC) {
        const xAOD::TrackParticle* trackParticle = in.primaryTrackParticle();
        if(trackParticle){
            // mcType <==> "truthType" of input xAOD (MCTruthClassifier)
            out.mcType = xAOD::TruthHelpers::getParticleTruthType(*trackParticle);
            // mcOrigin <==> "truthOrigin" of input xAOD (MCTruthClassifier)
            out.mcOrigin = xAOD::TruthHelpers::getParticleTruthOrigin(*trackParticle);
            const xAOD::TruthParticle* truthMu = xAOD::TruthHelpers::getTruthParticle(*trackParticle);
            out.matched2TruthLepton = truthMu ? true : false;
            int matchedPdgId = truthMu ? truthMu->pdgId() : -999;
            out.truthType = isFakeLepton(out.mcOrigin, out.mcType, matchedPdgId);
        }
    }

    //////////////////////////////////////
    // Trigger matching
    //////////////////////////////////////
    out.trigBits   = matchMuonTriggers(in);
//    cout << "testing electron trigBits" << endl;
//    int nbins = h_passTrigLevel->GetXaxis()->GetNbins();
//    for(int iTrig=1; iTrig<26; iTrig++){
//        bool bit = out.trigBits.TestBitNumber(iTrig);
//        string trigger = h_passTrigLevel->GetXaxis()->GetBinLabel(iTrig);
//        cout << "\t passed trigger " << trigger << "? " << (bit ? "yes" : "no") << endl;
//    }
//    cout << endl;

    //////////////////////////////////////
    // Lepton SF
    // - one for each MuonId that we use:
    // - Loose and Medium
    //////////////////////////////////////
    if(m_isMC  && (out.loose || out.medium) && (fabs(out.eta)<2.5)){
        // reco/iso SF (currently isoSF only defined for LooseTrackOnly Isolation WP)
        // GetSignalMuon(const xAOD::Muon, recoSF = true, isoSF = false);
        out.muoEffSF[MuonId::Loose] = m_susyObj[SusyObjId::muoLoose]->GetSignalMuonSF(in);
        out.muoEffSF[MuonId::Medium] = m_susyObj[SusyObjId::muoMedium]->GetSignalMuonSF(in);

        // Trigger SF (as the tool only accepts muon containers we need to build a proxy container each time-- OY VEY!)
        xAOD::MuonContainer *sf_muon = new xAOD::MuonContainer;
        xAOD::MuonAuxContainer *sf_muon_aux = new xAOD::MuonAuxContainer;
        sf_muon->setStore(sf_muon_aux);
        xAOD::Muon* sfMu = new xAOD::Muon;
        sfMu->makePrivateStore(in);
        sf_muon->push_back(sfMu);
        // providing the MuonTriggerSF tool with 'real' runNumber produces errors and it automatically
        // sets it to the run 267639 (this should be changed in more recent tags when they provide
        // period-dependent SFs so check this!)
        m_susyObj[SusyObjId::muoLoose]->setRunNumber(267639); // don't forget this thing SUSYTools doesn't do automatically
        m_susyObj[SusyObjId::muoMedium]->setRunNumber(267639); // don't forget this thing SUSYTools doesn't do automatically

        // loose
        for(uint i = 0; i < xaodTriggers_muonSF().size(); i++){
            out.muoTrigSF_loose[i] = m_susyObj[SusyObjId::muoLoose]->GetTotalMuonSF(*sf_muon, false, false, xaodTriggers_muonSF()[i]);
        } // i

        // medium
        for(uint i = 0; i < xaodTriggers_muonSF().size(); i++){
            out.muoTrigSF_medium[i] = m_susyObj[SusyObjId::muoMedium]->GetTotalMuonSF(*sf_muon, false, false, xaodTriggers_muonSF()[i]);
        } // i

        delete sf_muon;
        delete sf_muon_aux;
    } // if MC && quality
    //////////////////////////////////////
    // Systematic variation on muon SF
    //////////////////////////////////////
    if(m_isMC && m_sys && (out.veryLoose || out.loose || out.medium) && (fabs(out.eta)<2.5)) {
        for(const auto& sysInfo : systInfoList) {
            if(!(sysInfo.affectsType == ST::SystObjType::Muon && sysInfo.affectsWeights)) continue;
            const CP::SystematicSet& sys = sysInfo.systset;
            SusyNtSys ourSys = CPsys2sys((sys.name()).c_str());
            for(int i=SusyObjId::muoLoose; i<=SusyObjId::muoMedium; i++){
                if(m_susyObj[i]->applySystematicVariation(sys) != CP::SystematicCode::Ok) {
                    cout << "SusyNtMaker::storeMuon    cannot configure SUSYTools for systematic " << sys.name() << endl;
                    continue;
                }
            }
            vector<float> sf;
            sf.assign(MuonId::MuonIdInvalid, 1);
            sf[MuonId::Medium] = m_susyObj[SusyObjId::muoMedium]->GetSignalMuonSF(in);
            sf[MuonId::Loose] = m_susyObj[SusyObjId::muoLoose]->GetSignalMuonSF(in);
            for(int i=MuonId::VeryLoose; i<MuonId::MuonIdInvalid; i++){
                if     (ourSys == NtSys::MUONSFSTAT_UP)  out.errEffSF_stat_up[i] = sf[i] - out.muoEffSF[i];
                else if(ourSys == NtSys::MUONSFSTAT_DN) out.errEffSF_stat_dn[i] = sf[i] - out.muoEffSF[i];
                else if(ourSys == NtSys::MUONSFSYS_UP) out.errEffSF_syst_up[i] = sf[i] - out.muoEffSF[i];
                else if(ourSys == NtSys::MUONSFSYS_DN) out.errEffSF_syst_dn[i] = sf[i] - out.muoEffSF[i];
            }
        } // sysInfo
        for(int i=SusyObjId::muoLoose; i<=SusyObjId::muoMedium; i++){
            if(m_susyObj[i]->resetSystematics() != CP::SystematicCode::Ok){
                cout << "SusyNtMaker::storeMuon    cannot reset SUSYTools systematics. Aborting." << endl;
                abort();
            }
        }
    } // ifMC && sys 
    else {
        for(int i=MuonId::VeryLoose; i<MuonId::MuonIdInvalid; i++){
            out.errEffSF_stat_up[i] = out.errEffSF_stat_dn[i] = 0;
            out.errEffSF_syst_up[i] = out.errEffSF_syst_dn[i] = 0;
        }
    }

    // ASM-2014-11-02 :: Store to be true at the moment
    all_available =  false;
    if(m_dbg && !all_available) cout<<"missing some muon variables"<<endl;
    m_susyNt.muo()->push_back(out);
}
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::storeJet(const xAOD::Jet &in)
{
    if(m_dbg>=15) cout<<"SusyNtMaker::storeJet pT "<< in.pt()*MeV2GeV <<endl;

    Susy::Jet out;
    double pt(in.pt()*MeV2GeV), eta(in.eta()), phi(in.phi()), m(in.m()*MeV2GeV);
    out.SetPtEtaPhiM(pt, eta, phi, m);
    out.pt  = pt;
    out.eta = eta;
    out.phi = phi;
    out.m   = m;
    bool all_available=true;

    // number of associated tracks
    vector<int> nTrkVec;
    in.getAttribute(xAOD::JetAttribute::NumTrkPt500, nTrkVec);
    int jet_nTrk = (m_susyObj[m_eleIDDefault]->GetPrimVtx()==0 || nTrkVec.size()==0) ? 0 : nTrkVec[m_susyObj[m_eleIDDefault]->GetPrimVtx()->index()];
    out.nTracks = jet_nTrk;

    // JVF 
    // ASM-2014-11-04 :: Remember JVT is gonna replace JVF in Run-II but not yet available
    vector<float> jetJVF;
    in.getAttribute(xAOD::JetAttribute::JVF,jetJVF); // JVF returns a vector that holds jvf per vertex
    const xAOD::Vertex* PV = getPV();                // Need to know the PV
    out.jvf = (PV) ? jetJVF.at(PV->index()) : 0.;    // Upon discussion w/ TJ (2014-12-11)

    // JVT
    static SG::AuxElement::Accessor<float> acc_jvt("Jvt");
    out.jvt = acc_jvt(in);

    // Truth Label/Matching 
    if (m_isMC) { in.getAttribute("ConeTruthLabelID", out.truthLabel); }
//rel 20
    //int JetPartonID = (in.jet())->auxdata< int >("PartonTruthLabelID"); // ghost association
    //int JetConeID   = (in.jet())->auxdata< int >("ConeTruthLabelID"); // cone association

    // jetOut->matchTruth    = m_isMC? matchTruthJet(jetIdx) : false;

    // B-tagging 
    // dantrim July 3 2015 : mv1 is a goner
    double weight_mv2c20(0.);
    if(!in.btagging()->MVx_discriminant("MV2c20", weight_mv2c20)){ cout << "SusyNtMaker::storeJet ERROR    Failed to retrieve MV2c20 weight!" << endl; }
    out.mv2c20 = weight_mv2c20;

    out.sv1plusip3d   = (in.btagging())->SV1plusIP3D_discriminant();           
    out.bjet = m_susyObj[m_eleIDDefault]->IsBJet(in) ? 1 : 0;
    out.effscalefact = m_isMC ? in.auxdata< double >("effscalefact") : 1; // dantrim Jul 19 2015 -- error says new type is float, but by float they mean double

  //  vector<float> test_values;
  //  test_values.push_back(out.effscalefact);

    // B-tagging systematics
    //AT 06-29-15: Coomented out btag decoration in SusyTools 
/*
    if(m_isMC && m_sys) {
      for(const auto& sysInfo : systInfoList){
        if(!(sysInfo.affectsType == ST::SystObjType::BTag && sysInfo.affectsWeights)) continue;
        // Read information
        const CP::SystematicSet& sys = sysInfo.systset;
        SusyNtSys ourSys = CPsys2sys((sys.name()).c_str());
        if(m_dbg>=15) std::cout<< "\t Jet btag sys " << NtSys::SusyNtSysNames[ourSys] << endl;
        // Configure the tools
        if ( m_susyObj[m_eleIDDefault]->applySystematicVariation(sys) != CP::SystematicCode::Ok){
          cout << "SusyNtMaker::storeJet - cannot configure SUSYTools for " << sys.name() << endl;
          continue;
        }
        // Get and store the information
        m_susyObj[m_eleIDDefault]->BtagSF(XaodAnalysis::xaodJets(systInfoList[0])); // re-decorate the jets w/ appropriate effscalefact
                                                                                    // not the most efficient way but works 
        float scaleFactor = in.auxdata< float >("effscalefact");
        out.setFTSys(ourSys,scaleFactor);
        test_values.push_back(scaleFactor);
      }
      m_susyObj[m_eleIDDefault]->resetSystematics();
    }
*/

/*
    cout << "SERHAN :: " << endl;
    for(auto scale : test_values) {
        cout << scale << endl;
    }
*/

    // Misc
    out.detEta = (in.jetP4(xAOD::JetConstitScaleMomentum)).eta();
    in.getAttribute(xAOD::JetAttribute::EMFrac,out.emfrac);
    in.getAttribute(xAOD::JetAttribute::BchCorrJet,out.bch_corr_jet);
    in.getAttribute(xAOD::JetAttribute::BchCorrCell,out.bch_corr_cell);

    // isBadJet 
    //out.isBadVeryLoose = false; // DG tmp-2014-11-02 in.isAvailable("bad") ? in.auxdata<char>("bad") : false;
    out.isBadVeryLoose = (bool)in.auxdata<char>("bad") ? 1 : 0;

    // Hot Tile
    float fracSamplingMax, samplingMax;
    in.getAttribute(xAOD::JetAttribute::SamplingMax, samplingMax);
    in.getAttribute(xAOD::JetAttribute::FracSamplingMax, fracSamplingMax);
    const xAOD::EventInfo* eventinfo = XaodAnalysis::xaodEventInfo();
    //AT Removed in SUSYTools-00-005-00-29
    /*
    out.isHotTile = m_susyObj[m_eleIDDefault]->isHotTile(eventinfo->runNumber(),
                                                         fracSamplingMax,
                                                         samplingMax,
                                                         eta, phi); 
    */
    // // BCH cleaning flags - ASM-2014-11-04 :: Obsolete???
    // uint bchRun = m_isMC? m_mcRun : m_event.eventinfo.RunNumber();
    // uint bchLB = m_isMC? m_mcLB : m_event.eventinfo.lbn();
    // #define BCH_ARGS bchRun, bchLB, jetOut->detEta, jetOut->phi, jetOut->bch_corr_cell, jetOut->emfrac, jetOut->pt*1000.
    // jetOut->isBadMediumBCH = !m_susyObj[m_eleIDDefault]->passBCHCleaningMedium(BCH_ARGS, 0);
    // jetOut->isBadMediumBCH_up = !m_susyObj[m_eleIDDefault]->passBCHCleaningMedium(BCH_ARGS, 1);
    // jetOut->isBadMediumBCH_dn = !m_susyObj[m_eleIDDefault]->passBCHCleaningMedium(BCH_ARGS, -1);
    // jetOut->isBadTightBCH = !m_susyObj[m_eleIDDefault]->passBCHCleaningTight(BCH_ARGS);
    // #undef BCH_ARGS

    // // Save the met weights for the jets
    // // by checking status word similar to
    // // what is done in met utility
    // const D3PDReader::MissingETCompositionD3PDObjectElement &jetMetEgamma10NoTau = m_event.jet_AntiKt4LCTopo_MET_Egamma10NoTau[jetIdx];
    // // 0th element is what we care about
    // int sWord = jetMetEgamma10NoTau.statusWord().at(0);
    // bool passSWord = (MissingETTags::DEFAULT == sWord);       // Note assuming default met..

    if(m_dbg && !all_available) cout<<"missing some jet variables"<<endl;
    m_susyNt.jet()->push_back(out);
}
//----------------------------------------------------------
void SusyNtMaker::storePhoton(const xAOD::Photon &in)
{
    if(m_dbg>=15) cout<<"SusyNtMaker::storePhoton pT "<< in.pt()*MeV2GeV <<endl;

    Susy::Photon out;
    double pt(in.pt()*MeV2GeV), eta(in.eta()), phi(in.phi()), m(in.m()*MeV2GeV);
    out.SetPtEtaPhiM(pt, eta, phi, m);
    out.pt  = pt;
    out.eta = eta;
    out.phi = phi;
    out.m   = m;
    out.isConv = xAOD::EgammaHelpers::isConvertedPhoton(&in);
    bool all_available=true;

    all_available &= in.passSelection(out.tight,"Tight");

    if(const xAOD::CaloCluster* c = in.caloCluster()) {
        out.clusE   = c->e()*MeV2GeV;
        out.clusEta = c->eta();
        out.clusPhi = c->phi();
    } else {
        all_available = false;
    }
    out.OQ = in.isGoodOQ(xAOD::EgammaParameters::BADCLUSPHOTON);
//    in.isolationValue(out.topoEtcone40,xAOD::Iso::topoetcone40);
    out.topoEtcone40 = in.isolationValue(xAOD::Iso::topoetcone40) * MeV2GeV;

    // isolation
    out.isoCone40CaloOnly   = m_isoToolGradientLooseCone40Calo->accept(in) ? true : false;
    out.isoCone40           = m_isoToolGradientCone40->accept(in) ? true : false;
    out.isoCone20           = m_isoToolLooseTrackOnlyCone20->accept(in) ? true : false;
   
    if(m_dbg) cout << "AT: storePhoton: " << out.pt << " " << out.tight << " " << out.isConv << endl;
    if(m_dbg && !all_available) cout<<"missing some photon variables"<<endl;
    m_susyNt.pho()->push_back(out);
}
//----------------------------------------------------------
void SusyNtMaker::storeTau(const xAOD::TauJet &in)
{
    if(m_dbg>=15) cout<<"SusyNtMaker::storeTau pT "<< in.pt()*MeV2GeV <<endl;

    Susy::Tau out;
    double pt(in.pt()*MeV2GeV), eta(in.eta()), phi(in.phi()), m(in.m()*MeV2GeV);

    out.SetPtEtaPhiM(pt, eta, phi, m);
    out.pt  = pt;
    out.eta = eta;
    out.phi = phi;
    out.m   = m;
    out.q = int(in.charge());
    out.author = 0;//remove ?
/*
    if(in.isTau(xAOD::TauJetParameters::tauRec))  out.author |= 1<<0;
    if(in.isTau(xAOD::TauJetParameters::tau1P3P)) out.author |= 1<<1;
    if(in.isTau(xAOD::TauJetParameters::PanTau))  out.author |= 1<<2;
*/
   
    out.nTrack = in.nTracks();
    out.eleBDT = in.discriminant(xAOD::TauJetParameters::BDTEleScore);
    out.jetBDT = in.discriminant(xAOD::TauJetParameters::BDTJetScore);

    out.jetBDTSigLoose = in.isTau(xAOD::TauJetParameters::JetBDTSigLoose);
    out.jetBDTSigMedium = in.isTau(xAOD::TauJetParameters::JetBDTSigMedium);
    out.jetBDTSigTight = in.isTau(xAOD::TauJetParameters::JetBDTSigTight);

    out.eleBDTLoose = in.isTau(xAOD::TauJetParameters::EleBDTLoose);
    out.eleBDTMedium = in.isTau(xAOD::TauJetParameters::EleBDTMedium);
    out.eleBDTTight = in.isTau(xAOD::TauJetParameters::EleBDTTight);

    out.muonVeto = in.isTau(xAOD::TauJetParameters::MuonVeto);
    
    if (m_isMC){
       // // dantrim Jul 3 2015 : SUSYTools' examples don't even work...
       // const xAOD::TruthParticle* truthTau = m_tauTruthMatchingTool->applyTruthMatch(in);
       // if((bool)in.auxdata< char >("IsTruthMatched")) {
       //     cout << "tau IsTruthMatched == True    nProngs: " << int(in.auxdata<size_t>("TruthProng")) << "   charge: " << int(in.auxdata<int>("TruthCharge")) << endl;
       //     out.trueTau = true;
       // }

        //m_tauTruthMatchingTool->applyTruthMatch(tau); // memory leak check
        //if (in.auxdata<bool>("IsTruthMatched")) out.trueTau = true; // memory leak check
        //else out.trueTau = false;  // memory leak check
        //in.auxdata<size_t>("TruthProng");
        //in.auxdata<int>("TruthCharge");
        //in.auxdata<bool>("IsHadronicTau");

        m_TauEffEleTool->applyEfficiencyScaleFactor(in);

        //AT: Add errors!    
        //m_TauEffEleTool->getEfficiencyScaleFactor(tau, out.looseEffS);
        //How is the ID dealt with
        //This is not correct... we need the tool initialize with various selection!
        out.looseEffSF = in.auxdata<double>("TauScaleFactorContJetID");//ok
        out.mediumEffSF = in.auxdata<double>("TauScaleFactorContJetID");//ok
        out.tightEffSF = in.auxdata<double>("TauScaleFactorContJetID");//ok
        // stat errors not supported
        // systematics not included here
        double EVetoSF = 0.0;
        if (in.nTracks() == 1)
        {
            m_TauEffEleTool->getEfficiencyScaleFactor(in, EVetoSF); //?????
        }
       //AT:: Add errors - what is store does not make sense. Save same thing.
        out.looseEVetoSF = EVetoSF;
        out.mediumEVetoSF = EVetoSF;
        out.tightEVetoSF = EVetoSF;


 // may8 - comment out truthType accessor     out.truthType = classifyTau(tau);
        if(m_dbg>10) std::cout << "TauClassifier: found Tau= "<< out.truthType << std::endl;
    }


    //TO ADD
    // tauOut->matched2TruthLepton   = m_isMC? m_recoTruthMatch.Matched2TruthLepton(*tauLV, true) : false;
    // tauOut->detailedTruthType     = m_isMC? m_recoTruthMatch.TauDetailedFakeType(*tauLV) : -1;
    // tauOut->truthType             = m_isMC? m_recoTruthMatch.TauFakeType(tauOut->detailedTruthType) : -1;
    
   
   if(m_dbg>5) cout << "SusyNtMaker Filling Tau pt " << out.pt 
                     << " eta " << out.eta << " phi " << out.phi << " q " << out.q << " " << int(in.charge()) << endl;



// // ID efficiency scale factors
    // if(m_isMC){
    //   #define TAU_ARGS TauCorrUncert::BDTLOOSE, tauLV->Eta(), element->numTrack()
    //   //TauCorrections* tauSF       = m_susyObj[m_eleIDDefault]->GetTauCorrectionsProvider();
    //   TauCorrUncert::TauSF* tauSF = m_susyObj[m_eleIDDefault]->GetSFTool();
    //   //tauOut->looseEffSF        = tauSF->GetIDSF(TauCorrUncert::BDTLOOSE, tauLV->Eta(), element->numTrack());
    //   //tauOut->mediumEffSF       = tauSF->GetIDSF(TauCorrUncert::BDTMEDIUM, tauLV->Eta(), element->numTrack());
    //   //tauOut->tightEffSF        = tauSF->GetIDSF(TauCorrUncert::BDTTIGHT, tauLV->Eta(), element->numTrack());
    //   //tauOut->errLooseEffSF     = tauSF->GetIDSFUnc(TauCorrUncert::BDTLOOSE, tauLV->Eta(), element->numTrack());
    //   //tauOut->errMediumEffSF    = tauSF->GetIDSFUnc(TauCorrUncert::BDTMEDIUM, tauLV->Eta(), element->numTrack());
    //   //tauOut->errTightEffSF     = tauSF->GetIDSFUnc(TauCorrUncert::BDTTIGHT, tauLV->Eta(), element->numTrack());
    //   tauOut->looseEffSF          = tauSF->GetIDSF(TAU_ARGS);
    //   tauOut->mediumEffSF         = tauSF->GetIDSF(TAU_ARGS);
    //   tauOut->tightEffSF          = tauSF->GetIDSF(TAU_ARGS);
    //   tauOut->errLooseEffSF       = sqrt(pow(tauSF->GetIDStatUnc(TAU_ARGS), 2) + pow(tauSF->GetIDSysUnc(TAU_ARGS), 2));
    //   tauOut->errMediumEffSF      = sqrt(pow(tauSF->GetIDStatUnc(TAU_ARGS), 2) + pow(tauSF->GetIDSysUnc(TAU_ARGS), 2));
    //   tauOut->errTightEffSF       = sqrt(pow(tauSF->GetIDStatUnc(TAU_ARGS), 2) + pow(tauSF->GetIDSysUnc(TAU_ARGS), 2));
    //   #undef TAU_ARGS
    
    //   if(element->numTrack()==1){
    //     float eta = element->leadTrack_eta();
    //     tauOut->looseEVetoSF      = tauSF->GetEVetoSF(eta, TauCorrUncert::BDTLOOSE, TauCorrUncert::LOOSE, TauCorrUncert::MEDIUMPP);
    //     tauOut->mediumEVetoSF     = tauSF->GetEVetoSF(eta, TauCorrUncert::BDTMEDIUM, TauCorrUncert::MEDIUM, TauCorrUncert::MEDIUMPP);
    //     // Doesn't currently work. Not sure why. Maybe they don't provide SFs for this combo
    //     //tauOut->tightEVetoSF      = tauSF->GetEVetoSF(eta, TauCorrUncert::BDTTIGHT, TauCorrUncert::TIGHT, TauCorrUncert::MEDIUMPP);
    //     tauOut->errLooseEVetoSF   = tauSF->GetEVetoSFUnc(eta, TauCorrUncert::BDTLOOSE, TauCorrUncert::LOOSE, TauCorrUncert::MEDIUMPP, 1);
    //     tauOut->errMediumEVetoSF  = tauSF->GetEVetoSFUnc(eta, TauCorrUncert::BDTMEDIUM, TauCorrUncert::MEDIUM, TauCorrUncert::MEDIUMPP, 1);
    //     //tauOut->errTightEVetoSF   = tauSF->GetEVetoSFUnc(eta, TauCorrUncert::BDTTIGHT, TauCorrUncert::TIGHT, TauCorrUncert::MEDIUMPP, 1);
    //   }
    // }
    
    // Tau trigger bits needed   
 
   m_susyNt.tau()->push_back(out);
}

/*--------------------------------------------------------------------------------*/
// Fill MET variables
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::fillMetVars(SusyNtSys sys)
{
    if(m_dbg>=15) cout<<"SusyNtMaker::fillMetVars "<<endl;

    xAOD::MissingETContainer::const_iterator met_it = m_metContainer->find("Final");
    if(m_dbg>=15){
        cout << "Dump MET container - SusyNtMaker " << endl;
        for(auto it=m_metContainer->begin(), end=m_metContainer->end(); it!=end; ++it){
            cout << "Met container name " << (*it)->name() << endl;
        }
    }

    if (met_it == m_metContainer->end()) {
        cout<<"WARNING: SusyNtMaker: No RefFinal inside MET container found"<<endl;
        return;
    }
  
    m_susyNt.met()->push_back( Susy::Met() );
    Susy::Met* metOut = & m_susyNt.met()->back();
    metOut->Et = (*met_it)->met()*MeV2GeV;
    metOut->phi = (*met_it)->phi();
    metOut->sumet = (*met_it)->sumet()*MeV2GeV;
    metOut->sys = sys;
    if(m_dbg>=5) cout << " AT:fillMetVars " << metOut->Et << " " 
                      << metOut->phi << " " << metOut->lv().Pt() 
                      << " " << NtSys::SusyNtSysNames[sys] << endl;
    
    // RefEle
    xAOD::MissingETContainer::const_iterator met_find = m_metContainer->find("RefEle");
    if (met_find != m_metContainer->end()) {
        metOut->refEle_et = (*met_find)->met()*MeV2GeV;
        metOut->refEle_phi = (*met_find)->phi();
        metOut->refEle_sumet = (*met_find)->sumet()*MeV2GeV;
    }
    // RefGamma
    met_find = m_metContainer->find("RefGamma");
    if (met_find != m_metContainer->end()) {
        metOut->refGamma_et = (*met_find)->met()*MeV2GeV;
        metOut->refGamma_phi = (*met_find)->phi();
        metOut->refGamma_sumet = (*met_find)->sumet()*MeV2GeV;
    }
    // RefTau
    met_find = m_metContainer->find("RefTau");
    if (met_find != m_metContainer->end()) {
        metOut->refTau_et = (*met_find)->met()*MeV2GeV;
        metOut->refTau_phi = (*met_find)->phi();
        metOut->refTau_sumet = (*met_find)->sumet()*MeV2GeV;
    }
    // RefJet
    met_find = m_metContainer->find("RefJet");
    if (met_find != m_metContainer->end()) {
        metOut->refJet_et = (*met_find)->met()*MeV2GeV;
        metOut->refJet_phi = (*met_find)->phi();
        metOut->refJet_sumet = (*met_find)->sumet()*MeV2GeV;
    }
    // SoftTerm
    met_find = m_metContainer->find("PVSoftTrk"); // We use default GetMET method, which as TST met so soft term is PVSoftTrk (not SoftClus)
    if (met_find != m_metContainer->end()) {
        metOut->softTerm_et = (*met_find)->met()*MeV2GeV;
        metOut->softTerm_phi = (*met_find)->phi();
        metOut->softTerm_sumet = (*met_find)->sumet()*MeV2GeV;
    }
    // RefMuons
    met_find = m_metContainer->find("Muons");
    if (met_find != m_metContainer->end()) {
        metOut->refMuo_et = (*met_find)->met()*MeV2GeV;
        metOut->refMuo_phi = (*met_find)->phi();
        metOut->refMuo_sumet = (*met_find)->sumet()*MeV2GeV;
    }

    // // I guess these are the only ones we need to specify, the ones specified in SUSYTools...
    // // All the rest should be automatic (I think), e.g. JES
    // if(sys == NtSys_SCALEST_UP) metSys = METUtil::ScaleSoftTermsUp;
    // else if(sys == NtSys_SCALEST_DN) metSys = METUtil::ScaleSoftTermsDown;
    // else if(sys == NtSys_RESOST) metSys = METUtil::ResoSoftTermsUp;

}
//----------------------------------------------------------
void SusyNtMaker::fillTrackMetVars(SusyNtSys sys)
{
    if(m_dbg>=15) cout<<"SusyNtMaker::fillTrackMetVars " <<endl;

    xAOD::MissingETContainer::const_iterator trackMet_it = m_trackMetContainer->find("Track");
    
    if (trackMet_it == m_trackMetContainer->end())
    {
        cout << "No Track inside MET_Track container" << endl;
        return;
    }
    
    m_susyNt.tkm()->push_back(Susy::TrackMet());
    Susy::TrackMet* trackMetOut = &m_susyNt.tkm()->back();
    
    trackMetOut->Et =  (*trackMet_it)->met()*MeV2GeV;// m_met.Et();
    trackMetOut->phi = (*trackMet_it)->phi();// m_met.Phi();
    trackMetOut->sys = sys;
    trackMetOut->sumet = (*trackMet_it)->sumet()*MeV2GeV;

    // let's get the electron term
    xAOD::MissingETContainer::const_iterator met_find = m_trackMetContainer->find("RefEle");
    if(met_find != m_trackMetContainer->end()) {
        trackMetOut->refEle_et = (*met_find)->met()*MeV2GeV;
        trackMetOut->refEle_phi = (*met_find)->phi();
        trackMetOut->refEle_sumet = (*met_find)->sumet()*MeV2GeV;
    }
    // muon term
    met_find = m_trackMetContainer->find("Muons");
    if(met_find != m_trackMetContainer->end()) {
        trackMetOut->refMuo_et = (*met_find)->met()*MeV2GeV;
        trackMetOut->refMuo_phi = (*met_find)->phi();
        trackMetOut->refMuo_sumet = (*met_find)->sumet()*MeV2GeV;
    }
    // jet term
    met_find = m_trackMetContainer->find("RefJet");
    if(met_find != m_trackMetContainer->end()) {
        trackMetOut->refJet_et = (*met_find)->met()*MeV2GeV;
        trackMetOut->refJet_phi = (*met_find)->phi();
        trackMetOut->refJet_sumet = (*met_find)->sumet()*MeV2GeV;
    }
    // soft term
    met_find = m_trackMetContainer->find("PVSoftTrk");
    if(met_find != m_trackMetContainer->end()) {
        trackMetOut->softTerm_et = (*met_find)->met()*MeV2GeV;
        trackMetOut->softTerm_phi = (*met_find)->phi();
        trackMetOut->softTerm_sumet = (*met_find)->sumet()*MeV2GeV;
    } 
    
    if (m_dbg) cout << " AT:fillTrackMetVars " << trackMetOut->Et << " " << trackMetOut->phi << " " << trackMetOut->lv().Pt() << endl;
}
//----------------------------------------------------------
void SusyNtMaker::storeTruthParticle(const xAOD::TruthParticle &in)
{
    if(m_dbg>=15) cout<<"SusyNtMaker::storeTruthParticle "<< in.pt()*MeV2GeV <<endl;

    Susy::TruthParticle out;
    double pt(in.pt()*MeV2GeV), eta(in.eta()), phi(in.phi()), m(in.m()*MeV2GeV);
    out.SetPtEtaPhiM(pt, eta, phi, m);
    out.pt  = pt;
    out.eta = eta;
    out.phi = phi;
    out.m   = m;
    bool all_available=true;
    // out.charge = in.charge(); // DG 2014-08-29 discards const ??
    out.pdgId = in.pdgId();
    out.status = in.status();
    //   tprOut->motherPdgId = smc::determineParentPdg(m_event.mc.pdgId(),
    //                                                 m_event.mc.parent_index(),
    //                                                 truParIdx);
}
/*--------------------------------------------------------------------------------*/
// Fill Truth Jet variables
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::fillTruthJetVars()
{
#warning fillTruthJetVars not implemented
    // if(m_dbg>=5) cout << "fillTruthJetVars" << endl;

    // for(uint iTruJet=0; iTruJet<m_truJets.size(); iTruJet++){
    //   int truJetIdx = m_truJets[iTruJet];

    //   m_susyNt.tjt()->push_back( Susy::TruthJet() );
    //   Susy::TruthJet* truJetOut = & m_susyNt.tjt()->back();
    //   const D3PDReader::JetD3PDObjectElement* element = & m_event.AntiKt4Truth[truJetIdx];

    //   // Set TLV
    //   float pt  = element->pt() / GeV;
    //   float eta = element->eta();
    //   float phi = element->phi();
    //   float m   = element->m()  / GeV;

    //   truJetOut->SetPtEtaPhiM(pt, eta, phi, m);
    //   truJetOut->pt     = pt;
    //   truJetOut->eta    = eta;
    //   truJetOut->phi    = phi;
    //   truJetOut->m      = m;

    //   truJetOut->flavor = element->flavor_truth_label();
    // }
}
/*--------------------------------------------------------------------------------*/
// Fill Truth Met variables
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::fillTruthMetVars()
{
#warning fillTruthMetVars not implemented
    // if(m_dbg>=5) cout << "fillTruthMetVars" << endl;

    // // Just fill the lv for now
    // double Et  = m_truMet.Et()/GeV;
    // double phi = m_truMet.Phi();

    // m_susyNt.tmt()->push_back( Susy::TruthMet() );
    // Susy::TruthMet* truMetOut = & m_susyNt.tmt()->back();
    // truMetOut->Et  = Et;
    // truMetOut->phi = phi;
}



/*--------------------------------------------------------------------------------*/
// Handle Systematic
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::doSystematic()
{
    if(m_dbg>=5) cout<< "doSystematic " << systInfoList.size() << endl;

/*    
//     useful for figuring out what we have and what we expect
      for(const auto& sysInfo : systInfoList) {
        const CP::SystematicSet& sys = sysInfo.systset;
        if(sys.name()=="") continue;
        SusyNtSys ourSys = CPsys2sys((sys.name()).c_str());
        string affects = "";
        if(sysInfo.affectsType == ST::SystObjType::Jet) affects = "JET";
        else if(sysInfo.affectsType == ST::SystObjType::Muon) affects = "MUON";
        else if(sysInfo.affectsType == ST::SystObjType::Egamma) affects = "EGAMMA";
        else if(sysInfo.affectsType == ST::SystObjType::Electron) affects = "ELECTRON";
        else if(sysInfo.affectsType == ST::SystObjType::Photon) affects = "PHOTON";
        else if(sysInfo.affectsType == ST::SystObjType::Tau) affects = "TAU";
        else if(sysInfo.affectsType == ST::SystObjType::BTag) affects = "BTAG";
        else if(sysInfo.affectsType == ST::SystObjType::MET_TST) affects = "MET_TST";
        else if(sysInfo.affectsType == ST::SystObjType::MET_CST) affects = "MET_CST";
        else if(sysInfo.affectsType == ST::SystObjType::MET_Track) affects = "MET_TRACK";
        else if(sysInfo.affectsType == ST::SystObjType::EventWeight) affects = "EVENTWEIGHT";
        else { affects = "UKNOWN"; }
        string kinOrSys = "";
        if(sysInfo.affectsKinematics) kinOrSys = "Kinematics";
        else if(sysInfo.affectsWeights) kinOrSys = "Weights";
        cout << "systematic: " << (sys.name()).c_str() << "                 ours: " << NtSys::SusyNtSysNames[ourSys] << "   affects: " << affects << "  " << kinOrSys << endl;
    }
    cout << endl;
*/
    for(const auto& sysInfo : systInfoList){
        const CP::SystematicSet& sys = sysInfo.systset;
        if(m_dbg>=5) std::cout << ">>>> Working on variation: \"" <<(sys.name()).c_str() << "\" <<<<<<" << std::endl;
        if(sys.name()=="") continue; // skip Nominal
        if(!sysInfo.affectsKinematics) continue;
        if(m_dbg>=5) std::cout << "\t systematic is affecting the kinematics "<< endl;
    
        SusyNtSys ourSys = CPsys2sys((sys.name()).c_str());
        if(ourSys == NtSys::SYS_UNKNOWN ) continue;

        if(m_dbg>=5) cout << "Found syst in global registry: " << sys.name() 
                          << " matching to our systematic " << NtSys::SusyNtSysNames[ourSys] << endl;

        if ( m_susyObj[m_eleIDDefault]->applySystematicVariation(sys) != CP::SystematicCode::Ok){
            cout << "SusyNtMaker::doSystematic - cannot configure SUSYTools for " << sys.name() << endl;
            continue;
        }

      
        /*
          Recheck the event selection and save objects scale variation
        */
        clearOutputObjects(false);
        deleteShallowCopies(false);//Don't clear the nominal containers
        selectObjects(ourSys, sysInfo);
        retrieveXaodMet(sysInfo,ourSys);
        retrieveXaodTrackMet(sysInfo,ourSys);
        assignEventCleaningFlags(); //AT really needed for each systematic ? CHECK
        assignObjectCleaningFlags(sysInfo, ourSys);//AT really needed for each systematic ? CHECK

        storeElectronKinSys(sysInfo,ourSys);
        storeMuonKinSys(sysInfo,ourSys);
        storeTauKinSys(sysInfo,ourSys);
        storeJetKinSys(sysInfo,ourSys);
        //storePhotonKinSys(sysInfo,ourSys);//To be implemented if needed

        fillMetVars(ourSys);    
        fillTrackMetVars(ourSys);
        // m_susyNt.evt()->cutFlags[sys] = m_cutFlags;

        //Reset the systematics for all tools
        if(m_dbg>=15) cout << "AT reset systematics " << endl;
        if ( m_susyObj[m_eleIDDefault]->resetSystematics() != CP::SystematicCode::Ok){
            cout << "SusyNtMaker::doSystematics -- Cannot reset SUSYTools systematics. Aborting " << endl;
            abort();
        }
        //m_susyObj[m_eleIDDefault]->resetSystematics();
        if(m_dbg>=15) cout << "AT reset systematics - done " << endl;
    }


}

/*--------------------------------------------------------------------------------*/
void SusyNtMaker::storeElectronKinSys(ST::SystInfo sysInfo, SusyNtSys sys)
{
    if(!ST::testAffectsObject(xAOD::Type::Electron, sysInfo.affectsType)) return;

    xAOD::ElectronContainer* electrons     = xaodElectrons(sysInfo,sys);
    xAOD::ElectronContainer* electrons_nom = xaodElectrons(sysInfo,NtSys::NOM);

    if(m_dbg>=15) cout << "storeElectronKinSys " << NtSys::SusyNtSysNames[sys]  << endl;
    for(const auto &iEl : m_preElectrons){ //loop over array containing the xAOD electron idx
        const xAOD::Electron* ele = electrons->at(iEl);
        if(m_dbg>=5) cout << "This ele pt " << ele->pt() << " eta " << ele->eta() << " phi " << ele->phi() << endl; 
        
        const xAOD::Electron* ele_nom = NULL;
        Susy::Electron* ele_susyNt = NULL;
        int idx_susyNt=-1;
        for(uint idx=0; idx<m_preElectrons_nom.size(); idx++){
            int iEl_nom = m_preElectrons_nom[idx];
            if(iEl == iEl_nom){
                ele_nom = electrons_nom->at(iEl_nom);
                ele_susyNt = & m_susyNt.ele()->at(idx);
                idx_susyNt=idx;
                if(m_dbg>=5){
                    cout << "Found matching electron sys: " << iEl << "  " << iEl_nom << " " << idx_susyNt << endl;
                    cout << "\t ele_nom pt " << ele_nom->pt() << " eta " << ele_nom->eta() << " phi " << ele_nom->phi() << endl; 
                    ele_susyNt->print();
                }
                if( fabs(ele_nom->eta() - ele->eta())>0.001 || fabs(ele_nom->phi() - ele->phi())>0.001)
                    cout << "WARNING SusyNtMaker::storeElectronKinSys index mis-match " << endl;
                break;
            }
        }
        
        //Electron was not found. Add it at its nominal scale to susyNt and m_preElectron_nom 
        if(ele_susyNt == NULL){
            if(m_dbg>=5) cout << " Electron not found - adding to susyNt" << endl;
            ele_nom = electrons_nom->at(iEl);//assume order is preserved
            storeElectron(*ele_nom);//this add the electron at the end... 
            m_preElectrons_nom.push_back(iEl);
            ele_susyNt = & m_susyNt.ele()->back(); //get the newly inserted element
        }
        
        //Calculate systematic SF: shift/nom  INSANE !!!!
        float sf = ele->e() / ele_nom->e();
        if(m_dbg>=5) cout << "Ele SF " << sf << endl;
        if     ( sys == NtSys::EG_RESOLUTION_ALL_DN ) ele_susyNt->res_all_dn = sf;
        else if( sys == NtSys::EG_RESOLUTION_ALL_UP ) ele_susyNt->res_all_up = sf;
/*
        else if( sys == NtSys::EG_RESOLUTION_MATERIALCALO_DN ) ele_susyNt->res_matCalo_dn = sf;
        else if( sys == NtSys::EG_RESOLUTION_MATERIALCALO_UP ) ele_susyNt->res_matCalo_up = sf;
        else if( sys == NtSys::EG_RESOLUTION_MATERIALCRYO_DN ) ele_susyNt->res_matCryo_dn = sf;
        else if( sys == NtSys::EG_RESOLUTION_MATERIALCRYO_UP ) ele_susyNt->res_matCryo_up = sf;
        else if( sys == NtSys::EG_RESOLUTION_MATERIALGAP_DN ) ele_susyNt->res_matGap_dn = sf;
        else if( sys == NtSys::EG_RESOLUTION_MATERIALGAP_UP) ele_susyNt->res_matGap_up = sf;
        else if( sys == NtSys::EG_RESOLUTION_MATERIALID_DN ) ele_susyNt->res_matId_dn = sf;
        else if( sys == NtSys::EG_RESOLUTION_MATERIALID_UP ) ele_susyNt->res_matId_up = sf;
        else if( sys == NtSys::EG_RESOLUTION_NOMINAL ) ele_susyNt->res_nom = sf;
        else if( sys == NtSys::EG_RESOLUTION_NONE ) ele_susyNt->res_none = sf;
        else if( sys == NtSys::EG_RESOLUTION_PILEUP_DN ) ele_susyNt->res_pileup_dn = sf;
        else if( sys == NtSys::EG_RESOLUTION_PILEUP_UP ) ele_susyNt->res_pileup_up = sf;
        else if( sys == NtSys::EG_RESOLUTION_SAMPLINGTERM_DN ) ele_susyNt->res_sampTerm_dn = sf;
        else if( sys == NtSys::EG_RESOLUTION_SAMPLINGTERM_UP ) ele_susyNt->res_sampTerm_up = sf;
        else if( sys == NtSys::EG_RESOLUTION_ZSMEARING_DN ) ele_susyNt->res_z_dn = sf;
        else if( sys == NtSys::EG_RESOLUTION_ZSMEARING_UP ) ele_susyNt->res_z_up = sf;
*/
        else if( sys == NtSys::EG_SCALE_ALL_DN ) ele_susyNt->scale_all_dn = sf;
        else if( sys == NtSys::EG_SCALE_ALL_UP ) ele_susyNt->scale_all_up = sf;
/*
        else if( sys == NtSys::EG_SCALE_G4_DN ) ele_susyNt->scale_G4_dn = sf;
        else if( sys == NtSys::EG_SCALE_G4_UP ) ele_susyNt->scale_G4_up = sf;
        else if( sys == NtSys::EG_SCALE_L1GAIN_DN ) ele_susyNt->scale_L1_dn = sf;
        else if( sys == NtSys::EG_SCALE_L1GAIN_UP ) ele_susyNt->scale_L1_up = sf;
        else if( sys == NtSys::EG_SCALE_L2GAIN_DN ) ele_susyNt->scale_L2_dn = sf;
        else if( sys == NtSys::EG_SCALE_L2GAIN_UP ) ele_susyNt->scale_L2_up = sf;
        else if( sys == NtSys::EG_SCALE_LARCALIB_DN ) ele_susyNt->scale_LArCalib_dn = sf;
        else if( sys == NtSys::EG_SCALE_LARCALIB_UP ) ele_susyNt->scale_LArCalib_up = sf;
        else if( sys == NtSys::EG_SCALE_LARELECCALIB_DN ) ele_susyNt->scale_LArECalib_dn = sf;
        else if( sys == NtSys::EG_SCALE_LARELECCALIB_UP ) ele_susyNt->scale_LArECalib_up = sf;
        else if( sys == NtSys::EG_SCALE_LARELECUNCONV_DN ) ele_susyNt->scale_LArEunconv_dn = sf;
        else if( sys == NtSys::EG_SCALE_LARELECUNCONV_UP ) ele_susyNt->scale_LArEunconv_up = sf;
        else if( sys == NtSys::EG_SCALE_LARUNCONVCALIB_DN ) ele_susyNt->scale_LArUnconv_dn = sf;
        else if( sys == NtSys::EG_SCALE_LARUNCONVCALIB_UP ) ele_susyNt->scale_LArUnconv_up = sf;
        else if( sys == NtSys::EG_SCALE_LASTSCALEVARIATION ) ele_susyNt->scale_last = sf;
        else if( sys == NtSys::EG_SCALE_MATCALO_DN ) ele_susyNt->scale_matCalo_dn = sf;
        else if( sys == NtSys::EG_SCALE_MATCALO_UP ) ele_susyNt->scale_matCalo_up = sf;
        else if( sys == NtSys::EG_SCALE_MATCRYO_DN ) ele_susyNt->scale_matCryo_dn = sf;
        else if( sys == NtSys::EG_SCALE_MATCRYO_UP ) ele_susyNt->scale_matCryo_up = sf;
        else if( sys == NtSys::EG_SCALE_MATID_DN ) ele_susyNt->scale_matId_dn = sf;
        else if( sys == NtSys::EG_SCALE_MATID_UP ) ele_susyNt->scale_matId_up = sf;
        else if( sys == NtSys::EG_SCALE_NOMINAL ) ele_susyNt->scale_nom = sf;
        else if( sys == NtSys::EG_SCALE_NONE ) ele_susyNt->scale_none = sf;
        else if( sys == NtSys::EG_SCALE_PEDESTAL_DN ) ele_susyNt->scale_ped_dn = sf;
        else if( sys == NtSys::EG_SCALE_PEDESTAL_UP ) ele_susyNt->scale_ped_up = sf;
        else if( sys == NtSys::EG_SCALE_PS_DN ) ele_susyNt->scale_ps_dn = sf;
        else if( sys == NtSys::EG_SCALE_PS_UP ) ele_susyNt->scale_ps_up = sf;
        else if( sys == NtSys::EG_SCALE_S12_DN ) ele_susyNt->scale_s12_dn = sf;
        else if( sys == NtSys::EG_SCALE_S12_UP ) ele_susyNt->scale_s12_up = sf;
        else if( sys == NtSys::EG_SCALE_ZEESTAT_DN ) ele_susyNt->scale_ZeeStat_dn = sf;
        else if( sys == NtSys::EG_SCALE_ZEESTAT_UP ) ele_susyNt->scale_ZeeStat_up = sf;
        else if( sys == NtSys::EG_SCALE_ZEESYST_DN ) ele_susyNt->scale_ZeeSys_dn = sf;
        else if( sys == NtSys::EG_SCALE_ZEESYST_UP ) ele_susyNt->scale_ZeeSys_up = sf;
        else if( sys == NtSys::EL_SCALE_MOMENTUM_DN ) ele_susyNt->scale_mom_dn = sf;
        else if( sys == NtSys::EL_SCALE_MOMENTUM_UP ) ele_susyNt->scale_mom_up = sf;
*/
    }
}

/*--------------------------------------------------------------------------------*/
void SusyNtMaker::storeMuonKinSys(ST::SystInfo sysInfo, SusyNtSys sys)
{
    if(!ST::testAffectsObject(xAOD::Type::Muon, sysInfo.affectsType)) return;

    xAOD::MuonContainer* muons     = xaodMuons(sysInfo,sys);
    xAOD::MuonContainer* muons_nom = xaodMuons(sysInfo, NtSys::NOM);
  
    if(m_dbg>=15) cout << "storeMuonKinSys "  << NtSys::SusyNtSysNames[sys] << endl;
    for(const auto &iMu : m_preMuons){//loop over array containing the xAOD muon idx
        const xAOD::Muon* mu = muons->at(iMu);
        if(m_dbg>=5) cout << "This mu pt " << mu->pt() << " eta " << mu->eta() << " phi " << mu->phi() << endl; 
        
        const xAOD::Muon* mu_nom = NULL;
        Susy::Muon* mu_susyNt = NULL;
        int idx_susyNt=-1;
        for(uint idx=0; idx<m_preMuons_nom.size(); idx++){
            int iMu_nom = m_preMuons_nom[idx];
            if(iMu == iMu_nom){
                mu_nom = muons_nom->at(iMu_nom);
                mu_susyNt = & m_susyNt.muo()->at(idx);
                idx_susyNt=idx;
                if(m_dbg>=5){
                    cout << "Found matching muon sys: " << iMu << "  " << iMu_nom << " " << idx_susyNt << endl;
                    cout << "mu_nom pt " << mu_nom->pt() << " eta " << mu_nom->eta() << " phi " << mu_nom->phi() << endl; 
                    mu_susyNt->print();
                }
                if( fabs(mu_nom->eta() - mu->eta())>0.001 || fabs(mu_nom->phi() - mu->phi())>0.001)
                    cout << "WARNING SusyNtMaker::storeMuonKinSys index mis-match " << endl;
                break;
            }
        }
    
        //Muon was not found. Add it at its nominal scale to susyNt and m_preMuon_nom 
        if(mu_susyNt == NULL){
            if(m_dbg>=5) cout << " Muon not found - adding to susyNt" << endl;
            mu_nom = muons_nom->at(iMu);//assume order is preserved
            storeMuon(*mu_nom);//this add the mu at the end... 
            m_preMuons_nom.push_back(iMu);
            mu_susyNt = & m_susyNt.muo()->back(); //get the newly inserted mument
        }

        //Calculate systematic SF: shift/nom
        float sf = mu->e() / mu_nom->e();
        if(m_dbg>=5) cout << "Muo SF " << sf << endl;
        if(sys == NtSys::MUONS_MS_UP)      mu_susyNt->ms_up = sf;
        else if(sys == NtSys::MUONS_MS_DN) mu_susyNt->ms_dn = sf;
        else if(sys == NtSys::MUONS_ID_UP) mu_susyNt->id_up = sf;
        else if(sys == NtSys::MUONS_ID_DN) mu_susyNt->id_dn = sf;
        else if(sys == NtSys::MUONS_SCALE_UP) mu_susyNt->scale_up = sf;
        else if(sys == NtSys::MUONS_SCALE_DN) mu_susyNt->scale_dn = sf;
    }
}

/*--------------------------------------------------------------------------------*/
void SusyNtMaker::storeJetKinSys(ST::SystInfo sysInfo, SusyNtSys sys)
{
    if(!ST::testAffectsObject(xAOD::Type::Jet, sysInfo.affectsType)) return;
    
    xAOD::JetContainer* jets     = xaodJets(sysInfo,sys);
    xAOD::JetContainer* jets_nom = xaodJets(sysInfo,NtSys::NOM);
  
    if(m_dbg>=15) cout << "storeJetKinSys "  << NtSys::SusyNtSysNames[sys] << endl;
    for(const auto &iJ : m_preJets){ //loop over array containing the xAOD electron idx
        const xAOD::Jet* jet = jets->at(iJ);
        if(m_dbg>=5) cout << "This jet pt " << jet->pt() << " eta " << jet->eta() << " phi " << jet->phi() << endl; 
    
        const xAOD::Jet* jet_nom = NULL;
        Susy::Jet* jet_susyNt = NULL;
        int idx_susyNt=-1;
        for(uint idx=0; idx<m_preJets_nom.size(); idx++){
            int iJ_nom = m_preJets_nom[idx];
            if(iJ == iJ_nom){
                jet_nom = jets_nom->at(iJ_nom);
                jet_susyNt = & m_susyNt.jet()->at(idx);
                idx_susyNt=idx;
                if(m_dbg>=5){
                    cout << "\t Found matching jet sys: " << iJ << "  " << iJ_nom << " " << idx_susyNt << endl;
                    cout << "\t jet_nom pt " << jet_nom->pt() << " eta " << jet_nom->eta() << " phi " << jet_nom->phi() << endl; 
                    cout << "\t"; jet_susyNt->print();
                }
                if( fabs(jet_nom->eta() - jet->eta())>0.001 || fabs(jet_nom->phi() - jet->phi())>0.001)
                    cout << "WARNING SusyNtMaker::savJetSF index mis-match " << endl;
                break;
            }
        }

        //Jet was not found. Add it at its nominal scale to susyNt and m_preJet_nom 
        if(jet_susyNt == NULL){
            if(m_dbg>=5) cout << "\t\tJet not found - adding to susyNt jet_nom idx  " << iJ << endl;
            jet_nom = jets_nom->at(iJ);//assume order is preserved
            storeJet(*jet_nom);//add the jet at the end... 
            m_preJets_nom.push_back(iJ);
            jet_susyNt = & m_susyNt.jet()->back(); //get the newly inserted jet
            if(m_dbg>=5) {cout << "\t"; jet_susyNt->print();}
        }

        //Calculate systematic SF: shift/nom
        float sf = jet->e() / jet_nom->e();
        if(m_dbg>=5) cout << "\t Jet SF " << sf << endl;

        if     ( sys == NtSys::JER)                jet_susyNt->jer = sf;
        else if( sys == NtSys::JET_GroupedNP_1_UP) jet_susyNt->groupedNP[0] = sf;
        else if( sys == NtSys::JET_GroupedNP_1_DN) jet_susyNt->groupedNP[1] = sf;
        else if( sys == NtSys::JET_GroupedNP_2_UP) jet_susyNt->groupedNP[2] = sf;
        else if( sys == NtSys::JET_GroupedNP_2_DN) jet_susyNt->groupedNP[3] = sf;
        else if( sys == NtSys::JET_GroupedNP_3_UP) jet_susyNt->groupedNP[4] = sf;
        else if( sys == NtSys::JET_GroupedNP_3_DN) jet_susyNt->groupedNP[5] = sf;

        /*
        else if( sys == NtSys::JET_BJES_Response_DN) jet_susyNt->bjes[0] = sf;
        else if( sys == NtSys::JET_BJES_Response_UP) jet_susyNt->bjes[1] = sf;
        else if( sys == NtSys::JET_EffectiveNP_1_DN) jet_susyNt->effNp[0] = sf;
        else if( sys == NtSys::JET_EffectiveNP_1_UP) jet_susyNt->effNp[1] = sf;
        else if( sys == NtSys::JET_EffectiveNP_2_DN) jet_susyNt->effNp[2] = sf;
        else if( sys == NtSys::JET_EffectiveNP_2_UP) jet_susyNt->effNp[3] = sf;
        else if( sys == NtSys::JET_EffectiveNP_3_DN) jet_susyNt->effNp[4] = sf;
        else if( sys == NtSys::JET_EffectiveNP_3_UP) jet_susyNt->effNp[5] = sf;
        else if( sys == NtSys::JET_EffectiveNP_4_DN) jet_susyNt->effNp[6] = sf;
        else if( sys == NtSys::JET_EffectiveNP_4_UP) jet_susyNt->effNp[7] = sf;
        else if( sys == NtSys::JET_EffectiveNP_5_DN) jet_susyNt->effNp[8] = sf;
        else if( sys == NtSys::JET_EffectiveNP_5_UP) jet_susyNt->effNp[9] = sf;
        else if( sys == NtSys::JET_EffectiveNP_6restTerm_DN) jet_susyNt->effNp[10] = sf;
        else if( sys == NtSys::JET_EffectiveNP_6restTerm_UP) jet_susyNt-> effNp[11] = sf;
        else if( sys == NtSys::JET_EtaIntercalibration_Modelling_DN) jet_susyNt->etaInter[0] = sf;
        else if( sys == NtSys::JET_EtaIntercalibration_Modelling_UP) jet_susyNt->etaInter[1]= sf;
        else if( sys == NtSys::JET_EtaIntercalibration_TotalStat_DN) jet_susyNt->etaInter[2] = sf;
        else if( sys == NtSys::JET_EtaIntercalibration_TotalStat_UP) jet_susyNt->etaInter[3] = sf;
        else if( sys == NtSys::JET_Flavor_Composition_DN) jet_susyNt->flavor[0] = sf;
        else if( sys == NtSys::JET_Flavor_Composition_UP) jet_susyNt->flavor[1] = sf;
        else if( sys == NtSys::JET_Flavor_Response_DN) jet_susyNt->flavor[2] = sf;
        else if( sys == NtSys::JET_Flavor_Response_UP) jet_susyNt->flavor[3] = sf;
        else if( sys == NtSys::JET_Pileup_OffsetMu_DN) jet_susyNt->pileup[0] = sf;
        else if( sys == NtSys::JET_Pileup_OffsetMu_UP) jet_susyNt-> pileup[1] = sf;
        else if( sys == NtSys::JET_Pileup_OffsetNPV_DN) jet_susyNt->pileup[2]= sf;
        else if( sys == NtSys::JET_Pileup_OffsetNPV_UP) jet_susyNt->pileup[3] = sf;
        else if( sys == NtSys::JET_Pileup_PtTerm_DN) jet_susyNt-> pileup[4] = sf;
        else if( sys == NtSys::JET_Pileup_PtTerm_UP) jet_susyNt-> pileup[5] = sf;
        else if( sys == NtSys::JET_Pileup_RhoTopology_DN) jet_susyNt-> pileup[6] = sf;
        else if( sys == NtSys::JET_Pileup_RhoTopology_UP) jet_susyNt-> pileup[7] = sf;
        else if( sys == NtSys::JET_PunchThrough_MC12_DN) jet_susyNt->punchThrough[0] = sf;
        else if( sys == NtSys::JET_PunchThrough_MC12_UP) jet_susyNt->punchThrough[1] = sf;
        else if( sys == NtSys::JET_SingleParticle_HighPt_DN) jet_susyNt->singlePart[0] = sf;
        else if( sys == NtSys::JET_SingleParticle_HighPt_UP) jet_susyNt->singlePart[1] = sf;
        //else if( sys == NtSys::JET_RelativeNonClosure_MC12_DN) jet_susyNt->relativeNC[0] = sf;
        //else if( sys == NtSys::JET_RelativeNonClosure_MC12_UP) jet_susyNt->relativeNC[1] = sf;
        */

    }
}

/*--------------------------------------------------------------------------------*/
void SusyNtMaker::storeTauKinSys(ST::SystInfo sysInfo, SusyNtSys sys)
{
    if(!ST::testAffectsObject(xAOD::Type::Tau, sysInfo.affectsType)) return;

    xAOD::TauJetContainer* taus     = xaodTaus(sysInfo,sys);
    xAOD::TauJetContainer* taus_nom = xaodTaus(sysInfo,NtSys::NOM);

    if(m_dbg>=15) cout << "storeTauKinSys " << NtSys::SusyNtSysNames[sys]  << endl;
    for(const auto &iTau : m_preTaus){ //loop over array containing the xAOD tau idx
        const xAOD::TauJet* tau = taus->at(iTau);
        if(m_dbg>=5)  cout << "This tau pt " << tau->pt() << " eta " << tau->eta() << " phi " << tau->phi() << endl; 
    
        const xAOD::TauJet* tau_nom = NULL;
        Susy::Tau* tau_susyNt = NULL;
        int idx_susyNt=-1;
        for(uint idx=0; idx<m_preTaus_nom.size(); idx++){
            int iTau_nom = m_preTaus_nom[idx];
            if(iTau == iTau_nom){
                tau_nom = taus_nom->at(iTau_nom);
                tau_susyNt = & m_susyNt.tau()->at(idx);
                idx_susyNt=idx;
                if(m_dbg>=5){
                    cout << "Found matching tau sys: " << iTau << "  " << iTau_nom << " " << idx_susyNt << endl;
                    cout << "tau_nom pt " << tau_nom->pt() << " eta " << tau_nom->eta() << " phi " << tau_nom->phi() << endl; 
                    tau_susyNt->print();
                }
                if( fabs(tau_nom->eta() - tau->eta())>0.001 || fabs(tau_nom->phi() - tau->phi())>0.001)
                    cout << "WARNING SusyNtMaker::storeTauKinSys index mis-match " << endl;
                break;
            }
        }

        //Tau was not found. Add it at its nominal scale to susyNt and m_preTau_nom 
        if(tau_susyNt == NULL){
            if(m_dbg>=5) cout << " Tau not found - adding to susyNt" << endl;
            tau_nom = taus_nom->at(iTau);//assume order is preserved
            storeTau(*tau_nom);//this add the tau at the end... 
            m_preTaus_nom.push_back(iTau);
            tau_susyNt = & m_susyNt.tau()->back(); //get the newly inserted taument
        }

        //Calculate systematic SF: shift/nom
        float sf = tau->e() / tau_nom->e();
        if(m_dbg>=5) cout << "Tau SF " << sf << endl;

        if(sys == NtSys::TAUS_SME_TOTAL_UP) tau_susyNt->sme_total_up = sf;
        if(sys == NtSys::TAUS_SME_TOTAL_DN) tau_susyNt->sme_total_dn = sf;
    }
}

/*--------------------------------------------------------------------------------*/
void SusyNtMaker::addMissingElectron(const LeptonInfo* lep, SusyNtSys sys)
{
    // // This electron did not pass nominal cuts, and therefore
    // // needs to be added, but with the correct TLV

    // // Reset the Nominal TLV
    // // NOTE: this overwrites the TLV in SUSYObjDef with the nominal variables,
    // // regardless of our current systematic.
    // const D3PDReader::ElectronD3PDObjectElement* element = lep->getElectronElement();
    // m_susyObj[m_eleIDDefault]->SetElecTLV(lep->idx(), element->eta(), element->phi(), element->cl_eta(), element->cl_phi(), element->cl_E(),
    //                      element->tracketa(), element->trackphi(), element->nPixHits(), element->nSCTHits(), SystErr::NONE);
    // // Now push it back onto to susyNt
    // fillElectronVars(lep);
}

/*--------------------------------------------------------------------------------*/
void SusyNtMaker::addMissingMuon(const LeptonInfo* lep, SusyNtSys sys)
{
    // // This muon did not pass nominal cuts, and therefore
    // // needs to be added, but with the correct TLV
    // // Reset the Nominal TLV
    // // NOTE: this overwrites the TLV in SUSYObjDef with the nominal variables,
    // // regardless of our current systematic.
    // const D3PDReader::MuonD3PDObjectElement* element = lep->getMuonElement();
    // m_susyObj[m_eleIDDefault]->SetMuonTLV(lep->idx(), element->pt(), element->eta(), element->phi(),
    //                      element->me_qoverp_exPV(), element->id_qoverp_exPV(), element->me_theta_exPV(),
    //                      element->id_theta_exPV(), element->charge(), element->isCombinedMuon(),
    //                      element->isSegmentTaggedMuon(), SystErr::NONE);
    // fillMuonVars(lep);
}

/*--------------------------------------------------------------------------------*/
void SusyNtMaker::addMissingJet(int index, SusyNtSys sys)
{
    // // Get the systematic shifted E, used to calculate a shift factor
    // //TLorentzVector tlv_sys = m_susyObj[m_eleIDDefault]->GetJetTLV(index);
    // //float E_sys = m_susyObj[m_eleIDDefault]->GetJetTLV(index).E();

    // // Reset the Nominal TLV
    // // NOTE: this overwrites the TLV in SUSYObjDef with the nominal variables,
    // // regardless of our current systematic.
    // const D3PDReader::JetD3PDObjectElement* jet = &m_event.jet_AntiKt4LCTopo[index];
    // m_susyObj[m_eleIDDefault]->FillJet(index, jet->pt(), jet->eta(), jet->phi(), jet->E(),
    //                   jet->constscale_eta(), jet->constscale_phi(), jet->constscale_E(), jet->constscale_m(),
    //                   jet->ActiveAreaPx(), jet->ActiveAreaPy(), jet->ActiveAreaPz(), jet->ActiveAreaE(),
    //                   m_event.Eventshape.rhoKt4LC(),
    //                   m_event.eventinfo.averageIntPerXing(),
    //                   m_event.vxp.nTracks());
    // fillJetVar(index);
    // // Set SF This should only be done in storeJetKinSys
}

/*--------------------------------------------------------------------------------*/
void SusyNtMaker::addMissingTau(int index, SusyNtSys sys)
{
    // // This tau did not pass nominal cuts, and therefore
    // // needs to be added, but with the correct TLV.
    // // Get the systematic shifted E, used to calculate a shift factor
    // //TLorentzVector tlv_sys = m_susyObj[m_eleIDDefault]->GetTauTLV(index);
    // //float E_sys = m_susyObj[m_eleIDDefault]->GetTauTLV(index).E();
    // // Grab the d3pd variables
    // const D3PDReader::TauD3PDObjectElement* element = & m_event.tau[index];
    // // Reset the Nominal TLV
    // // NOTE: this overwrites the TLV in SUSYObjDef with the nominal variables,
    // // regardless of our current systematic.
    // m_susyObj[m_eleIDDefault]->SetTauTLV(index, element->pt(), element->eta(), element->phi(), element->Et(), element->numTrack(),
    //                     element->leadTrack_eta(), SUSYTau::TauMedium, SystErr::NONE, true);
    // // Fill the tau vars for this guy
    // fillTauVar(index);
}
/*--------------------------------------------------------------------------------*/
//bool SusyNtMaker::isBuggyWwSherpaSample(const int &dsid)
//{
//  return (dsid==126892 || dsid==157817 || dsid==157818 || dsid==157819);
//}
/*--------------------------------------------------------------------------------*/
//bool SusyNtMaker::hasRadiativeBquark(const vint_t *pdg, const vint_t *status)
//{
//  if(!pdg || !status || pdg->size()!=status->size()) return false;
//  const vint_t &p = *pdg;
//  const vint_t &s = *status;
//  const int pdgB(5), statRad(3);
//  for(size_t i=0; i<p.size(); ++i) if(abs(p[i])==pdgB && s[i]==statRad) return true;
//  return false;
//}
//----------------------------------------------------------
SusyNtMaker& SusyNtMaker::initializeOuputTree()
{
    m_outTreeFile = new TFile("susyNt.root", "recreate");
    //m_outTreeFile->SetCompressionLevel(9); //Default =1, 9 is max AT:05-02-15
    m_outTree = new TTree("susyNt", "susyNt");
    m_outTree->SetAutoSave(10000000); // DG-2014-08-15 magic numbers, ask Steve
    m_outTree->SetMaxTreeSize(3000000000u);
    m_susyNt.SetActive();
    m_susyNt.WriteTo(m_outTree);
    return *this;
}
//----------------------------------------------------------
SusyNtMaker& SusyNtMaker::initializeCutflowHistograms()
{
    h_rawCutFlow = makeCutFlow("rawCutFlow", "rawCutFlow;Cuts;Events");
    h_genCutFlow = makeCutFlow("genCutFlow", "genCutFlow;Cuts;Events");
    std::vector<std::string> trigs = XaodAnalysis::xaodTriggers();
    h_passTrigLevel = new TH1F("trig", "Event Level Triggers Fired", trigs.size()+1, 0.0, trigs.size()+1); // dantrim trig
    h_passTrigLevel->GetXaxis()->SetLabelSize(0.8 * h_passTrigLevel->GetLabelSize());
    for ( unsigned int iTrig = 0; iTrig < trigs.size(); iTrig++) {
        h_passTrigLevel->GetXaxis()->SetBinLabel(iTrig+1, trigs[iTrig].c_str());
    }
    return *this;
}
//----------------------------------------------------------
SusyNtMaker& SusyNtMaker::writeMetadata()
{
    struct {
        string operator()(const string &s) { return (s.size()==0 ? " Warning, empty string!" : ""); }
    } warn_if_empty;
    if(m_dbg){
        cout<<"Writing the following info to file:"<<endl
            <<"m_inputContainerName: '"<<m_inputContainerName<<"'"<<warn_if_empty(m_inputContainerName)<<endl
            <<"m_outputContainerName: '"<<m_outputContainerName<<"'"<<warn_if_empty(m_outputContainerName)<<endl
            <<"m_productionTag: '"<<m_productionTag<<"'"<<warn_if_empty(m_productionTag)<<endl
            <<"m_productionCommand: '"<<m_productionCommand<<"'"<<warn_if_empty(m_productionCommand)<<endl;
    }
    if(m_outTreeFile){
        TDirectory *current_directory = gROOT->CurrentDirectory();
        m_outTreeFile->cd();
        TNamed inputContainerName("inputContainerName", m_inputContainerName.c_str());
        TNamed outputContainerName("outputContainerName", m_outputContainerName.c_str());
        TNamed productionTag("productionTag", m_productionTag.c_str());
        TNamed productionCommand("productionCommand", m_productionCommand.c_str());
        inputContainerName.Write();
        outputContainerName.Write();
        productionTag.Write();
        productionCommand.Write();
        current_directory->cd();
    } else {
        cout<<"SusyNtMaker::writeMetadata: missing output file, cannot write"<<endl;
    }
    return *this;
}
//----------------------------------------------------------
bool SusyNtMaker::guessWhetherIsWhSample(const TString &samplename)
{
    return (samplename.Contains("simplifiedModel_wA_noslep_WH") ||
            samplename.Contains("Herwigpp_sM_wA_noslep_notauhad_WH"));
}
//----------------------------------------------------------
SusyNtMaker& SusyNtMaker::saveOutputTree()
{
    m_outTreeFile = m_outTree->GetCurrentFile();
    m_outTreeFile->Write(0, TObject::kOverwrite);
    cout<<"susyNt tree saved to "<<m_outTreeFile->GetName()<<endl;
    writeMetadata();
    m_outTreeFile->Close();
    return *this;
}
//----------------------------------------------------------
std::string SusyNtMaker::timerSummary() /*const*/ // TStopwatch::<*>Time is not const
{
    double realTime = m_timer.RealTime();
    double cpuTime  = m_timer.CpuTime();
    int hours = int(realTime / 3600);
    realTime -= hours * 3600;
    int min   = int(realTime / 60);
    realTime -= min * 60;
    int sec   = int(realTime);
    int nEventInput = m_cutstageCounters.front();
    int nEventOutput = m_outTree ? m_outTree->GetEntries() : -1;
    float speed = nEventInput/m_timer.RealTime()/1000;
    TString line1; line1.Form("Real %d:%02d:%02d, CPU %.3f", hours, min, sec, cpuTime);
    TString line2; line2.Form("[kHz]: %2.3f",speed);
    ostringstream oss;
    oss<<"---------------------------------------------------\n"
       <<" Number of events processed: "<<nEventInput<<endl
       <<" Number of events saved:     "<<nEventOutput<<endl
       <<"\t Analysis time: "<<line1<<endl
       <<"\t Analysis speed "<<line2<<endl
       <<"---------------------------------------------------"<<endl
       <<endl;
    return oss.str();
}
//----------------------------------------------------------
std::string SusyNtMaker::counterSummary() const
{
    ostringstream oss;
    oss<<"Object counter"<<endl
       <<"  PreEle    "<<n_pre_ele   <<endl
       <<"  PreMuo    "<<n_pre_muo   <<endl
       <<"  PreTau    "<<n_pre_tau   <<endl
       <<"  PreJet    "<<n_pre_jet   <<endl

       <<"  BaseEle   "<<n_base_ele   <<endl
       <<"  BaseMuo   "<<n_base_muo   <<endl
       <<"  BaseTau   "<<n_base_tau   <<endl
       <<"  BaseJet   "<<n_base_jet   <<endl

       <<"  SigEle    "<<n_sig_ele    <<endl
       <<"  SigMuo    "<<n_sig_muo    <<endl
       <<"  SigTau    "<<n_sig_tau    <<endl
       <<"  SigJet    "<<n_sig_jet    <<endl
       <<endl;

    oss<<"Event counter"<<endl;
    vector<string> labels = SusyNtMaker::cutflowLabels();
    struct shorter { bool operator()(const string &a, const string &b) { return a.size() < b.size(); } };
    size_t max_label_length = max_element(labels.begin(), labels.end(), shorter())->size();

    for(size_t i=0; i<m_cutstageCounters.size(); ++i)
        oss<<"  "<<setw(max_label_length+2)<<std::left<<labels[i]<<m_cutstageCounters[i]<<endl;
    oss<<endl;
    return oss.str();
}
//----------------------------------------------------------
struct FillCutFlow { ///< local function object to fill the cutflow histograms
    TH1 *raw, *gen, *perProcess; ///< ptr to histos with counters
    int iCut; ///< index of the sequential cut (must match bin labels, see SusyNtMaker::makeCutFlow())
    bool passAll; ///< whether we've survived all cuts so far
    bool includeThisCut_; ///< whether this cut should be used when computing passAll
    vector< size_t > *counters;
    FillCutFlow(TH1 *r, TH1* g, TH1* p, vector< size_t > *cs) :
        raw(r), gen(g), perProcess(p), iCut(0), passAll(true), includeThisCut_(true), counters(cs) {}
    FillCutFlow& operator()(bool thisEventDoesPassThisCut, float weight) {
        if(thisEventDoesPassThisCut && passAll) {
            if(raw       ) raw       ->Fill(iCut);
            if(gen       ) gen       ->Fill(iCut, weight);
            if(perProcess) perProcess->Fill(iCut, weight);
            counters->at(iCut) += 1;
        } else {
            if(includeThisCut_) passAll = false;
        }
        iCut++;
        return *this;
    }
    FillCutFlow& disableFilterNextCuts() { includeThisCut_ = false; return *this; }
    FillCutFlow& enableFilterNextCuts() { includeThisCut_ = true; return *this; }
};
//----------------------------------------------------------
void SusyNtMaker::fillTriggerHisto()
{
    std::vector<std::string> trigs = XaodAnalysis::xaodTriggers();
    for ( unsigned int iTrig = 0; iTrig < trigs.size(); iTrig++ ) {
        if(m_susyObj[m_eleIDDefault]->IsTrigPassed(trigs[iTrig])) h_passTrigLevel->Fill(iTrig+0.5);
        //if(m_trigTool->isPassed(trigs[iTrig]))         h_passTrigLevel->Fill(iTrig+0.5);
    }
}
//----------------------------------------------------------
bool SusyNtMaker::passEventlevelSelection()
{
    const xAOD::EventInfo* eventinfo = XaodAnalysis::xaodEventInfo();
    float w = m_isMC ? eventinfo->mcEventWeight() : 1;

    TH1F* h_procCutFlow = getProcCutFlow(m_susyFinalState);

    FillCutFlow fillCutFlow(h_rawCutFlow, h_genCutFlow, h_procCutFlow, &m_cutstageCounters);

    assignEventCleaningFlags();
    bool keep_all_events(!m_filter);

    bool pass_grl(m_cutFlags & ECut_GRL);
    bool pass_lar(m_cutFlags & ECut_LarErr);
    bool pass_tile(m_cutFlags & ECut_TileErr);
    bool pass_TTC(m_cutFlags & ECut_TTC);
    bool pass_errorFlags(pass_lar && pass_tile && pass_TTC);

    fillCutFlow(true, w); // initial bin (total read-in)
    fillCutFlow(pass_grl, w);
    fillCutFlow(pass_errorFlags, w); ///< used in cutflow

    if(m_dbg>=5 &&  !(keep_all_events || fillCutFlow.passAll) ) 
        cout << "SusyNtMaker fail passEventlevelSelection " 
             << keep_all_events << " " << fillCutFlow.passAll <<  endl;
    return (keep_all_events || fillCutFlow.passAll);
}
//----------------------------------------------------------
bool SusyNtMaker::passObjectlevelSelection()
{
    const xAOD::EventInfo* eventinfo = XaodAnalysis::xaodEventInfo();
    float w = m_isMC ? eventinfo->mcEventWeight() : 1; 

    SusyNtSys sys=NtSys::NOM;
    ST::SystInfo sysInfo =  systInfoList[0];//nominal
    selectObjects(sys,sysInfo);
    // buildMet(sys);  //AT: 12/16/14: Should retreive Met be called so that can add cut on met as event filter ?
     
    assignObjectCleaningFlags(sysInfo, sys);

    n_pre_ele += m_preElectrons.size();
    n_pre_muo += m_preMuons.size();
    n_pre_tau += m_preTaus.size();
    n_pre_jet += m_preJets.size();
    n_base_ele += m_baseElectrons.size();
    n_base_muo += m_baseMuons.size();
    n_base_tau += m_baseTaus.size();
    n_base_jet += m_baseJets.size();
    n_sig_ele += m_sigElectrons.size();
    n_sig_muo += m_sigMuons.size();
    n_sig_tau += m_sigTaus.size();
    n_sig_jet += m_sigJets.size();

    TH1F* h_procCutFlow = getProcCutFlow(m_susyFinalState);
    FillCutFlow fillCutFlow(h_rawCutFlow, h_genCutFlow, h_procCutFlow, &m_cutstageCounters);
    fillCutFlow.iCut = 3; // we've filled up to 'Primary vertex' in passEvent...

    bool pass_JetCleaning(m_cutFlags & ECut_BadJet);
    bool pass_goodpv(m_cutFlags & ECut_GoodVtx);
    bool pass_bad_muon(m_cutFlags & ECut_BadMuon);
    bool pass_cosmic(m_cutFlags & ECut_Cosmic);
    
    bool pass_ge1bl(1>=(m_baseElectrons.size()+m_baseMuons.size()));
    bool pass_exactly1sig(1==(m_sigElectrons.size()+m_sigMuons.size()));
    bool pass_exactly1base(1==(m_baseElectrons.size()+m_baseMuons.size()));
    bool pass_exactly2base(2==(m_baseElectrons.size()+m_baseMuons.size()));
    bool pass_ge1sl(1>=(m_sigElectrons.size()+m_sigMuons.size()));
    bool pass_e1j(1==(m_baseJets.size()));
    bool pass_e1sj(1==(m_sigJets.size()));
    bool pass_exactly2sig(2==(m_sigElectrons.size()+m_sigMuons.size()));

    fillCutFlow(pass_goodpv, w);
    fillCutFlow(pass_bad_muon, w);
    fillCutFlow(pass_cosmic, w);
    fillCutFlow(pass_JetCleaning, w);
    fillCutFlow(pass_ge1bl, w);
    fillCutFlow(pass_ge1sl, w);


    // filter
    bool pass = true;
    bool pass_nLepFilter( (m_preElectrons.size()+m_preMuons.size()) >= m_nLepFilter );
    if(m_filter) {
        pass = pass_nLepFilter;
    }
  //  bool trig_has_fired( h_passTrigLevel->Integral(0,-1) > 0. ); // check if any of the triggers fired
  //  if(m_filter) {
  //      if(m_filterTrigger) { pass = (pass_nLepFilter && trig_has_fired); }
  //      else { pass = pass_nLepFilter; }
  //  }
    if(m_dbg>=5 && !pass)
        cout << "SusyNtMaker: fail passObjectlevelSelection " << endl;
    return pass;

}
//----------------------------------------------------------
