#ifndef _WIN32
#include "config.h"
#endif 

#include <ChiCEInterface.h>
#include <iostream>
#include "PinYinConversionHelper.h"

CPinYinConversionHelper::CPinYinConversionHelper(){

  // TODO: Need to figure out what this does - it was previously set to true in PopulateChildren
  m_bTraceNeeded = true;//reset trace bool

  //clears the process phrase flags
  for(int i(0); i<MAX_HZ_NUM; i++)
    m_bPhrasesProcessed[i]=0;  


  BuildDataBase();
  CEInitialise();

}
  
bool CPinYinConversionHelper::Convert(const std::string &strSource, SCENode ** pRoot, int * HZCount, int CMid) {

  SCENode * pStart;

  if(CEConvert (strSource.c_str(), &pStart, HZCount, CMid)){ 
    *pRoot= pStart;

    // Connect up the rest of the nodes to make a lattice
    SCENode *pHead(pStart);
    
    while(pHead) {
      SCENode *pNewChild(pHead->pChild);
      SCENode *pCurrent(pHead->pNext);
      
      while(pCurrent) {
	pCurrent->pChild = pNewChild;
	pCurrent = pCurrent->pNext;
      }
      
      pHead = pHead->pChild;
    }
   
    return 1;
  }
  else{
    *pRoot = 0;
    return 0;
  }
}


bool CPinYinConversionHelper::GetPhraseList(int HZIndex, SCENode ** psOutput, int CMid){
  SCENode * pStart;
  
  if(CEGetPhraseList(HZIndex, &pStart, CMid)){ 
   
    *psOutput= pStart;
   
    return 1;
  }
  else{
    *psOutput = 0;
    return 0;
  }
}

void CPinYinConversionHelper::BuildDataBase(){
  
  std::vector<std::vector<std::vector<std::vector<std::vector<int> > > > >IndexStack;
  std::vector<std::vector<std::vector<std::vector<int> > > >CandStack;
  std::vector<std::vector<std::vector<int> > > subIndexStack;
  std::vector<std::vector<int> > subCandStack;
  std::vector<int> cell;
  
  vContextData.clear();
  cell.push_back(0);
  subCandStack.push_back(cell);
  

    for(int i(0); i<MAX_HZ_NUM; i++){
      for(int j(0); j<MAX_CARE_CAND; j++){      
	for(int k(0); k<i+1; k++)	  
	  subIndexStack.push_back(subCandStack);
	CandStack.push_back(subIndexStack);
	subIndexStack.clear();
      }
      IndexStack.push_back(CandStack);
      CandStack.clear();
    }

    for(int k(0); k<MAX_CM_NUM; k++)
      vContextData.push_back(IndexStack);
}	


void CPinYinConversionHelper::ClearData(int CMid){
  
  std::vector<int>  cell;
  cell.push_back(0);

  for(int i(0); i<MAX_HZ_NUM; i++){
      for(int j(0); j<MAX_CARE_CAND/*(m_pRoot[i]->pChild->IsHeadAndCandNum)*/; j++){      
	for(int k(0); k<i+1; k++){
	  vContextData[CMid][i][j][k].clear();
	  vContextData[CMid][i][j][k].push_back(cell);
	}
      }
  }

}


std::vector<std::vector<std::vector<std::vector<std::vector<int> > > > > * CPinYinConversionHelper::GetDP(int CMid){

  return &vContextData[CMid];

}



//THIS FUNCTION IS CALLED WHEN A SET OF CHILDREN IS BEING POPULATED
//AND BEFORE CALCULATING EACH NODE'S SCORE. THE POSITION IN THE
//SENTENCE IS GIVEN TO THE PY HELPER->LIBRARY AND A LIST OF PHRASES
//CORRESPONDING TO THE CHARACTER IS RETURNED. THESE PHRASES ARE
//PROCESSED INTO THE CONTEXT DATA IN PY HELPER
    
void CPinYinConversionHelper::ProcessPhrase(HZIDX HZIndex){
  
  SCENode * pPhraseList;
  SCENode * pNode;
  
  //  bool stop=0;

  int iIdx(0);
  int i;
  int score[m_iHZCount-HZIndex];

  CANDIDX CandIndex[m_iHZCount-HZIndex]; //list to store candidates
					 //returned from HZlookup,
					 //used to allocate data
  std::string strtemp;

  std::vector<int>  cell;

  if(!(GetPhraseList(HZIndex, &pPhraseList, m_iCMID)))
    return;

  if(pPhraseList->AcCharCount>4)
    pNode = pPhraseList->pNext;
  else 
    pNode = pPhraseList;

  while((pNode)&&(iIdx<=MAX_CARE_PHRASE)){

    //this section needs research. What scores would be a good estimate.

    switch(pNode->AcCharCount){
    case 2:
      score[0] = 2;
      score[1] = 3;
      break;
    case 3:
      score[0] = 3;
      score[1] = 4;
      score[2] = 5;
      break;
    case 4:
      score[0] = 4;
      score[1] = 5;
      score[2] = 6;
      score[3] = 7;
      break;
    default:
      for(int j(0); j< m_iHZCount-HZIndex; j++)
	score[j] = 5+j;
      break;
	}
    
    
    for(i=0 ; (i<pNode->AcCharCount); i++){
      strtemp=pNode->pszConversion;

      //TESTING
      //std::cout<<"accharcount"<<pNode->AcCharCount<<std::endl;
      //std::cout<<"the cut string is"<<strtemp.substr(3*i,3)<<std::endl;
      //std::cout<<"list to look from
      //is"<<m_pRoot[HZIndex+i]->pChild->pszConversion<<std::endl;


      CandIndex[i] = HZLookup(HZIndex+i, strtemp.substr(3*i, 3));
      
      //TESTING
      //std::cout<<"the lookup is"<<CandIndex[i]<<std::endl;

      if(CandIndex[i]==-1)
	break;
      else{
	cell.push_back(score[i]);
	for(int j(0); j< i; j++)
	  cell.push_back(CandIndex[i-j-1]);
	
	//
	//say the phrase is XYZ(this) push back in each cell in the
	//order: score, Z, Y, X so as to match with vTrace in
	//calculatescore

	if(!(HZIndex + i> MAX_HZ_NUM -1))	
	  (*(GetDP(m_iCMID)))[HZIndex +i][CandIndex[i]][HZIndex].push_back(cell);
	cell.clear();
	
      }
    }

    pNode = pNode ->pNext;
    iIdx ++;
  }
  m_bPhrasesProcessed[HZIndex]=1;
}


    
CANDIDX CPinYinConversionHelper::HZLookup(HZIDX HZIndex, const std::string &strSource){


  // this was done before candindex was put into node member, change
  // if have time

  int iIdx(0);
  if((HZIndex > m_iHZCount-1)||strSource.size()!=3)
    return -1;

  // TODO: Reimplement -----

//   SCENode * pNode = m_pRoot[HZIndex]->pChild;

//   while(pNode&&(iIdx<=MAX_CARE_CAND)){
    
//     if(strSource== pNode->pszConversion)
//       return iIdx;
//     pNode = pNode->pNext;
//     iIdx++;
//   }

  // -----
  
  return -1;
}

//CALCULATES SCORE OF A CERTAIN CANDIDATE HZ CHARACTER NODE TO BE
//POPULATED, FROM THE CONTEXT DATA IN PY HELPER. FINDS VTRACE TO MATCH
//CONTEXT SEQUENCE STORED IN THE LAST LEVEL OF DATABASE

int CPinYinConversionHelper::CalculateScore(CDasherNode * pNode, CANDIDX CandIndex){   
  CDasherNode *pIterateDNode(pNode);
  SCENode *pTemp;

  HZIDX HZIndex;
  int score=0;

  bool addtick=1; //bool to signal add score



  //THIS SECTION IS TO FIND VTRACE, IN THE SAME WAY AS FINDING INPUT
  //PY STRING

  if(m_bTraceNeeded){
    vTrace.clear();
    
    while(pIterateDNode&&(pIterateDNode->m_pNodeManager->GetID() == 2)) {
      
      pTemp=static_cast<SCENode*>(pIterateDNode->m_pUserData);
      
      if(!pTemp)
	pIterateDNode=0;
      else{
	vTrace.push_back(pTemp->CandIndex);
	pIterateDNode= pIterateDNode->Parent();
      }
    }
    
    //THE FOLLOWING IS TESTING FOR VTRACE
    // if(vTrace.size()!=0){
    // std::cout<<"signal"<<std::endl;
    // for(std::vector<int>::iterator it(vTrace.begin());it!=vTrace.end();it++)
    //  std::cout<< *it <<std::endl;  
    //}

  }
  m_bTraceNeeded= false;



  
  if(CandIndex>=MAX_CARE_CAND)
    return 0;



  pTemp=static_cast<SCENode*>(pNode->m_pUserData);
  
  if(pTemp)
    HZIndex = pTemp->AcCharCount;
  else
    HZIndex = 0;


  //THE DATA IS CONSTRUCTED OF UNITS OF SINGLE CELLS STORING CONTEXT SEQUENCE
  //AND A CORRESPONDING SCORE 

  //LEVEL 1 : HZ INDEX : NUMBER OF CHARACTERS CONVERTED
  //LEVEL 2 : CAND INDEX : NUMBER OF CANDIDATES WITH EACH POSITION
  //LEVEL 3 : SUB HZ INDEX :(COULD BE REDUNDANT) CORRESPONDES TO
  //          INDEX OF CHARACTERS WHICH WERE CONSISTED IN PHRASES
  //LEVEL 4 : SUB CAND INDEX : WHICH CAND WAS IN THE PHRASE 
  //LEVLE 5 : CELL: STORING PHASES IN HZ INDEX AND ASSIGNED SCORE
  //          IN THE WAY: 1.SCORE 2.Z 3.Y 4.X FOR PHRASE XYZ
  //          PREVIOUSLY PROCESSED

  for(std::vector<std::vector<std::vector<int> > >::iterator itIndex((*(GetDP(m_iCMID)))[HZIndex][CandIndex].begin()); itIndex!=(*(GetDP(m_iCMID)))[HZIndex][CandIndex].end();itIndex++){

    for(std::vector<std::vector<int> >::iterator itCand(itIndex->begin()); itCand!=itIndex->end(); itCand++){

      //IF HAS LEFT CONTEXT INFORMATION, MATCH VTRACE WITH CELL
      //POSITION 1

      if((*itCand).size() !=1){
	for(unsigned int i(0); i<(*itCand).size()-1; i++)
	  if((*itCand)[i+1]!=vTrace[i]){
	    addtick = 0;
	    break;
	  }
	  if(addtick)
	    score+=(*itCand)[0];
	  addtick =1;
      }
      else
	score+=(*itCand)[0];
    }
  }
    
  return score;
} 