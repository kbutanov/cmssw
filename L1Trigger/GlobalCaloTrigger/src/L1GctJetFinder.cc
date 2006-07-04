#include "L1Trigger/GlobalCaloTrigger/interface/L1GctJetFinder.h"
 
#include "FWCore/Utilities/interface/Exception.h"  

#include <iostream>
using namespace std;

//DEFINE STATICS
// *** Note the following definition in terms of COL_OFFSET appears not to work ***
// ***          for some deep C++ reason that I don't understand - GPH          ***
// const unsigned int L1GctJetFinder::MAX_REGIONS_IN = L1GctJetFinderBase::COL_OFFSET*L1GctJetFinder::N_COLS;
// ***                        So - use the following instead                    ***
const unsigned int L1GctJetFinder::MAX_REGIONS_IN = (((L1GctJet::N_RGN_ETA)/2)+1)*L1GctJetFinder::N_COLS;

const int L1GctJetFinder::N_COLS = 4;
const unsigned int L1GctJetFinder::CENTRAL_COL0 = 1;

L1GctJetFinder::L1GctJetFinder(int id, vector<L1GctSourceCard*> sourceCards,
                               L1GctJetEtCalibrationLut* jetEtCalLut):
  L1GctJetFinderBase(id, sourceCards, jetEtCalLut)
{
  this->reset();
}

L1GctJetFinder::~L1GctJetFinder()
{
}

ostream& operator << (ostream& os, const L1GctJetFinder& algo)
{
  os << "===L1GctTdrJetFinder===" << endl;
  const L1GctJetFinderBase* temp = &algo;
  os << *temp;
  return os;
}

void L1GctJetFinder::fetchInput()
{
  fetchCentreStripsInput();
  fetchEdgeStripsInput();
}

void L1GctJetFinder::process() 
{
  findJets();
  sortJets();
  doEnergySums();
}

/// HERE IS THE JETFINDER CODE

void L1GctJetFinder::findJets() 
{
  UShort jetNum = 0; //holds the number of jets currently found
  UShort centreIndex = COL_OFFSET*this->centralCol0();
  for(UShort column = 0; column <2; ++column)  //Find jets in the central search region
  {
    //don't include row zero as it is not in the search region
    ++centreIndex;
    for (UShort row = 1; row < COL_OFFSET; ++row)  
    {
      if (m_inputRegions.at(centreIndex).et()>0) {
      }
      //Determine if we are at end of the HF or not (so need 3*2 window)
      bool hfBoundary = (row == COL_OFFSET-1);
      //Determine if we are at the end of the endcap HCAL regions, so need boundary condition tauveto
      bool heBoundary = (row == COL_OFFSET-5);

      //debug checks for improper input indices
      assert(centreIndex % COL_OFFSET != 0);  //Don't want the 4 regions from other half of detector
      assert(centreIndex >= COL_OFFSET);  //Don't want the shared column to left of jet finding area
      assert(centreIndex < (MAX_REGIONS_IN - COL_OFFSET)); //Don't want column to the right either
                        
      if(detectJet(centreIndex, hfBoundary))
      {
        assert(jetNum < MAX_JETS_OUT);
                
        m_outputJets.at(jetNum).setRawsum(calcJetEnergy(centreIndex, hfBoundary));
        m_outputJets.at(jetNum).setDetId(calcJetPosition(centreIndex));
        if(row < COL_OFFSET-4)  //if we are not in the HF, perform tauVeto analysis
        {
          m_outputJets.at(jetNum).setTauVeto(calcJetTauVeto(centreIndex,heBoundary));
        }
        else //can't be a tau jet because we are in the HF
        {
          m_outputJets.at(jetNum).setTauVeto(true);
        }
        ++jetNum;
      }
      ++centreIndex;
    }
  }
}

// Returns true if region index is the centre of a jet. Set boundary = true if at edge of HCAL.
bool L1GctJetFinder::detectJet(const UShort centreIndex, const bool boundary) const
{
  if(!boundary)  //Not at boundary, so use 3*3 window of regions to determine if a jet
  {
    // Get the energy of the central region
    ULong testEt = m_inputRegions.at(centreIndex).et();
        
    //Test if our region qualifies as a jet by comparing its energy with the energies of the
    //surrounding eight regions.  In the event of neighbouring regions with identical energy,
    //this will locate the jet in the lower-most (furthest away from eta=0), left-most (least phi) region.
    if(testEt >  m_inputRegions.at(centreIndex-1-COL_OFFSET).et() &&
       testEt >  m_inputRegions.at(centreIndex - COL_OFFSET).et() &&
       testEt >  m_inputRegions.at(centreIndex+1-COL_OFFSET).et() &&
           
       testEt >= m_inputRegions.at(centreIndex - 1).et() &&
       testEt >  m_inputRegions.at(centreIndex + 1).et() &&
           
       testEt >= m_inputRegions.at(centreIndex-1+COL_OFFSET).et() &&
       testEt >= m_inputRegions.at(centreIndex + COL_OFFSET).et() &&
       testEt >= m_inputRegions.at(centreIndex+1+COL_OFFSET).et())
    {
      return true;
    }
//USE THIS BLOCK INSTEAD IF YOU WANT OVERFLOW BIT FUNCTIONALITY        
//*** BUT IT WILL NEED MODIFICATION SINCE L1GctRegion IS OBSOLETE ***
/*    // Get the energy of the central region & OR the overflow bit to become the MSB
    ULong testEt = (m_inputRegions.at(centreIndex).et() | (m_inputRegions.at(centreIndex).getOverFlow() << L1GctRegion::ET_BITWIDTH));
        
    //Test if our region qualifies as a jet by comparing its energy with the energies of the
    //surrounding eight regions.  In the event of neighbouring regions with identical energy,
    //this will locate the jet in the lower-most (furthest away from eta=0), left-most (least phi) region.
    if(testEt >  (m_inputRegions.at(centreIndex-1-COL_OFFSET).et() | (m_inputRegions.at(centreIndex-1-COL_OFFSET).getOverFlow() << L1GctRegion::ET_BITWIDTH)) &&
       testEt >  (m_inputRegions.at(centreIndex - COL_OFFSET).et() | (m_inputRegions.at(centreIndex - COL_OFFSET).getOverFlow() << L1GctRegion::ET_BITWIDTH)) &&
       testEt >  (m_inputRegions.at(centreIndex+1-COL_OFFSET).et() | (m_inputRegions.at(centreIndex+1-COL_OFFSET).getOverFlow() << L1GctRegion::ET_BITWIDTH)) &&
           
       testEt >= (m_inputRegions.at(centreIndex - 1).et() | (m_inputRegions.at(centreIndex - 1).getOverFlow() << L1GctRegion::ET_BITWIDTH)) &&
       testEt >  (m_inputRegions.at(centreIndex + 1).et() | (m_inputRegions.at(centreIndex + 1).getOverFlow() << L1GctRegion::ET_BITWIDTH)) &&
           
       testEt >= (m_inputRegions.at(centreIndex-1+COL_OFFSET).et() | (m_inputRegions.at(centreIndex-1+COL_OFFSET).getOverFlow() << L1GctRegion::ET_BITWIDTH)) &&
       testEt >= (m_inputRegions.at(centreIndex + COL_OFFSET).et() | (m_inputRegions.at(centreIndex + COL_OFFSET).getOverFlow() << L1GctRegion::ET_BITWIDTH)) &&
       testEt >= (m_inputRegions.at(centreIndex+1+COL_OFFSET).et() | (m_inputRegions.at(centreIndex+1+COL_OFFSET).getOverFlow() << L1GctRegion::ET_BITWIDTH)))
    {
      return true;
    }
*/  //END OVERFLOW FUNCTIONALITY       
  }
  else    //...so only test surround 5 regions in our jet testing.
  {    
    // Get the energy of the central region
    // Don't need all the overflow bit adjustments as above, since we are in the HF here
    ULong testEt = m_inputRegions.at(centreIndex).et();        
        
    if(testEt >  m_inputRegions.at(centreIndex-1-COL_OFFSET).et() &&
       testEt >  m_inputRegions.at(centreIndex - COL_OFFSET).et() &&
       
       testEt >= m_inputRegions.at(centreIndex - 1).et() &&
           
       testEt >= m_inputRegions.at(centreIndex-1+COL_OFFSET).et() &&
       testEt >= m_inputRegions.at(centreIndex + COL_OFFSET).et())
    {
      return true;
    }
  }
  return false;           
}

// returns the energy sum of the nine regions centred (physically) about centreIndex
ULong L1GctJetFinder::calcJetEnergy(const UShort centreIndex, const bool boundary) const
{
  ULong energy = 0;
    
  if(!boundary)
  {
    for(int column = -1; column <= +1; ++column)
    {
      energy += m_inputRegions.at(centreIndex-1 + (column*COL_OFFSET)).et() +
                m_inputRegions.at( centreIndex  + (column*COL_OFFSET)).et() +
                m_inputRegions.at(centreIndex+1 + (column*COL_OFFSET)).et();
    }
  }
  else
  {
    for(int column = -1; column <= +1; ++column)
    {
      energy += m_inputRegions.at(centreIndex-1 + (column*COL_OFFSET)).et() +
                m_inputRegions.at( centreIndex  + (column*COL_OFFSET)).et();
    }
  }
  return energy;                                   
}

// returns the encoded (eta, phi) position of the centre region
L1CaloRegionDetId L1GctJetFinder::calcJetPosition(const UShort centreIndex) const
{
  return m_inputRegions.at(centreIndex).id();
}

// returns the combined tauveto of the nine regions centred (physically) about centreIndex. Set boundary = true if at edge of Endcap.
bool L1GctJetFinder::calcJetTauVeto(const UShort centreIndex, const bool boundary) const
{
  bool partial[3] = {false, false, false};
    
  if(!boundary)
  {
    for(int column = -1; column <= +1; ++column)
    {
      partial[column+1] = m_inputRegions.at(centreIndex-1 + (column*COL_OFFSET)).tauVeto() ||
                          m_inputRegions.at( centreIndex  + (column*COL_OFFSET)).tauVeto() ||
                          m_inputRegions.at(centreIndex+1 + (column*COL_OFFSET)).tauVeto();
    }
  }
  else
  {
    for(int column = -1; column <= +1; ++column)
    {
      partial[column+1] = m_inputRegions.at(centreIndex-1 + (column*COL_OFFSET)).tauVeto() ||
                          m_inputRegions.at( centreIndex  + (column*COL_OFFSET)).tauVeto();
    }
  }
  return partial[0] || partial[1] || partial[2];
}

