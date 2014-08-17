#include "egammaAnalysisUtils/CaloIsoCorrection.h"

//#include "TauCorrections/TauCorrections.h"
#include "TauCorrUncert/TauSF.h"
//#include "SUSYTools/MV1.h"

#include "SusyCommon/SusyNtMaker.h"
#include "SusyCommon/TruthTools.h"
#include "SusyNtuple/SusyNtTools.h"
#include "SusyNtuple/WhTruthExtractor.h"
#include "SusyNtuple/mc_truth_utils.h"

#include "ElectronEfficiencyCorrection/TElectronEfficiencyCorrectionTool.h"


#include "xAODTruth/TruthEvent.h"
#include "xAODTruth/TruthEventContainer.h"

#include <algorithm> // max_element
#include <iomanip> // setw
#include <sstream> // std::ostringstream

using namespace std;
namespace smc =susy::mc;

using susy::SusyNtMaker;

#define GeV 1000.

/*--------------------------------------------------------------------------------*/
// SusyNtMaker Constructor
/*--------------------------------------------------------------------------------*/
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
    m_cutstageCounters(SusyNtMaker::cutflowLabels().size(), 0)
{
  n_base_ele=0;
  n_base_muo=0;
  n_base_tau=0;
  n_base_jet=0;
  n_sig_ele=0;
  n_sig_muo=0;
  n_sig_tau=0;
  n_sig_jet=0;
}
/*--------------------------------------------------------------------------------*/
// Destructor
/*--------------------------------------------------------------------------------*/

SusyNtMaker::~SusyNtMaker()
{
}
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::SlaveBegin(TTree* tree)
{
  XaodAnalysis::SlaveBegin(tree);
  if(m_dbg)
      cout<<"SusyNtMaker::SlaveBegin"<<endl;
  if(m_fillNt)
      initializeOuputTree();
  m_isWhSample = guessWhetherIsWhSample(m_sample);
  initializeCutflowHistograms();
  m_timer.Start();
}
/*--------------------------------------------------------------------------------*/
const std::vector< std::string > SusyNtMaker::cutflowLabels()
{
    vector<string> labels;
    labels.push_back("Initial"        );
    labels.push_back("SusyProp Veto"  );
    labels.push_back("GRL"            );
    labels.push_back("LAr Error"      );
    labels.push_back("Tile Error"     );
    labels.push_back("TTC Veto"       );
    labels.push_back("Good Vertex"    );
    labels.push_back("Buggy WWSherpa" );
    labels.push_back("Hot Spot"       );
    labels.push_back("Bad Jet"        );
    labels.push_back("Bad Muon"       );
    labels.push_back("Cosmic"         );
    labels.push_back(">=1 lep"        );
    labels.push_back(">=2 lep"        );
    labels.push_back("==3 lep"        );
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
/*--------------------------------------------------------------------------------*/
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

/*--------------------------------------------------------------------------------*/
// Main process loop function
/*--------------------------------------------------------------------------------*/
Bool_t SusyNtMaker::Process(Long64_t entry)
{
  // Communicate the entry number to the interface objects
  m_event.getEntry(entry);

  const xAOD::EventInfo* eventinfo = 0;
  m_event.retrieve(eventinfo, "EventInfo");
  cout<<"run "<<eventinfo->eventNumber()<<" event "<<eventinfo->runNumber()<<endl;

  if(!m_flagsHaveBeenChecked) {
      m_flagsAreConsistent = runningOptionsAreValid();
      m_flagsHaveBeenChecked=true;
      if(!m_flagsAreConsistent) {
          cout<<"ERROR: Inconsistent options. Stopping here."<<endl;
          abort();
      }
  }

  static Long64_t chainEntry = -1;
  chainEntry++;
  if(m_dbg || chainEntry%5000==0)
  {
    cout << "**** Processing entry " << setw(6) << chainEntry
         << " run " << setw(6) << eventinfo->runNumber()
         << " event " << setw(7) << eventinfo->eventNumber() << " ****" << endl;
  }

  if(selectEvent() && m_fillNt){
    int bytes = m_outTree->Fill();
    if(bytes==-1){
      cout << "SusyNtMaker ERROR filling tree!  Abort!" << endl;
      abort();
    }
  }

  return kTRUE;
}

/*--------------------------------------------------------------------------------*/
// The Terminate() function is the last function to be called during
// a query. It always runs on the client, it can be used to present
// the results graphically or save the results to file.
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::Terminate()
{
  XaodAnalysis::Terminate();
  m_timer.Stop();
  if(m_dbg) cout<<"SusyNtMaker::Terminate"<<endl;
  cout<<counterSummary()<<endl;
  cout<<timerSummary()<<endl;
  if(m_fillNt) saveOutputTree();
}

/*--------------------------------------------------------------------------------*/
// Select event
/*--------------------------------------------------------------------------------*/
bool isSimplifiedModel(const TString &sampleName)
{
    return sampleName.Contains("simplifiedModel");
}
//----------------------------------------------------------
bool SusyNtMaker::selectEvent()
{
  if(m_dbg>=5) cout << "selectEvent" << endl;
  clearObjects();
  m_susyNt.clear();
  return (passEventlevelSelection() &&
          passObjectlevelSelection());
}
// bool SusyNtMaker::selectEvent()
// {

//   //-=-=-=-=-=-=-=-=-=-=-=-=-=-=-//
//   // Get Nominal Objects

//   selectObjects();
//   buildMet();
//   //evtCheck();
//   assignObjectCleaningFlags();


//   // These next cuts are not used to filter the SusyNt because they depend on systematics.
//   // Instead, they are simply used for the counters, for comparing the cutflow

//   // Tile hot spot
//   if(m_cutFlags & ECut_HotSpot)
//   {
//     fillCutFlow(w);
//     n_evt_hotSpot++;

//     // Bad jet cut
//     if(m_cutFlags & ECut_BadJet)
//     {
//       fillCutFlow(w);
//       n_evt_badJet++;

//       // Bad muon veto
//       if(m_cutFlags & ECut_BadMuon)
//       {
// 	fillCutFlow(w);
//         n_evt_badMu++;

//         // Cosmic muon veto
//         if(m_cutFlags & ECut_Cosmic)
//         {
//           fillCutFlow(w);
//           n_evt_cosmic++;

//           n_base_ele += m_baseElectrons.size();
//           n_base_muo += m_baseMuons.size();
//           n_base_tau += m_baseTaus.size();
//           n_base_jet += m_baseJets.size();
//           n_sig_ele += m_sigElectrons.size();
//           n_sig_muo += m_sigMuons.size();
//           n_sig_tau += m_sigTaus.size();
//           n_sig_jet += m_sigJets.size();

//           // Lepton multiplicity
//           uint nSigLep = m_sigElectrons.size() + m_sigMuons.size();
//           //cout << "nSigLep " << nSigLep << endl;
//           if(nSigLep >= 1){
//             fillCutFlow(w);
//             n_evt_1Lep++;
//             if(nSigLep >= 2){
//               fillCutFlow(w);
//               n_evt_2Lep++;
//               if(nSigLep == 3){
//                 fillCutFlow(w);
//                 n_evt_3Lep++;
//                 //cout << "Event " << d3pd.evt.EventNumber() << endl;
//               }
//             }
//           }
//         }
//       }
//     }
//   }

//   // Match the triggers
//   // Will this work for systematic leptons?
//   // I think so.
//   matchTriggers();

//   // Setup reco truth matching
//   if(m_isMC){
//     D3PDReader::TruthParticleD3PDObject  &mc = m_event.mc;
//     D3PDReader::EventInfoD3PDObject    &info = m_event.eventinfo;
//     D3PDReader::JetD3PDObject          &jets = m_event.jet_AntiKt4LCTopo;
//     D3PDReader::ElectronD3PDObject    &elecs = m_event.el;
//     D3PDReader::TruthMuonD3PDObject &truthMu = m_event.muonTruth;
//     D3PDReader::TrackParticleD3PDObject &trk = m_event.trk;
//     m_recoTruthMatch = RecoTauMatch(0.1, info.mc_channel_number(),
//                                     mc.n(), mc.barcode(), mc.status(), mc.pdgId(),
//                                     mc.parents(), mc.children(),
//                                     mc.pt(), mc.eta(), mc.phi(), mc.m(),
//                                     jets.pt(), jets.eta(), jets.phi(), jets.m(),
//                                     jets.flavor_truth_label(),
//                                     elecs.pt(), elecs.eta(), elecs.phi(), elecs.m(),
//                                     elecs.type(), elecs.origin(),
//                                     truthMu.pt(), truthMu.eta(), truthMu.phi(), truthMu.m(),
//                                     truthMu.type(), truthMu.origin(),
//                                     trk.pt(), trk.eta(), trk.phi_wrtPV(), trk.mc_barcode());
//    }

//   if(m_fillNt){

//     // This will fill the pre selected
//     // objects prior to overlap removal
//     fillNtVars();

//     // If it is mc and option for sys is set
//     if(m_isMC && m_sys) doSystematic();

//     // For filling the output tree, filter on number of saved light leptons and taus
//     if(m_filter){
//       uint nLepSaved = m_susyNt.ele()->size() + m_susyNt.muo()->size();
//       uint nTauSaved = m_susyNt.tau()->size();
//       if(nLepSaved < m_nLepFilter) return false;
//       if((nLepSaved + nTauSaved) < m_nLepTauFilter) return false;
//     }

//     // Trigger filtering, only save events for which one of our triggers has fired
//     if(m_filterTrigger && (m_evtTrigFlags == 0)) return false;
//   }

//   return true;
// }

/*--------------------------------------------------------------------------------*/
// Fill SusyNt variables
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::fillNtVars()
{
  fillEventVars();
  fillLeptonVars();
  fillJetVars();
  fillTauVars();
  fillMetVars();
  fillPhotonVars();
  if(m_isMC && getSelectTruthObjects() ) {
    fillTruthParticleVars();
    fillTruthJetVars();
    fillTruthMetVars();
  }
}

/*--------------------------------------------------------------------------------*/
// Fill Event variables
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::fillEventVars()
{
  if(m_dbg>=5) cout << "fillEventVars" << endl;
  Susy::Event* evt = m_susyNt.evt();
  const xAOD::EventInfo* eventinfo = 0; // todo: do this once and set the pointer (access through XaodAnalysis::eventinfo())
  m_event.retrieve(eventinfo, "EventInfo");

  evt->run              = eventinfo->runNumber();
  evt->event            = eventinfo->eventNumber();
  evt->lb               = eventinfo->lumiBlock();
  evt->stream           = m_stream;

  evt->isMC             = m_isMC;
  evt->mcChannel        = m_isMC? eventinfo->mcChannelNumber() : 0;
  evt->w                = m_isMC? eventinfo->mcEventWeight()   : 1; // \todo DG this now has an arg, is 0 the right one?
  //evt->larError         = m_event.eventinfo.larError();  // \todo DG still relevant?

  evt->nVtx             = getNumGoodVtx();
  evt->avgMu            = eventinfo->averageInteractionsPerCrossing();

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
  evt->trigFlags        = m_evtTrigFlags;

  // evt->wPileup          = m_isMC? getPileupWeight() : 1;
  // evt->wPileup_up       = m_isMC? getPileupWeightUp() : 1;
  // evt->wPileup_dn       = m_isMC? getPileupWeightDown() : 1;
  evt->xsec             = m_isMC? getXsecWeight() : 1;
  evt->errXsec          = m_isMC? m_errXsec : 1;
  evt->sumw             = m_isMC? m_sumw : 1;

	  
  const xAOD::TruthEventContainer* truthEvent = 0;
  m_event.retrieve(truthEvent, "TruthEvent");
  xAOD::TruthEventContainer::const_iterator truthE_itr = truthEvent->begin();
  ( *truthE_itr )->pdfInfoParameter(evt->pdf_id1  , xAOD::TruthEvent::id1);
  ( *truthE_itr )->pdfInfoParameter(evt->pdf_id2  , xAOD::TruthEvent::id2);
  // ( *truthE_itr )->pdfInfoParameter(evt->pdf_x1    , xAOD::TruthEvent::pdf1);
  // ( *truthE_itr )->pdfInfoParameter(evt->pdf_x2    , xAOD::TruthEvent::pdf2);
  // DG not working? ( *truthE_itr )->pdfInfoParameter(float(evt->pdf_scale), xAOD::TruthEvent::scalePDF);
  // DG what are these two?
  // ( *truthE_itr )->pdfInfoParameter(evt->pdf_x1   , xAOD::TruthEvent::x1);
  // ( *truthE_itr )->pdfInfoParameter(evt->pdf_x2   , xAOD::TruthEvent::x2);
  evt->pdfSF            = m_isMC? getPDFWeight8TeV() : 1;
  m_susyNt.evt()->cutFlags[NtSys_NOM] = m_cutFlags;
}

/*--------------------------------------------------------------------------------*/
// Fill lepton variables
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::fillLeptonVars()
{
#warning fillLeptonVars not implemented
  // // loop over preselected leptons and fill the output tree
  // for(uint iLep=0; iLep < m_preLeptons.size(); iLep++){
  //   const LeptonInfo* lep = & m_preLeptons[iLep];
  //   if(lep->isElectron()) fillElectronVars(lep);
  //   else fillMuonVars(lep);
  // }
}
//----------------------------------------------------------
void get_electron_eff_sf(float& sf, float& uncert,
                         const float &el_cl_eta, const float &pt,
                         bool recoSF, bool idSF, bool triggerSF, bool isAF2,
                         Root::TElectronEfficiencyCorrectionTool* electronRecoSF,
                         Root::TElectronEfficiencyCorrectionTool* electronIDSF,
                         Root::TElectronEfficiencyCorrectionTool* electronTriggerSF,
                         int RunNumber)
{
    sf = 1;
    uncert = 0;
    PATCore::ParticleDataType::DataType dataType;
    if(isAF2) dataType = PATCore::ParticleDataType::Fast;
    else dataType = PATCore::ParticleDataType::Full;

    if(recoSF && electronRecoSF){
        const Root::TResult &resultReco = electronRecoSF->calculate(dataType, RunNumber, el_cl_eta, pt);
        sf *= resultReco.getScaleFactor();
        uncert = resultReco.getTotalUncertainty();
    }
    if(idSF && electronIDSF){
        const Root::TResult &resultID = electronIDSF->calculate(dataType, RunNumber, el_cl_eta, pt);
        sf *= resultID.getScaleFactor();
        uncert = sqrt(pow(uncert,2) + pow(resultID.getTotalUncertainty(),2));
    }
    if(triggerSF && electronTriggerSF){
        const Root::TResult &resultTrigger = electronTriggerSF->calculate(dataType, RunNumber, el_cl_eta, pt);
        sf *= resultTrigger.getScaleFactor();
        uncert = sqrt(pow(uncert,2) + pow(resultTrigger.getTotalUncertainty(),2));
    }
}
//----------------------------------------------------------
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::fillElectronVars(const LeptonInfo* lepIn)
{
#warning fillElectronVars not implemented
  // if(m_dbg>=5) cout << "fillElectronVars" << endl;
  // m_susyNt.ele()->push_back( Susy::Electron() );
  // Susy::Electron* eleOut = & m_susyNt.ele()->back();
  // const D3PDReader::ElectronD3PDObjectElement* element = lepIn->getElectronElement();

  // // LorentzVector
  // const TLorentzVector* lv = lepIn->lv();
  // float pt  = lv->Pt() / GeV;
  // float eta = lv->Eta();
  // float phi = lv->Phi();
  // float m   = lv->M() / GeV;

  // eleOut->SetPtEtaPhiM(pt, eta, phi, m);
  // eleOut->pt            = pt;
  // eleOut->eta           = eta;
  // eleOut->phi           = phi;
  // eleOut->m             = m;

  // // TODO: clean this up, group things together, etc.

  // eleOut->etcone20      = element->topoEtcone20_corrected()/GeV;
  // eleOut->ptcone20      = element->ptcone20()/GeV;
  // eleOut->ptcone30      = element->ptcone30()/GeV;
  // eleOut->q             = element->charge();
  // eleOut->mcType        = m_isMC? element->type() : 0;
  // eleOut->mcOrigin      = m_isMC? element->origin() : 0;
  // eleOut->clusE         = element->cl_E()/GeV;
  // eleOut->clusEta       = element->cl_eta();
  // eleOut->clusPhi       = element->cl_phi();
  // eleOut->trackPt       = element->trackpt()/GeV;

  // // Check for charge flip
  // eleOut->isChargeFlip          = m_isMC? m_recoTruthMatch.isChargeFlip(*lv, element->charge()) : false;
  // eleOut->matched2TruthLepton   = m_isMC? m_recoTruthMatch.Matched2TruthLepton(*lv) : false;
  // eleOut->truthType             = m_isMC? m_recoTruthMatch.fakeType(*lv, element->origin(), element->type()) : -1;

  // // IsEM quality flags - no need to recalculate them
  // eleOut->mediumPP    = element->mediumPP();
  // eleOut->tightPP     = element->tightPP();

  // eleOut->d0            = element->trackd0pv();
  // eleOut->errD0         = element->tracksigd0pv();
  // eleOut->z0            = element->trackz0pv();
  // eleOut->errZ0         = element->tracksigz0pv();

  // eleOut->d0Unbiased    = element->trackIPEstimate_d0_unbiasedpvunbiased();
  // eleOut->errD0Unbiased = element->trackIPEstimate_sigd0_unbiasedpvunbiased();
  // eleOut->z0Unbiased    = element->trackIPEstimate_z0_unbiasedpvunbiased();
  // eleOut->errZ0Unbiased = element->trackIPEstimate_sigz0_unbiasedpvunbiased();

  // // Corrected topo iso is available in the susy d3pd, apparently calculated using the cluster E.
  // // However, the CaloIsoCorrection header says to use the energy after scaling/smearing...
  // // So, which should we use?
  // // For now, I will just use the cluster E, which I think people are using anyway

  // // Corrected etcone has Pt and ED corrections
  // eleOut->etcone30Corr  = CaloIsoCorrection::GetPtEDCorrectedIsolation(element->Etcone40(),
  //                                                                      element->Etcone40_ED_corrected(),
  //                                                                      element->cl_E(), element->etas2(),
  //                                                                      element->etap(), element->cl_eta(), 0.3,
  //                                                                      m_isMC, element->Etcone30())/GeV;

  // // Corrected topoEtcone has Pt and ED corrections.  Use D3PD branch for now
  // //float topo            = CaloIsoCorrection::GetPtEDCorrectedTopoIsolation(element->ED_median(), element->cl_E(),
  // //                                                                         element->etas2(), element->etap(),
  // //                                                                         element->cl_eta(), 0.3, m_isMC,
  // //                                                                         element->topoEtcone30())/GeV;
  // eleOut->topoEtcone30Corr      = element->topoEtcone30_corrected()/GeV;

  // // Trigger flags
  // eleOut->trigFlags     = m_eleTrigFlags[ lepIn->idx() ];

  // // Efficiency scale factor.  For now, use tightPP if electrons is tightPP, otherwise mediumPP
  // //int set               = eleOut->tightPP? 7 : 6;
  // //eleOut->effSF         = m_isMC? m_susyObj.GetSignalElecSF   ( element->cl_eta(), lepIn->lv()->Pt(), set ) : 1;
  // //eleOut->errEffSF      = m_isMC? m_susyObj.GetSignalElecSFUnc( element->cl_eta(), lepIn->lv()->Pt(), set ) : 1;

  // // Tight electron SFs can come directly from SUSYTools
  // // To get the SF uncert using GetSignalElecSF, we must get the shifted value and take the difference
  // float nomPt = lepIn->lv()->Pt();
  // float sfPt = nomPt >= 7.*GeV ? nomPt : 7.*GeV;
  // if(eleOut->tightPP){
  //   eleOut->effSF       = // m_isMC? // DG not implemented yet
  //                         // m_susyObj.GetSignalElecSF(element->cl_eta(), sfPt, true, true, false) :
  //       1;
  //   eleOut->errEffSF    = //m_isMC? // DG not implemented yet
  //                         //m_susyObj.GetSignalElecSF(element->cl_eta(), sfPt, true, true, false,
  //                         //                          200841, SystErr::EEFFUP) - eleOut->effSF : 
  //       0;
  // }

  // // For the medium SF, need to use our own function
  // else{
  //   float sf = 1, uncert = 0;
  //   bool recoSF(true), idSF(true), triggerSF(false);
  //   int runNumber=200841; // DG why this dummy value? (copied from MultiLep/ElectronTools.h)
  //   if (m_isMC) get_electron_eff_sf(sf, uncert, element->cl_eta(), sfPt,
  //                                   recoSF, idSF, triggerSF, m_isAF2,
  //                                   m_susyObj.GetElectron_recoSF_Class(), m_eleMediumSFTool, 0,
  //                                   runNumber);
  //   eleOut->effSF       = sf;
  //   eleOut->errEffSF    = uncert;
  // }

  // // Do we need this??
  // eleOut->idx           = lepIn->idx();
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
void SusyNtMaker::fillMuonVars(const LeptonInfo* lepIn)
{
#warning fillMuonVars not implemented
  // if(m_dbg>=5) cout << "fillMuonVars" << endl;
  // m_susyNt.muo()->push_back( Susy::Muon() );
  // Susy::Muon* muOut = & m_susyNt.muo()->back();
  // const D3PDReader::MuonD3PDObjectElement* element = lepIn->getMuonElement();

  // // LorentzVector
  // const TLorentzVector* lv = lepIn->lv();
  // float pt  = lv->Pt() / GeV;
  // float eta = lv->Eta();
  // float phi = lv->Phi();
  // float m   = lv->M() / GeV;
  // muOut->SetPtEtaPhiM(pt, eta, phi, m);
  // muOut->pt  = pt;
  // muOut->eta = eta;
  // muOut->phi = phi;
  // muOut->m   = m;

  // muOut->q              = element->charge();
  // muOut->ptcone20       = element->ptcone20()/GeV;
  // muOut->ptcone30       = element->ptcone30()/GeV;
  // muOut->etcone20       = element->etcone20()/GeV;
  // muOut->etcone30       = element->etcone30()/GeV;

  // muOut->ptcone30ElStyle= m_d3pdTag>=D3PD_p1181? element->ptcone30_trkelstyle()/GeV : 0;

  // // ID coordinates
  // muOut->idTrackPt      = element->id_qoverp_exPV() != 0.?
  //                         fabs(sin(element->id_theta_exPV())/element->id_qoverp_exPV())/GeV : 0.;
  // muOut->idTrackEta     = -1.*log(tan(element->id_theta_exPV()/2.));
  // muOut->idTrackPhi     = element->id_phi_exPV();
  // muOut->idTrackQ       = element->id_qoverp_exPV() < 0 ? -1 : 1;

  // // MS coordinates
  // muOut->msTrackPt      = element->ms_qoverp() != 0.?
  //                         fabs(sin(element->ms_theta())/element->ms_qoverp())/GeV : 0.;
  // muOut->msTrackEta     = -1.*log(tan(element->ms_theta()/2.));
  // muOut->msTrackPhi     = element->ms_phi();
  // muOut->msTrackQ       = element->ms_qoverp() < 0 ? -1 : 1;

  // muOut->d0             = element->d0_exPV();
  // muOut->errD0          = sqrt(element->cov_d0_exPV());
  // muOut->z0             = element->z0_exPV();
  // muOut->errZ0          = sqrt(element->cov_z0_exPV());

  // muOut->d0Unbiased     = element->trackIPEstimate_d0_unbiasedpvunbiased();
  // muOut->errD0Unbiased  = element->trackIPEstimate_sigd0_unbiasedpvunbiased();
  // muOut->z0Unbiased     = element->trackIPEstimate_z0_unbiasedpvunbiased();
  // muOut->errZ0Unbiased  = element->trackIPEstimate_sigz0_unbiasedpvunbiased();

  // muOut->isCombined     = element->isCombinedMuon();

  // // theta_exPV. Not sure if necessary.
  // muOut->thetaPV        = element->theta_exPV();

  // // Cleaning flags
  // muOut->isBadMuon      = m_susyObj.IsBadMuon(element->qoverp_exPV(), element->cov_qoverp_exPV(), 0.2);
  // muOut->isCosmic       = m_susyObj.IsCosmicMuon(element->z0_exPV(), element->d0_exPV(), 1., 0.2);

  // // Truth flags
  // if(m_isMC){
  //   muOut->mcType       = element->type();
  //   muOut->mcOrigin     = element->origin();
  //   // If type and origin are zero, try matching to the muons in the truthMuon block
  //   // This might not actually do anything
  //   if(element->type()==0 && element->origin()==0 && element->truth_barcode()!=0){
  //     const D3PDReader::TruthMuonD3PDObjectElement* trueMuon = getMuonTruth(d3pdMuons(), lepIn->idx(), &m_event.muonTruth);
  //     muOut->mcType     = trueMuon? trueMuon->type()   : 0;
  //     muOut->mcOrigin   = trueMuon? trueMuon->origin() : 0;
  //   }
  //   muOut->matched2TruthLepton  = m_recoTruthMatch.Matched2TruthLepton(*lv);
  //   muOut->truthType            = m_recoTruthMatch.fakeType(*lv, muOut->mcOrigin, muOut->mcType);
  // }

  // muOut->trigFlags      = m_muoTrigFlags[ lepIn->idx() ];

  // // Syntax of the GetSignalMuonSF has changed.  Now, the same method is used to get the nominal and shifted value.
  // // So, in order to store the uncert, I take the shifted value minus the nominal, and save that.
  // muOut->effSF          = m_isMC? m_susyObj.GetSignalMuonSF(lepIn->idx()) : 1;
  // muOut->errEffSF       = m_isMC? m_susyObj.GetSignalMuonSF(lepIn->idx(), SystErr::MEFFUP) - muOut->effSF : 0;

  // // Do we need this??
  // muOut->idx            = lepIn->idx();
}

/*--------------------------------------------------------------------------------*/
// Fill jet variables
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::fillJetVars()
{
#warning fillJetVars not implemented
  // if(m_dbg>=5) cout << "fillJetVars" << endl;
  // // Calculate random run/lb number, necessary for BCH cleaning flag
  // if(m_isMC) calcRandomRunLB();
  // // Loop over selected jets and fill output tree
  // for(uint iJet=0; iJet<m_preJets.size(); iJet++){
  //   int jetIndex = m_preJets[iJet];
  //   fillJetVar(jetIndex);
  // }
}
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::fillJetVar(int jetIdx)
{
  // const D3PDReader::JetD3PDObjectElement *element = &m_event.jet_AntiKt4LCTopo[jetIdx];
  // m_susyNt.jet()->push_back( Susy::Jet() );
  // Susy::Jet* jetOut = & m_susyNt.jet()->back();

  // const TLorentzVector* lv = & m_susyObj.GetJetTLV(jetIdx);
  // float pt  = lv->Pt() / GeV;
  // float eta = lv->Eta();
  // float phi = lv->Phi();
  // float m   = lv->M() / GeV;
  // jetOut->SetPtEtaPhiM(pt, eta, phi, m);
  // jetOut->pt  = pt;
  // jetOut->eta = eta;
  // jetOut->phi = phi;
  // jetOut->m   = m;

  // jetOut->detEta        = element->constscale_eta();
  // jetOut->emfrac        = element->emfrac();
  // jetOut->idx           = jetIdx;
  // jetOut->jvf           = element->jvtxf();
  // jetOut->truthLabel    = m_isMC? element->flavor_truth_label() : 0;

  // // truth jet matching
  // jetOut->matchTruth    = m_isMC? matchTruthJet(jetIdx) : false;

  // // btag weights
  // jetOut->sv0           = element->flavor_weight_SV0();
  // jetOut->combNN        = element->flavor_weight_JetFitterCOMBNN();
  // jetOut->mv1           = element->flavor_weight_MV1();
  // jetOut->jfit_mass     = element->flavor_component_jfit_mass();
  // jetOut->sv0p_mass     = element->flavor_component_sv0p_mass();
  // jetOut->svp_mass      = element->flavor_component_svp_mass();

  // jetOut->bch_corr_jet  = element->BCH_CORR_JET();
  // jetOut->bch_corr_cell = element->BCH_CORR_CELL();
  // jetOut->isBadVeryLoose= JetID::isBadJet(JetID::VeryLooseBad,
  //                                         element->emfrac(),
  //                                         element->hecf(),
  //                                         element->LArQuality(),
  //                                         element->HECQuality(),
  //                                         element->Timing(),
  //                                         element->sumPtTrk_pv0_500MeV()/GeV,
  //                                         element->emscale_eta(), pt,
  //                                         element->fracSamplingMax(),
  //                                         element->NegativeE(),
  //                                         element->AverageLArQF());
  // jetOut->isHotTile     = m_susyObj.isHotTile(m_event.eventinfo.RunNumber(), element->fracSamplingMax(),
  //                                             element->SamplingMax(), eta, phi);

  // // BCH cleaning flags
  // uint bchRun = m_isMC? m_mcRun : m_event.eventinfo.RunNumber();
  // uint bchLB = m_isMC? m_mcLB : m_event.eventinfo.lbn();
  // #define BCH_ARGS bchRun, bchLB, jetOut->detEta, jetOut->phi, jetOut->bch_corr_cell, jetOut->emfrac, jetOut->pt*1000.
  // jetOut->isBadMediumBCH = !m_susyObj.passBCHCleaningMedium(BCH_ARGS, 0);
  // jetOut->isBadMediumBCH_up = !m_susyObj.passBCHCleaningMedium(BCH_ARGS, 1);
  // jetOut->isBadMediumBCH_dn = !m_susyObj.passBCHCleaningMedium(BCH_ARGS, -1);
  // jetOut->isBadTightBCH = !m_susyObj.passBCHCleaningTight(BCH_ARGS);
  // #undef BCH_ARGS

  // // Save the met weights for the jets
  // // by checking status word similar to
  // // what is done in met utility
  // const D3PDReader::MissingETCompositionD3PDObjectElement &jetMetEgamma10NoTau = m_event.jet_AntiKt4LCTopo_MET_Egamma10NoTau[jetIdx];
  // // 0th element is what we care about
  // int sWord = jetMetEgamma10NoTau.statusWord().at(0);
  // bool passSWord = (MissingETTags::DEFAULT == sWord);       // Note assuming default met..
  // jetOut->met_wpx = 0; passSWord ? jetMetEgamma10NoTau.wpx().at(0) : 0;
  // jetOut->met_wpy = 0; passSWord ? jetMetEgamma10NoTau.wpy().at(0) : 0;
}

/*--------------------------------------------------------------------------------*/
// Fill Photon variables
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::fillPhotonVars()
{
#warning fillPhotonVars not implemented
  // if(m_dbg>=5) cout << "fillPhotonVars" << endl;

  // // Loop over photons
  // for(uint iPh=0; iPh<m_sigPhotons.size(); iPh++){
  //   int phIndex = m_sigPhotons[iPh];

  //   fillPhotonVar(phIndex);
  // }
}
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::fillPhotonVar(int phIdx)
{
  // if(m_dbg>=5) cout << "fillPhotonVar" << endl;
  // m_susyNt.pho()->push_back( Susy::Photon() );
  // Susy::Photon* phoOut = & m_susyNt.pho()->back();
  // const D3PDReader::PhotonD3PDObjectElement* element = & m_event.ph[phIdx];


  // // Set TLV
  // const TLorentzVector* phTLV = & m_susyObj.GetPhotonTLV(phIdx);
  // float pt  = phTLV->Pt() / GeV;
  // float E   = phTLV->E()  / GeV;
  // float eta = phTLV->Eta();
  // float phi = phTLV->Phi();

  // phoOut->SetPtEtaPhiE(pt, eta, phi, E);
  // phoOut->pt  = pt;
  // phoOut->eta = eta;
  // phoOut->phi = phi;
  // phoOut->m   = 0.;

  // // Save conversion info
  // phoOut->isConv = element->isConv();

  // // Miscellaneous
  // phoOut->idx    = phIdx;
}

/*--------------------------------------------------------------------------------*/
// Fill Tau variables
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::fillTauVars()
{
#warning fillTauVars not implemented
  // if(m_dbg>=5) cout << "fillTauVars" << endl;

  // // Loop over selected taus
  // vector<int>& saveTaus = m_saveContTaus? m_contTaus : m_preTaus;
  // for(uint iTau=0; iTau < saveTaus.size(); iTau++){
  //   int tauIdx = saveTaus[iTau];

  //   fillTauVar(tauIdx);
  // }
}
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::fillTauVar(int tauIdx)
{
  // if(m_dbg>=5) cout << "fillTauVar" << endl;
  // m_susyNt.tau()->push_back( Susy::Tau() );
  // Susy::Tau* tauOut = & m_susyNt.tau()->back();
  // const D3PDReader::TauD3PDObjectElement* element = & m_event.tau[tauIdx];

  // // Set TLV
  // //const TLorentzVector* tauLV = & m_tauLVs.at(tauIdx);
  // const TLorentzVector* tauLV = & m_susyObj.GetTauTLV(tauIdx);
  // float pt  = tauLV->Pt() / GeV;
  // float eta = tauLV->Eta();
  // float phi = tauLV->Phi();
  // float m   = tauLV->M() / GeV;

  // tauOut->SetPtEtaPhiM(pt, eta, phi, m);
  // tauOut->pt    = pt;
  // tauOut->eta   = eta;
  // tauOut->phi   = phi;
  // tauOut->m     = m;

  // tauOut->q                     = element->charge();
  // tauOut->author                = element->author();
  // tauOut->nTrack                = element->numTrack();
  // tauOut->eleBDT                = element->BDTEleScore();
  // tauOut->jetBDT                = element->BDTJetScore();

  // tauOut->jetBDTSigLoose        = element->JetBDTSigLoose();
  // tauOut->jetBDTSigMedium       = element->JetBDTSigMedium();
  // tauOut->jetBDTSigTight        = element->JetBDTSigTight();

  // // New ele BDT corrections
  // //tauOut->eleBDTLoose           = element->EleBDTLoose();
  // //tauOut->eleBDTMedium          = element->EleBDTMedium();
  // //tauOut->eleBDTTight           = element->EleBDTTight();
  // tauOut->eleBDTLoose           = m_susyObj.GetCorrectedEleBDTFlag(SUSYTau::TauLoose, element->EleBDTLoose(),
  //                                                                  element->BDTEleScore(), element->numTrack(),
  //                                                                  tauLV->Pt(), element->leadTrack_eta());
  // tauOut->eleBDTMedium          = m_susyObj.GetCorrectedEleBDTFlag(SUSYTau::TauMedium, element->EleBDTMedium(),
  //                                                                  element->BDTEleScore(), element->numTrack(),
  //                                                                  tauLV->Pt(), element->leadTrack_eta());
  // tauOut->eleBDTTight           = m_susyObj.GetCorrectedEleBDTFlag(SUSYTau::TauTight, element->EleBDTTight(),
  //                                                                  element->BDTEleScore(), element->numTrack(),
  //                                                                  tauLV->Pt(), element->leadTrack_eta());

  // tauOut->muonVeto              = element->muonVeto();

  // tauOut->trueTau               = m_isMC? element->trueTauAssoc_matched() : false;

  // tauOut->matched2TruthLepton   = m_isMC? m_recoTruthMatch.Matched2TruthLepton(*tauLV, true) : false;
  // tauOut->detailedTruthType     = m_isMC? m_recoTruthMatch.TauDetailedFakeType(*tauLV) : -1;
  // tauOut->truthType             = m_isMC? m_recoTruthMatch.TauFakeType(tauOut->detailedTruthType) : -1;

  // // ID efficiency scale factors
  // if(m_isMC){
  //   #define TAU_ARGS TauCorrUncert::BDTLOOSE, tauLV->Eta(), element->numTrack()
  //   //TauCorrections* tauSF       = m_susyObj.GetTauCorrectionsProvider();
  //   TauCorrUncert::TauSF* tauSF = m_susyObj.GetSFTool();
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

  // tauOut->trigFlags             = m_tauTrigFlags[tauIdx];

  // tauOut->idx   = tauIdx;
}

/*--------------------------------------------------------------------------------*/
// Fill MET variables
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::fillMetVars(SusyNtSys sys)
{
#warning fillMetVars not implemented
  // if(m_dbg>=5) cout << "fillMetVars: sys " << sys << endl;

  // // Just fill the lv for now
  // double Et  = m_met.Et()/GeV;
  // double phi = m_met.Phi();

  // //double px = m_met.Px()/GeV;
  // //double py = m_met.Py()/GeV;
  // //double pz = m_met.Pz()/GeV;
  // //double E  = m_met.E()/GeV;

  // // Need to get the metUtility in order to
  // // get all the sumet terms.  In the future,
  // // we could use the metUtility to get all the
  // // comonents instead of the SUSYTools method
  // // computeMetComponent, but that is up to Steve,
  // // Lord of the Ntuples.
  // METUtility* metUtil = m_susyObj.GetMETUtility();

  // m_susyNt.met()->push_back( Susy::Met() );
  // Susy::Met* metOut = & m_susyNt.met()->back();
  // metOut->Et    = Et;
  // metOut->phi   = phi;
  // metOut->sys   = sys;
  // metOut->sumet = metUtil->getMissingET(METUtil::RefFinal, METUtil::None).sumet()/GeV;

  // // MET comp terms
  // // Need to save these for the MET systematics as well.
  // // Use the sys enum to determine which argument to pass to SUSYTools
  // METUtil::Systematics metSys = METUtil::None;

  // // I guess these are the only ones we need to specify, the ones specified in SUSYTools...
  // // All the rest should be automatic (I think), e.g. JES
  // if(sys == NtSys_SCALEST_UP) metSys = METUtil::ScaleSoftTermsUp;
  // else if(sys == NtSys_SCALEST_DN) metSys = METUtil::ScaleSoftTermsDown;
  // else if(sys == NtSys_RESOST) metSys = METUtil::ResoSoftTermsUp;

  // // Save the MET terms
  // TVector2 refEleV   = m_susyObj.computeMETComponent(METUtil::RefEle, metSys);
  // TVector2 refMuoV   = m_susyObj.computeMETComponent(METUtil::MuonTotal, metSys);
  // TVector2 refJetV   = m_susyObj.computeMETComponent(METUtil::RefJet, metSys);
  // TVector2 refGammaV = m_susyObj.computeMETComponent(METUtil::RefGamma, metSys);
  // //TVector2 softJetV  = m_susyObj.computeMETComponent(METUtil::SoftJets, metSys);
  // //TVector2 refCellV  = m_susyObj.computeMETComponent(METUtil::CellOutEflow, metSys);
  // TVector2 softTermV = m_susyObj.computeMETComponent(METUtil::SoftTerms, metSys);
  // //float sumet = m_susyObj._metUtility->getMissingET(METUtil::SoftTerms).sumet();

  // metOut->refEle     = refEleV.Mod()/GeV;
  // metOut->refEle_etx = refEleV.Px()/GeV;
  // metOut->refEle_ety = refEleV.Py()/GeV;
  // metOut->refEle_sumet = metUtil->getMissingET(METUtil::RefEle, metSys).sumet()/GeV;

  // metOut->refMuo     = refMuoV.Mod()/GeV;
  // metOut->refMuo_etx = refMuoV.Px()/GeV;
  // metOut->refMuo_ety = refMuoV.Py()/GeV;
  // metOut->refMuo_sumet = metUtil->getMissingET(METUtil::MuonTotal, metSys).sumet()/GeV;

  // metOut->refJet     = refJetV.Mod()/GeV;
  // metOut->refJet_etx = refJetV.Px()/GeV;
  // metOut->refJet_ety = refJetV.Py()/GeV;
  // metOut->refJet_sumet = metUtil->getMissingET(METUtil::RefJet, metSys).sumet()/GeV;

  // metOut->refGamma     = refGammaV.Mod()/GeV;
  // metOut->refGamma_etx = refGammaV.Px()/GeV;
  // metOut->refGamma_ety = refGammaV.Py()/GeV;
  // metOut->refGamma_sumet = metUtil->getMissingET(METUtil::RefGamma, metSys).sumet()/GeV;

  // //metOut->softJet     = softJetV.Mod()/GeV;
  // //metOut->softJet_etx = softJetV.Px()/GeV;
  // //metOut->softJet_ety = softJetV.Py()/GeV;

  // //metOut->refCell     = refCellV.Mod()/GeV;
  // //metOut->refCell_etx = refCellV.Px()/GeV;
  // //metOut->refCell_ety = refCellV.Py()/GeV;

  // metOut->softTerm     = softTermV.Mod()/GeV;
  // metOut->softTerm_etx = softTermV.Px()/GeV;
  // metOut->softTerm_ety = softTermV.Py()/GeV;
  // metOut->softTerm_sumet = metUtil->getMissingET(METUtil::SoftTerms, metSys).sumet()/GeV;

  // //metOut->refEle        = m_susyObj.computeMETComponent(METUtil::RefEle, metSys).Mod()/GeV;
  // //metOut->refMuo        = m_susyObj.computeMETComponent(METUtil::MuonTotal, metSys).Mod()/GeV;
  // //metOut->refJet        = m_susyObj.computeMETComponent(METUtil::RefJet, metSys).Mod()/GeV;
  // //metOut->refGamma      = m_susyObj.computeMETComponent(METUtil::RefGamma, metSys).Mod()/GeV;
  // //metOut->softJet       = m_susyObj.computeMETComponent(METUtil::SoftJets, metSys).Mod()/GeV;
  // //metOut->refCell       = m_susyObj.computeMETComponent(METUtil::CellOutEflow, metSys).Mod()/GeV;
}

/*--------------------------------------------------------------------------------*/
// Fill Truth Particle variables
/*--------------------------------------------------------------------------------*/
bool isMcAtNloTtbar(const int &channel) { return channel==105200; }
/*--------------------------------------------------------------------------------*/
void SusyNtMaker::fillTruthParticleVars()
{
#warning fillTruthParticleVars not implemented
  // if(m_dbg>=5) cout << "fillTruthParticleVars" << endl;

  // // Retrieve indicies
  // m_truParticles        = m_recoTruthMatch.LepFromHS_McIdx();
  // vector<int> truthTaus = m_recoTruthMatch.TauFromHS_McIdx();
  // m_truParticles.insert( m_truParticles.end(), truthTaus.begin(), truthTaus.end() );
  // if(m_isMC && isMcAtNloTtbar(m_event.eventinfo.mc_channel_number())){
  //   vector<int> ttbarPart(WhTruthExtractor::ttbarMcAtNloParticles(m_event.mc.pdgId(),
  //                                                                 m_event.mc.child_index()));
  //   m_truParticles.insert(m_truParticles.end(), ttbarPart.begin(), ttbarPart.end());
  // }

  // // Loop over selected truth particles
  // for(uint iTruPar=0; iTruPar<m_truParticles.size(); iTruPar++){
  //   int truParIdx = m_truParticles[iTruPar];

  //   m_susyNt.tpr()->push_back( Susy::TruthParticle() );
  //   Susy::TruthParticle* tprOut         = & m_susyNt.tpr()->back();

  //   // Set TLV
  //   float pt  = m_event.mc.pt() ->at(truParIdx) / GeV;
  //   float eta = m_event.mc.eta()->at(truParIdx);
  //   float phi = m_event.mc.phi()->at(truParIdx);
  //   float m   = m_event.mc.m()  ->at(truParIdx) / GeV;

  //   tprOut->SetPtEtaPhiM(pt, eta, phi, m);
  //   tprOut->pt          = pt;
  //   tprOut->eta         = eta;
  //   tprOut->phi         = phi;
  //   tprOut->m           = m;

  //   tprOut->charge      = m_event.mc.charge()->at(truParIdx);
  //   tprOut->pdgId       = m_event.mc.pdgId() ->at(truParIdx);
  //   tprOut->status      = m_event.mc.status()->at(truParIdx);
  //   tprOut->motherPdgId = smc::determineParentPdg(m_event.mc.pdgId(),
  //                                                 m_event.mc.parent_index(),
  //                                                 truParIdx);
  // }
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
  // Loop over the systematics: start at 1, nominal saved
  for(int i = 1; i < NtSys_N; i++){
      SusyNtSys sys = static_cast<SusyNtSys>(i);
    if(m_dbg>=5) cout << "Doing sys " << SusyNtSystNames[sys] << endl;    
    clearObjects();
    selectObjects(sys);
    buildMet(sys);
    assignEventCleaningFlags();
    assignObjectCleaningFlags();
    if     (isElecSys(sys)) saveElectronSF(sys); // Lepton Specific sys
    else if(isMuonSys(sys)) saveMuonSF(sys);
    else if(isJetSys(sys))  saveJetSF(sys);
    else if(isTauSys(sys))  saveTauSF(sys);
    fillMetVars(sys);
    m_susyNt.evt()->cutFlags[sys] = m_cutFlags;
  }
}

/*--------------------------------------------------------------------------------*/
void SusyNtMaker::saveElectronSF(SusyNtSys sys)
{
#warning saveElectronSF not implemented
  // // Loop over preselected leptons and fill the systematic shifts
  // for(uint iLep=0; iLep < m_preLeptons.size(); iLep++){
  //   const LeptonInfo* lep = & m_preLeptons[iLep];
  //   if(!lep->isElectron()) continue;

  //   // Systematic shifted energy
  //   float E_sys = lep->lv()->E() / GeV;

  //   // Try to find this electron in the list of SusyNt electrons
  //   Susy::Electron* eleOut = 0;
  //   for(uint iEl=0; iEl<m_susyNt.ele()->size(); iEl++){
  //     Susy::Electron* ele = & m_susyNt.ele()->at(iEl);
  //     if(ele->idx == lep->idx()){
  //       eleOut = ele;
  //       break;
  //     }
  //   }

  //   // If electron not found, then we need to add it
  //   if(eleOut == 0){
  //     addMissingElectron(lep, sys);
  //     eleOut = & m_susyNt.ele()->back();
  //   }

  //   // Calculate systematic scale factor
  //   float sf = E_sys / eleOut->E();
  //   if(sys == NtSys_EES_Z_UP)        eleOut->ees_z_up = sf;
  //   else if(sys == NtSys_EES_Z_DN)   eleOut->ees_z_dn = sf;
  //   else if(sys == NtSys_EES_MAT_UP) eleOut->ees_mat_up = sf;
  //   else if(sys == NtSys_EES_MAT_DN) eleOut->ees_mat_dn = sf;
  //   else if(sys == NtSys_EES_PS_UP)  eleOut->ees_ps_up = sf;
  //   else if(sys == NtSys_EES_PS_DN)  eleOut->ees_ps_dn = sf;
  //   else if(sys == NtSys_EES_LOW_UP) eleOut->ees_low_up = sf;
  //   else if(sys == NtSys_EES_LOW_DN) eleOut->ees_low_dn = sf;
  //   else if(sys == NtSys_EER_UP)     eleOut->eer_up = sf;
  //   else if(sys == NtSys_EER_DN)     eleOut->eer_dn = sf;
  // }
}

/*--------------------------------------------------------------------------------*/
void SusyNtMaker::saveMuonSF(SusyNtSys sys)
{
#warning saveMuonSF not implemented
  // // Loop over preselected leptons and fill the systematic shifts
  // for(uint iLep=0; iLep < m_preLeptons.size(); iLep++){
  //   const LeptonInfo* lep = & m_preLeptons[iLep];
  //   if(lep->isElectron()) continue;

  //   // Systematic shifted energy
  //   float E_sys = lep->lv()->E() / GeV;

  //   // Try to find this muon in the list of SusyNt muons
  //   Susy::Muon* muOut = 0;
  //   for(uint iMu=0; iMu<m_susyNt.muo()->size(); iMu++){
  //     Susy::Muon* mu = & m_susyNt.muo()->at(iMu);
  //     if(mu->idx == lep->idx()){
  //       muOut = mu;
  //       break;
  //     }
  //   }

  //   // If muon not found, then we need to add it
  //   if(muOut == 0){
  //     addMissingMuon(lep, sys);
  //     muOut = & m_susyNt.muo()->back();
  //   }

  //   // Calculate systematic scale factor
  //   float sf = E_sys / muOut->E();
  //   if(sys == NtSys_MS_UP)      muOut->ms_up = sf;
  //   else if(sys == NtSys_MS_DN) muOut->ms_dn = sf;
  //   else if(sys == NtSys_ID_UP) muOut->id_up = sf;
  //   else if(sys == NtSys_ID_DN) muOut->id_dn = sf;
  //  } // end loop over leptons
}

/*--------------------------------------------------------------------------------*/
void SusyNtMaker::saveJetSF(SusyNtSys sys)
{
#warning saveJetSF not implemented
  // // Loop over selected jets and fill the systematic shifts
  // for(uint iJet=0; iJet<m_preJets.size(); iJet++){
  //   uint jetIdx = m_preJets[iJet];

  //   // Systematic shifted energy
  //   float E_sys = m_susyObj.GetJetTLV(jetIdx).E() / GeV;

  //   // Try to find this jet in the list of SusyNt jets
  //   Susy::Jet* jetOut = 0;
  //   for(uint iJ=0; iJ<m_susyNt.jet()->size(); ++iJ){
  //     Susy::Jet* jet = & m_susyNt.jet()->at(iJ);
  //     if(jet->idx == jetIdx){
  //       jetOut = jet;
  //       break;
  //     }
  //   }

  //   // If jet not found, then we need to add it
  //   if(jetOut == 0){
  //     addMissingJet(jetIdx, sys);
  //     jetOut = & m_susyNt.jet()->back();
  //   }

  //   // Calculate systematic scale factor
  //   float sf = E_sys / jetOut->E();
  //   if(sys == NtSys_JES_UP)      jetOut->jes_up = sf;
  //   else if(sys == NtSys_JES_DN) jetOut->jes_dn = sf;
  //   else if(sys == NtSys_JER)    jetOut->jer = sf;
  // } // end loop over jets in pre-jets
}

/*--------------------------------------------------------------------------------*/
void SusyNtMaker::saveTauSF(SusyNtSys sys)
{
#warning saveTauSF not implemented
  // // Loop over preselected taus and fill systematic shifts
  // for(uint iTau=0; iTau<m_preTaus.size(); iTau++){
  //   uint tauIdx = m_preTaus[iTau];

  //   // Get the systematic shifted E, used to calculate a shift factor
  //   float E_sys = m_susyObj.GetTauTLV(tauIdx).E() / GeV;

  //   // Try to find this tau in the list of SusyNt taus
  //   Susy::Tau* tauOut = 0;
  //   for(uint iT=0; iT<m_susyNt.tau()->size(); iT++){
  //     Susy::Tau* tau = & m_susyNt.tau()->at(iT);
  //     if(tau->idx == tauIdx){
  //       tauOut = tau;
  //       break;
  //     }
  //   }
  //   // If tau not found, then it was not nominally pre-selected and must be added now
  //   if(tauOut == 0){
  //     addMissingTau(tauIdx, sys);
  //     tauOut = & m_susyNt.tau()->back();
  //   }

  //   // Calculate systematic scale factor
  //   float sf = E_sys / tauOut->E();
  //   if(sys == NtSys_TES_UP) tauOut->tes_up = sf;
  //   if(sys == NtSys_TES_DN) tauOut->tes_dn = sf;
  // }
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
  // m_susyObj.SetElecTLV(lep->idx(), element->eta(), element->phi(), element->cl_eta(), element->cl_phi(), element->cl_E(),
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
  // m_susyObj.SetMuonTLV(lep->idx(), element->pt(), element->eta(), element->phi(),
  //                      element->me_qoverp_exPV(), element->id_qoverp_exPV(), element->me_theta_exPV(),
  //                      element->id_theta_exPV(), element->charge(), element->isCombinedMuon(),
  //                      element->isSegmentTaggedMuon(), SystErr::NONE);
  // fillMuonVars(lep);
}

/*--------------------------------------------------------------------------------*/
void SusyNtMaker::addMissingJet(int index, SusyNtSys sys)
{
  // // Get the systematic shifted E, used to calculate a shift factor
  // //TLorentzVector tlv_sys = m_susyObj.GetJetTLV(index);
  // //float E_sys = m_susyObj.GetJetTLV(index).E();

  // // Reset the Nominal TLV
  // // NOTE: this overwrites the TLV in SUSYObjDef with the nominal variables,
  // // regardless of our current systematic.
  // const D3PDReader::JetD3PDObjectElement* jet = &m_event.jet_AntiKt4LCTopo[index];
  // m_susyObj.FillJet(index, jet->pt(), jet->eta(), jet->phi(), jet->E(),
  //                   jet->constscale_eta(), jet->constscale_phi(), jet->constscale_E(), jet->constscale_m(),
  //                   jet->ActiveAreaPx(), jet->ActiveAreaPy(), jet->ActiveAreaPz(), jet->ActiveAreaE(),
  //                   m_event.Eventshape.rhoKt4LC(),
  //                   m_event.eventinfo.averageIntPerXing(),
  //                   m_event.vxp.nTracks());
  // fillJetVar(index);
  // // Set SF This should only be done in saveJetSF
}

/*--------------------------------------------------------------------------------*/
void SusyNtMaker::addMissingTau(int index, SusyNtSys sys)
{
  // // This tau did not pass nominal cuts, and therefore
  // // needs to be added, but with the correct TLV.
  // // Get the systematic shifted E, used to calculate a shift factor
  // //TLorentzVector tlv_sys = m_susyObj.GetTauTLV(index);
  // //float E_sys = m_susyObj.GetTauTLV(index).E();
  // // Grab the d3pd variables
  // const D3PDReader::TauD3PDObjectElement* element = & m_event.tau[index];
  // // Reset the Nominal TLV
  // // NOTE: this overwrites the TLV in SUSYObjDef with the nominal variables,
  // // regardless of our current systematic.
  // m_susyObj.SetTauTLV(index, element->pt(), element->eta(), element->phi(), element->Et(), element->numTrack(),
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
bool SusyNtMaker::passEventlevelSelection()
{
    TH1F* h_procCutFlow = getProcCutFlow(m_susyFinalState);
    float w = 1.0; // DG-2014-08-16, not available yet in xAOD??? \todo m_isMC? d3pd.truth.event_weight() : 1;

    struct FillCutFlow { ///< local function object to fill the cutflow histograms
        TH1 *raw, *gen, *perProcess; ///< ptr to histos with counters
        int iCut; ///< index of the sequential cut (must match bin labels, see SusyNtMaker::makeCutFlow())
        bool passAll; ///< whether we've survived all cuts so far
        bool includeThisCut_; ///< whether this cut should be used when computing passAll
        vector< size_t > *counters;
        FillCutFlow(TH1 *r, TH1* g, TH1* p, vector< size_t > *cs) :
            raw(r), gen(g), perProcess(p), iCut(0), passAll(true), includeThisCut_(true), counters(cs) {}
        void operator()(bool thisEventDoesPassThisCut, float weight) {
            if(thisEventDoesPassThisCut && passAll) {
                if(raw       ) raw       ->Fill(iCut);
                if(gen       ) gen       ->Fill(iCut, weight);
                if(perProcess) perProcess->Fill(iCut, weight);
                counters->at(iCut) += 1;
            } else {
                if(includeThisCut_) passAll = false;
            }
            iCut++;
        }
        FillCutFlow& includeThisCut(bool v) { includeThisCut_ = v; return *this; }
    }
    fillCutFlow(h_rawCutFlow, h_genCutFlow, h_procCutFlow, &m_cutstageCounters);

    bool keep_all_events(!m_filter);
    bool pass_susyprop(!m_hasSusyProp);
    bool pass_grl(m_cutFlags & ECut_GRL), pass_lar(m_cutFlags & ECut_LarErr), pass_tile(m_cutFlags & ECut_TileErr);
    bool pass_ttc(m_cutFlags & ECut_TTC), pass_goodpv(m_cutFlags & ECut_GoodVtx), pass_tiletrip(m_cutFlags & ECut_TileTrip);
    bool pass_wwfix(true); //(!m_isMC || (m_susyObj.Sherpa_WW_veto())); // DG-2014-08-16 sherpa ww bugfix probably obsolete

    fillCutFlow(true, w); // initial bin
    fillCutFlow.includeThisCut(false); // susyProp just counts (for normalization), doesn't drop
    fillCutFlow(pass_susyprop, w);
    fillCutFlow.includeThisCut(true);
    fillCutFlow(pass_grl, w);
    fillCutFlow(pass_lar, w);
    fillCutFlow(pass_tile, w);
    fillCutFlow(pass_ttc, w);
    fillCutFlow(pass_goodpv, w);
    fillCutFlow(pass_wwfix, w);
    return (keep_all_events || fillCutFlow.passAll);
}
//----------------------------------------------------------
bool SusyNtMaker::passObjectlevelSelection()
{
    return true;
}
//----------------------------------------------------------
#undef GeV
