// DasherInterfaceBase.cpp
//
// Copyright (c) 2007 The Dasher Team
//
// This file is part of Dasher.
//
// Dasher is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Dasher is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Dasher; if not, write to the Free Software 
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include "../Common/Common.h"

#include "DasherInterfaceBase.h"

//#include "ActionButton.h"
#include "AlphabetManagerFactory.h"
#include "DasherViewSquare.h"
#include "ControlManager.h"
#include "DasherScreen.h"
#include "DasherView.h"
#include "DasherInput.h"
#include "DasherModel.h"
#include "EventHandler.h"
#include "Event.h"
#include "NodeCreationManager.h"
#include "UserLog.h"
#include "BasicLog.h"
#include "WrapperFactory.h"

// Input filters
#include "ClickFilter.h" 
#include "DefaultFilter.h"
#include "DasherButtons.h"
#include "DynamicFilter.h"
#include "EyetrackerFilter.h"
#include "OneDimensionalFilter.h"
#include "StylusFilter.h"
#include "TwoButtonDynamicFilter.h"

// STL headers
#include <iostream>
#include <memory>

// Legacy C library headers
namespace {
  #include "stdio.h"
}

// Declare our global file logging object
#include "../DasherCore/FileLogger.h"
#ifdef _DEBUG
const eLogLevel g_iLogLevel   = logDEBUG;
const int       g_iLogOptions = logTimeStamp | logDateStamp | logDeleteOldFile;    
#else
const eLogLevel g_iLogLevel   = logNORMAL;
const int       g_iLogOptions = logTimeStamp | logDateStamp;
#endif
CFileLogger* g_pLogger = NULL;

using namespace Dasher;
using namespace std;

// Track memory leaks on Windows to the line that new'd the memory
#ifdef _WIN32
#ifdef _DEBUG_MEMLEAKS
#define DEBUG_NEW new( _NORMAL_BLOCK, THIS_FILE, __LINE__ )
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
#endif

CDasherInterfaceBase::CDasherInterfaceBase() {

  // Ensure that pointers to 'owned' objects are set to NULL.
  m_Alphabet = NULL;
  m_pDasherModel = NULL;
  m_DasherScreen = NULL;
  m_pDasherView = NULL;
  m_pInput = NULL;
  m_AlphIO = NULL;
  m_ColourIO = NULL;
  m_pUserLog = NULL;
  m_pInputFilter = NULL;
  m_pNCManager = NULL;

  // Various state variables
  m_bRedrawScheduled = false;
  
  m_iCurrentState = ST_START;
  
  m_iLockCount = 0;
  m_bGlobalLock = false;

  // TODO: Are these actually needed?
  strCurrentContext = ". ";
  strTrainfileBuffer = "";

  // Create an event handler.
  m_pEventHandler = new CEventHandler(this);

  m_bLastChanged = true;

  // Global logging object we can use from anywhere
  g_pLogger = new CFileLogger("dasher.log",
                              g_iLogLevel,
                              g_iLogOptions);

}

void CDasherInterfaceBase::Realize() {

  // TODO: What exactly needs to have happened by the time we call Realize()?
  CreateSettingsStore();
  SetupUI();
  SetupPaths();

  std::vector<std::string> vAlphabetFiles;
  ScanAlphabetFiles(vAlphabetFiles);
  m_AlphIO = new CAlphIO(GetStringParameter(SP_SYSTEM_LOC), GetStringParameter(SP_USER_LOC), vAlphabetFiles);

  std::vector<std::string> vColourFiles;
  ScanColourFiles(vColourFiles);
  m_ColourIO = new CColourIO(GetStringParameter(SP_SYSTEM_LOC), GetStringParameter(SP_USER_LOC), vColourFiles);

  ChangeColours();
  ChangeAlphabet();

  // Create the user logging object if we are suppose to.  We wait
  // until now so we have the real value of the parameter and not
  // just the default.

  // TODO: Sort out log type selection

  int iUserLogLevel = GetLongParameter(LP_USER_LOG_LEVEL_MASK);

  if(iUserLogLevel == 10)
    m_pUserLog = new CBasicLog(m_pEventHandler, m_pSettingsStore);
  else if (iUserLogLevel > 0) 
    m_pUserLog = new CUserLog(m_pEventHandler, m_pSettingsStore, iUserLogLevel, m_Alphabet);  

  CreateFactories();
  CreateLocalFactories();

  CreateInput();
  CreateInputFilter();
  SetupActionButtons();

  // FIXME - need to rationalise this sort of thing.
  // InvalidateContext(true);
  ScheduleRedraw();
    
  // All the setup is done by now, so let the user log object know
  // that future parameter changes should be logged.
  if (m_pUserLog != NULL) 
    m_pUserLog->InitIsDone();

  // TODO: Make things work when model is created latet
  ChangeState(TR_MODEL_INIT);
}

CDasherInterfaceBase::~CDasherInterfaceBase() {
  DASHER_ASSERT(m_iCurrentState == ST_SHUTDOWN);

  delete m_pDasherModel;        // The order of some of these deletions matters
  delete m_Alphabet;
  delete m_pDasherView;
  delete m_ColourIO;
  delete m_AlphIO;
  delete m_pInputFilter;
  delete m_pNCManager;
  // Do NOT delete Edit box or Screen. This class did not create them.

  // When we destruct on shutdown, we'll output any detailed log file
  if (m_pUserLog != NULL)
  {
    m_pUserLog->OutputFile();
    delete m_pUserLog;
    m_pUserLog = NULL;
  }

  if (g_pLogger != NULL) {
    delete g_pLogger;
    g_pLogger = NULL;
  }

  // Must delete event handler after all CDasherComponent derived classes

  delete m_pEventHandler;
}

void CDasherInterfaceBase::PreSetNotify(int iParameter, const std::string &sNewValue) {

  // FIXME - make this a more general 'pre-set' event in the message
  // infrastructure

  switch(iParameter) {
  case SP_ALPHABET_ID: 
    // Cycle the alphabet history
    if(GetStringParameter(SP_ALPHABET_ID) != sNewValue) {
      if(GetStringParameter(SP_ALPHABET_1) != sNewValue) {
	if(GetStringParameter(SP_ALPHABET_2) != sNewValue) {
	  if(GetStringParameter(SP_ALPHABET_3) != sNewValue)
	    SetStringParameter(SP_ALPHABET_4, GetStringParameter(SP_ALPHABET_3));
	  
	  SetStringParameter(SP_ALPHABET_3, GetStringParameter(SP_ALPHABET_2));
	}
	
	SetStringParameter(SP_ALPHABET_2, GetStringParameter(SP_ALPHABET_1));
      }
      
      SetStringParameter(SP_ALPHABET_1, GetStringParameter(SP_ALPHABET_ID));
    }

    break;
  }
}

void CDasherInterfaceBase::InterfaceEventHandler(Dasher::CEvent *pEvent) {

  if(pEvent->m_iEventType == 1) {
    Dasher::CParameterNotificationEvent * pEvt(static_cast < Dasher::CParameterNotificationEvent * >(pEvent));

    switch (pEvt->m_iParameter) {

    case BP_OUTLINE_MODE:
      ScheduleRedraw();
      break;
    case BP_DRAW_MOUSE:
      ScheduleRedraw();
      break;
    case BP_CONTROL_MODE:
      ScheduleRedraw();
      break;
    case BP_DRAW_MOUSE_LINE:
      ScheduleRedraw();
      break;
    case LP_ORIENTATION:
      if(GetLongParameter(LP_ORIENTATION) == Dasher::Opts::AlphabetDefault)
	// TODO: See comment in DasherModel.cpp about prefered values
	SetLongParameter(LP_REAL_ORIENTATION, m_Alphabet->GetOrientation());
      else
	SetLongParameter(LP_REAL_ORIENTATION, GetLongParameter(LP_ORIENTATION));
      ScheduleRedraw();
      break;
    case SP_ALPHABET_ID:
      ChangeAlphabet();
      ScheduleRedraw();
      break;
    case SP_COLOUR_ID:
      ChangeColours();
      ScheduleRedraw();
      break;
    case SP_DEFAULT_COLOUR_ID: // Delibarate fallthrough
    case BP_PALETTE_CHANGE: 
      if(GetBoolParameter(BP_PALETTE_CHANGE))
	 SetStringParameter(SP_COLOUR_ID, GetStringParameter(SP_DEFAULT_COLOUR_ID));
      break;
    case LP_LANGUAGE_MODEL_ID:
      CreateNCManager();
      break;
    case LP_LINE_WIDTH:
      ScheduleRedraw();
      break;
    case LP_DASHER_FONTSIZE:
      ScheduleRedraw();
      break;
    case SP_INPUT_DEVICE:
      CreateInput();
      break;
    case SP_INPUT_FILTER:
      CreateInputFilter();
      ScheduleRedraw();
      break;
    case BP_DASHER_PAUSED:
      ScheduleRedraw();
      break;
    default:
      break;
    }
  }
  else if(pEvent->m_iEventType == 2) {
    CEditEvent *pEditEvent(static_cast < CEditEvent * >(pEvent));
    
    if(pEditEvent->m_iEditType == 1) {
      strCurrentContext += pEditEvent->m_sText;
      if( strCurrentContext.size() > 20 )
	strCurrentContext = strCurrentContext.substr( strCurrentContext.size() - 20 );

      strTrainfileBuffer += pEditEvent->m_sText;
    }
    else if(pEditEvent->m_iEditType == 2) {
      strCurrentContext = strCurrentContext.substr( 0, strCurrentContext.size() - pEditEvent->m_sText.size());

      strTrainfileBuffer = strTrainfileBuffer.substr( 0, strTrainfileBuffer.size() - pEditEvent->m_sText.size());
    }
  }
  else if(pEvent->m_iEventType == EV_CONTROL) {
    CControlEvent *pControlEvent(static_cast <CControlEvent*>(pEvent));

    switch(pControlEvent->m_iID) {
    case CControlManager::CTL_STOP:
      PauseAt(0,0);
      break;
    case CControlManager::CTL_PAUSE:
      Halt();
      break;
    }
  }
  else if(pEvent->m_iEventType == EV_LOCK) {
    CLockEvent *pLockEvent(static_cast<CLockEvent *>(pEvent));

//     // TODO: Sort this out - at the moment these don't occur in pairs, so the old boolean variable is still needed
//     if(pLockEvent->m_bLock) {
//       if(m_bGlobalLock)
// 	AddLock(0);
//     }
//     else {
//       if(!m_bGlobalLock)
// 	ReleaseLock(0);
//     }
    
    m_bGlobalLock = pLockEvent->m_bLock;
  }
}

void CDasherInterfaceBase::WriteTrainFileFull() {
  WriteTrainFile(strTrainfileBuffer);
  strTrainfileBuffer = "";
}

void CDasherInterfaceBase::WriteTrainFilePartial() {
  // TODO: what if we're midway through a unicode character?
  WriteTrainFile(strTrainfileBuffer.substr(0,100));
  strTrainfileBuffer = strTrainfileBuffer.substr(100);
}

void CDasherInterfaceBase::CreateModel(int iOffset) {
  // Creating a model without a node creation manager is a bad plan
  if(!m_pNCManager)
    return;

  if(m_pDasherModel) {
    delete m_pDasherModel;
    m_pDasherModel = 0;
  }
  
  if(m_deGameModeStrings.size() == 0) {
    m_pDasherModel = new CDasherModel(m_pEventHandler, m_pSettingsStore, m_pNCManager, this, m_pDasherView, iOffset);
  }
  else {
    m_pDasherModel = new CDasherModel(m_pEventHandler, m_pSettingsStore, m_pNCManager, this, m_pDasherView, iOffset, true, m_deGameModeStrings[0]);
  }
}

void CDasherInterfaceBase::CreateNCManager() {
  // TODO: Try and make this work without necessarilty rebuilding the model

  if(!m_AlphIO)
    return;

  int lmID = GetLongParameter(LP_LANGUAGE_MODEL_ID);

  if( lmID == -1 ) 
    return;

 // Train the new language model
    CLockEvent *pEvent;
    
    pEvent = new CLockEvent("Training Dasher", true, 0);
    m_pEventHandler->InsertEvent(pEvent);
    delete pEvent;

    AddLock(0);

    int iOffset;

    if(m_pDasherModel)
      iOffset = m_pDasherModel->GetOffset();
    else
      iOffset = 0; // TODO: Is this right?

    // Delete the old model and create a new one
    if(m_pDasherModel) {
      delete m_pDasherModel;
      m_pDasherModel = 0;
    }

    if(m_pNCManager) {
      delete m_pNCManager;
      m_pNCManager = 0;
    }
      
    if(m_deGameModeStrings.size() == 0) {
      m_pNCManager = new CNodeCreationManager(this, m_pEventHandler, m_pSettingsStore, false, "", m_AlphIO);
    }
    else {
      m_pNCManager = new CNodeCreationManager(this, m_pEventHandler, m_pSettingsStore, true, m_deGameModeStrings[0], m_AlphIO);
    }

    m_Alphabet = m_pNCManager->GetAlphabet();
    
    string T = m_Alphabet->GetTrainingFile();

    int iTotalBytes = 0;
    iTotalBytes += GetFileSize(GetStringParameter(SP_SYSTEM_LOC) + T);
    iTotalBytes += GetFileSize(GetStringParameter(SP_USER_LOC) + T);

    if(iTotalBytes > 0) {
      int iOffset;
      iOffset = TrainFile(GetStringParameter(SP_SYSTEM_LOC) + T, iTotalBytes, 0);
      TrainFile(GetStringParameter(SP_USER_LOC) + T, iTotalBytes, iOffset);
    }
    else {
      CMessageEvent oEvent("No training text is avilable for the selected alphabet. Dasher will function, but it may be difficult to enter text.\nPlease see http://www.dasher.org.uk/alphabets/ for more information.", 0, 0);
      m_pEventHandler->InsertEvent(&oEvent);
    }

    pEvent = new CLockEvent("Training Dasher", false, 0);
    m_pEventHandler->InsertEvent(pEvent);
    delete pEvent;

    ReleaseLock(0);

    // TODO: Eventually we'll not have to pass the NC manager to the model...
    CreateModel(iOffset);
}

// void CDasherInterfaceBase::Start() {
//   // TODO: Clarify the relationship between Start() and
//   // InvalidateContext() - I believe that they essentially do the same
//   // thing
//   PauseAt(0, 0);
//   if(m_pDasherModel != 0) {
//     m_pDasherModel->Start();
//   }
//   if(m_pDasherView != 0) {
// //     m_pDasherView->ResetSum();
// //     m_pDasherView->ResetSumCounter();
// //     m_pDasherView->ResetYAutoOffset();
//   }

// TODO: Reimplement this sometime

//   int iMinWidth;
  
//   if(m_pInputFilter && m_pInputFilter->GetMinWidth(iMinWidth)) {
//     m_pDasherModel->LimitRoot(iMinWidth);
//   }
// }

void CDasherInterfaceBase::PauseAt(int MouseX, int MouseY) {
  SetBoolParameter(BP_DASHER_PAUSED, true);

  // Request a full redraw at the next time step.
  SetBoolParameter(BP_REDRAW, true);

  Dasher::CStopEvent oEvent;
  m_pEventHandler->InsertEvent(&oEvent);

  if (m_pUserLog != NULL)
    m_pUserLog->StopWriting((float) GetNats());
}

void CDasherInterfaceBase::Halt() {
  SetBoolParameter(BP_DASHER_PAUSED, true);

  // This will cause us to reinitialise the frame rate counter - ie we start off slowly
  if(m_pDasherModel != 0)
    m_pDasherModel->Halt();
}

void CDasherInterfaceBase::Unpause(unsigned long Time) {
  SetBoolParameter(BP_DASHER_PAUSED, false);

  if(m_pDasherModel != 0) 
    m_pDasherModel->Reset_framerate(Time);

  Dasher::CStartEvent oEvent;
  m_pEventHandler->InsertEvent(&oEvent);

  ResetNats();
  if (m_pUserLog != NULL)
    m_pUserLog->StartWriting();
}

void CDasherInterfaceBase::CreateInput() {
  if(m_pInput) {
    m_pInput->Deactivate();
    m_pInput->Unref();
  }

  m_pInput = (CDasherInput *)GetModuleByName(GetStringParameter(SP_INPUT_DEVICE));

  if(m_pInput) {
    m_pInput->Ref();
    m_pInput->Activate();
  }

  if(m_pDasherView != 0)
    m_pDasherView->SetInput(m_pInput);
}

void CDasherInterfaceBase::NewFrame(unsigned long iTime, bool bForceRedraw) {
  // Fail if Dasher is locked
  if(m_iCurrentState != ST_NORMAL)
    return;

  bool bChanged(false);

  if(m_pDasherView != 0) {
    if(!GetBoolParameter(BP_TRAINING)) {
      if (m_pUserLog != NULL) {
	
	Dasher::VECTOR_SYMBOL_PROB vAdded;
	int iNumDeleted = 0;
	
	if(m_pInputFilter) {
	  bChanged = m_pInputFilter->Timer(iTime, m_pDasherView, m_pDasherModel, &vAdded, &iNumDeleted);
	}
	
	if (iNumDeleted > 0)
	  m_pUserLog->DeleteSymbols(iNumDeleted);
	if (vAdded.size() > 0)
	  m_pUserLog->AddSymbols(&vAdded);
	
      }
      else {
	if(m_pInputFilter) {
	  bChanged = m_pInputFilter->Timer(iTime, m_pDasherView, m_pDasherModel, 0, 0);
	}
      }
      
      m_pDasherModel->CheckForNewRoot(m_pDasherView);
    }
  }

  // TODO: This is a bit hacky - we really need to sort out the redraw logic
  if((!bChanged && m_bLastChanged) || m_bRedrawScheduled || bForceRedraw) {
    m_pDasherView->Screen()->SetCaptureBackground(true);
    m_pDasherView->Screen()->SetLoadBackground(true);
  }

  Redraw((bChanged || m_bLastChanged) || m_bRedrawScheduled || bForceRedraw);

  m_bLastChanged = bChanged;
  m_bRedrawScheduled = false;

  // This just passes the time through to the framerate tracker, so we
  // know how often new frames are being drawn.
  if(m_pDasherModel != 0)
    m_pDasherModel->NewFrame(iTime);
}

void CDasherInterfaceBase::CheckRedraw() {
  if(m_bRedrawScheduled)
    Redraw(true);

  m_bRedrawScheduled = false;
}

/// Full redraw - renders model, decorations and blits onto display
/// buffer. Not recommended if nothing has changed with the model,
/// otherwise we're wasting effort.

void CDasherInterfaceBase::Redraw(bool bRedrawNodes) {

  if(!m_pDasherView || !m_pDasherModel)
    return;
  
  // TODO: More generic notion of 'layers'?

  if(bRedrawNodes) {
    m_pDasherView->Screen()->SendMarker(0);
    m_pDasherModel->RenderToView(m_pDasherView, true);
  }
  
  m_pDasherView->Screen()->SendMarker(1);
  
  bool bDecorationsChanged(false);

  if(m_pInputFilter) {
    bDecorationsChanged = m_pInputFilter->DecorateView(m_pDasherView);
  }

  bool bActionButtonsChanged(false);
#ifdef EXPERIMENTAL_FEATURES
  bActionButtonsChanged = DrawActionButtons();
#endif

  if(bRedrawNodes || bDecorationsChanged || bActionButtonsChanged)
    m_pDasherView->Display();
}

void CDasherInterfaceBase::ChangeAlphabet() {

  if(GetStringParameter(SP_ALPHABET_ID) == "") {
    SetStringParameter(SP_ALPHABET_ID, m_AlphIO->GetDefault());
    // This will result in ChangeAlphabet() being called again, so
    // exit from the first recursion
    return;
  }
  
  // Send a lock event

  WriteTrainFileFull();

  // Lock Dasher to prevent changes from happening while we're training.

  SetBoolParameter( BP_TRAINING, true );

  CreateNCManager();

  // Let our user log object know about the new alphabet since
  // it needs to convert symbols into text for the log file.
  if (m_pUserLog != NULL)
    m_pUserLog->SetAlphabetPtr(m_Alphabet);

  // Apply options from alphabet

  SetBoolParameter( BP_TRAINING, false );

  //}
}

void CDasherInterfaceBase::ChangeColours() {
  if(!m_ColourIO || !m_DasherScreen)
    return;
 
  // TODO: Make fuction return a pointer directly
  m_DasherScreen->SetColourScheme(&(m_ColourIO->GetInfo(GetStringParameter(SP_COLOUR_ID))));
}

void CDasherInterfaceBase::ChangeScreen(CDasherScreen *NewScreen) {
  m_DasherScreen = NewScreen;
  ChangeColours();

  if(m_pDasherView != 0) {
    m_pDasherView->ChangeScreen(m_DasherScreen);
  } else {
    if(GetLongParameter(LP_VIEW_ID) != -1)
      ChangeView();
  }

  PositionActionButtons();
  ScheduleRedraw();
}

void CDasherInterfaceBase::ChangeView() {
  // TODO: Actually respond to LP_VIEW_ID parameter (although there is only one view at the moment)

  if(m_DasherScreen != 0 && m_pDasherModel != 0) {
    delete m_pDasherView;
    
    m_pDasherView = new CDasherViewSquare(m_pEventHandler, m_pSettingsStore, m_DasherScreen);
 
    if (m_pInput)
      m_pDasherView->SetInput(m_pInput);
  }
}

const CAlphIO::AlphInfo & CDasherInterfaceBase::GetInfo(const std::string &AlphID) {
  return m_AlphIO->GetInfo(AlphID);
}

void CDasherInterfaceBase::SetInfo(const CAlphIO::AlphInfo &NewInfo) {
  m_AlphIO->SetInfo(NewInfo);
}

void CDasherInterfaceBase::DeleteAlphabet(const std::string &AlphID) {
  m_AlphIO->Delete(AlphID);
}

/*
	I've used C style I/O because I found that C++ style I/O bloated
	the Win32 code enormously. The overhead of loading the buffer into
	the string instead of reading straight into a string seems to be
	negligible compared to huge requirements elsewhere.
*/
int CDasherInterfaceBase::TrainFile(std::string Filename, int iTotalBytes, int iOffset) {
  if(Filename == "")
    return 0;
  
  FILE *InputFile;
  if((InputFile = fopen(Filename.c_str(), "r")) == (FILE *) 0)
    return 0;

  AddLock(0);

  const int BufferSize = 1024;
  char InputBuffer[BufferSize];
  string StringBuffer;
  int NumberRead;
  int iTotalRead(0);

  vector < symbol > Symbols;

  CAlphabetManagerFactory::CTrainer * pTrainer = m_pNCManager->GetTrainer();
  do {
    NumberRead = fread(InputBuffer, 1, BufferSize - 1, InputFile);
    InputBuffer[NumberRead] = '\0';
    StringBuffer += InputBuffer;
    bool bIsMore = false;
    if(NumberRead == (BufferSize - 1))
      bIsMore = true;

    Symbols.clear();
    m_Alphabet->GetSymbols(&Symbols, &StringBuffer, bIsMore);

    pTrainer->Train(Symbols);
    iTotalRead += NumberRead;
  
    // TODO: No reason for this to be a pointer (other than cut/paste laziness!)
    CLockEvent *pEvent;
    pEvent = new CLockEvent("Training Dasher", true, static_cast<int>((100.0 * (iTotalRead + iOffset))/iTotalBytes));
    m_pEventHandler->InsertEvent(pEvent);
    delete pEvent;

  } while(NumberRead == BufferSize - 1);

  delete pTrainer;

  fclose(InputFile);

  ReleaseLock(0);

  return iTotalRead;
}

void CDasherInterfaceBase::GetFontSizes(std::vector <int >*FontSizes) const {
  FontSizes->push_back(20);
  FontSizes->push_back(14);
  FontSizes->push_back(11);
  FontSizes->push_back(40);
  FontSizes->push_back(28);
  FontSizes->push_back(22);
  FontSizes->push_back(80);
  FontSizes->push_back(56);
  FontSizes->push_back(44);
}

double CDasherInterfaceBase::GetCurCPM() {
  //
  return 0;
}

double CDasherInterfaceBase::GetCurFPS() {
  //
  return 0;
}

// int CDasherInterfaceBase::GetAutoOffset() {
//   if(m_pDasherView != 0) {
//     return m_pDasherView->GetAutoOffset();
//   }
//   return -1;
// }

double CDasherInterfaceBase::GetNats() const {
  if(m_pDasherModel)
    return m_pDasherModel->GetNats();
  else
    return 0.0;
}

void CDasherInterfaceBase::ResetNats() {
  if(m_pDasherModel)
    m_pDasherModel->ResetNats();
}


// TODO: Check that none of this needs to be reimplemented

// void CDasherInterfaceBase::InvalidateContext(bool bForceStart) {
//   m_pDasherModel->m_strContextBuffer = "";

//   Dasher::CEditContextEvent oEvent(10);
//   m_pEventHandler->InsertEvent(&oEvent);

//    std::string strNewContext(m_pDasherModel->m_strContextBuffer);

//   // We keep track of an internal context and compare that to what
//   // we are given - don't restart Dasher if nothing has changed.
//   // This should really be integrated with DasherModel, which
//   // probably will be the case when we start to deal with being able
//   // to back off indefinitely. For now though we'll keep it in a
//   // separate string.

//    int iContextLength( 6 ); // The 'important' context length - should really get from language model

//    // FIXME - use unicode lengths

//    if(bForceStart || (strNewContext.substr( std::max(static_cast<int>(strNewContext.size()) - iContextLength, 0)) != strCurrentContext.substr( std::max(static_cast<int>(strCurrentContext.size()) - iContextLength, 0)))) {

//      if(m_pDasherModel != NULL) {
//        // TODO: Reimplement this
//        //       if(m_pDasherModel->m_bContextSensitive || bForceStart) {
//        {
//  	m_pDasherModel->SetContext(strNewContext);
//  	PauseAt(0,0);
//        }
//      }
    
//      strCurrentContext = strNewContext;
//      WriteTrainFileFull();
//    }

//    if(bForceStart) {
//      int iMinWidth;

//      if(m_pInputFilter && m_pInputFilter->GetMinWidth(iMinWidth)) {
//        m_pDasherModel->LimitRoot(iMinWidth);
//      }
//    }

//    if(m_pDasherView)
//      while( m_pDasherModel->CheckForNewRoot(m_pDasherView) ) {
//        // Do nothing
//      }

//    ScheduleRedraw();
// }

// TODO: Fix this

std::string CDasherInterfaceBase::GetContext(int iStart, int iLength) {
  m_strContext = "";
  
  CEditContextEvent oEvent(iStart, iLength);
  m_pEventHandler->InsertEvent(&oEvent);

  return m_strContext;
}

void CDasherInterfaceBase::SetContext(std::string strNewContext) {
  m_strContext = strNewContext;
}

// Control mode stuff

void CDasherInterfaceBase::RegisterNode( int iID, const std::string &strLabel, int iColour ) {
  m_pNCManager->RegisterNode(iID, strLabel, iColour);
}

void CDasherInterfaceBase::ConnectNode(int iChild, int iParent, int iAfter) {
  m_pNCManager->ConnectNode(iChild, iParent, iAfter);
}

void CDasherInterfaceBase::DisconnectNode(int iChild, int iParent) {
  m_pNCManager->DisconnectNode(iChild, iParent);
}

void CDasherInterfaceBase::SetBoolParameter(int iParameter, bool bValue) {
  m_pSettingsStore->SetBoolParameter(iParameter, bValue);
};

void CDasherInterfaceBase::SetLongParameter(int iParameter, long lValue) { 
  m_pSettingsStore->SetLongParameter(iParameter, lValue);
};

void CDasherInterfaceBase::SetStringParameter(int iParameter, const std::string & sValue) {
  PreSetNotify(iParameter, sValue);
  m_pSettingsStore->SetStringParameter(iParameter, sValue);
};

bool CDasherInterfaceBase::GetBoolParameter(int iParameter) {
  return m_pSettingsStore->GetBoolParameter(iParameter);
}

long CDasherInterfaceBase::GetLongParameter(int iParameter) {
  return m_pSettingsStore->GetLongParameter(iParameter);
}

std::string CDasherInterfaceBase::GetStringParameter(int iParameter) {
  return m_pSettingsStore->GetStringParameter(iParameter);
}

void CDasherInterfaceBase::ResetParameter(int iParameter) {
  m_pSettingsStore->ResetParameter(iParameter);
}

// We need to be able to get at the UserLog object from outside the interface
CUserLogBase* CDasherInterfaceBase::GetUserLogPtr() {
  return m_pUserLog;
}

void CDasherInterfaceBase::KeyDown(int iTime, int iId) {
  if(m_iCurrentState != ST_NORMAL)
    return;

  if(m_pInputFilter && !GetBoolParameter(BP_TRAINING)) {
    m_pInputFilter->KeyDown(iTime, iId, m_pDasherModel, m_pUserLog);
  }

  if(m_pInput && !GetBoolParameter(BP_TRAINING)) {
    m_pInput->KeyDown(iTime, iId);
  }
}

void CDasherInterfaceBase::KeyUp(int iTime, int iId) {
  if(m_iCurrentState != ST_NORMAL)
    return;

  if(m_pInputFilter && !GetBoolParameter(BP_TRAINING)) {
    m_pInputFilter->KeyUp(iTime, iId, m_pDasherModel);
  }

  if(m_pInput && !GetBoolParameter(BP_TRAINING)) {
    m_pInput->KeyUp(iTime, iId);
  }
}

void CDasherInterfaceBase::CreateInputFilter()
{
  if(m_pInputFilter) {
    m_pInputFilter->Deactivate();
    m_pInputFilter->Unref();
    m_pInputFilter = NULL;
  }

  m_pInputFilter = (CInputFilter *)GetModuleByName(GetStringParameter(SP_INPUT_FILTER));

  if(m_pInputFilter) {
    m_pInputFilter->Ref();
    m_pInputFilter->Activate();
  }
  else {
    // Fall back to a sensible alternative if for some reason the
    // current choice isn't valid.
    if(GetStringParameter(SP_INPUT_FILTER) != "Normal Control")
      SetStringParameter(SP_INPUT_FILTER, "Normal Control");
  }
}

void CDasherInterfaceBase::RegisterFactory(CModuleFactory *pFactory) {
  m_oModuleManager.RegisterFactory(pFactory);
}
 
CDasherModule *CDasherInterfaceBase::GetModule(ModuleID_t iID) {
   return m_oModuleManager.GetModule(iID);
}

CDasherModule *CDasherInterfaceBase::GetModuleByName(const std::string &strName) {
   return m_oModuleManager.GetModuleByName(strName);
}

void CDasherInterfaceBase::CreateFactories() {
  RegisterFactory(new CWrapperFactory(m_pEventHandler, m_pSettingsStore, new CDefaultFilter(m_pEventHandler, m_pSettingsStore, this, m_pDasherModel,3, _("Normal Control"))));
  RegisterFactory(new CWrapperFactory(m_pEventHandler, m_pSettingsStore, new COneDimensionalFilter(m_pEventHandler, m_pSettingsStore, this, m_pDasherModel)));
  RegisterFactory(new CWrapperFactory(m_pEventHandler, m_pSettingsStore, new CEyetrackerFilter(m_pEventHandler, m_pSettingsStore, this, m_pDasherModel)));
  RegisterFactory(new CWrapperFactory(m_pEventHandler, m_pSettingsStore, new CClickFilter(m_pEventHandler, m_pSettingsStore, this)));
  RegisterFactory(new CWrapperFactory(m_pEventHandler, m_pSettingsStore, new CDynamicFilter(m_pEventHandler, m_pSettingsStore, this)));
  RegisterFactory(new CWrapperFactory(m_pEventHandler, m_pSettingsStore, new CTwoButtonDynamicFilter(m_pEventHandler, m_pSettingsStore, this, 14, 1, _("Two Button Dynamic Mode"))));
  // TODO: specialist factory for button mode
  RegisterFactory(new CWrapperFactory(m_pEventHandler, m_pSettingsStore, new CDasherButtons(m_pEventHandler, m_pSettingsStore, this, 5, 1, true,8, _("Menu Mode"))));
  RegisterFactory(new CWrapperFactory(m_pEventHandler, m_pSettingsStore, new CDasherButtons(m_pEventHandler, m_pSettingsStore, this, 3, 0, false,10, _("Direct Mode"))));
  //  RegisterFactory(new CWrapperFactory(m_pEventHandler, m_pSettingsStore, new CDasherButtons(m_pEventHandler, m_pSettingsStore, this, 4, 0, false,11, "Buttons 3")));
  RegisterFactory(new CWrapperFactory(m_pEventHandler, m_pSettingsStore, new CDasherButtons(m_pEventHandler, m_pSettingsStore, this, 3, 3, false,12, _("Alternating Direct Mode"))));
  RegisterFactory(new CWrapperFactory(m_pEventHandler, m_pSettingsStore, new CDasherButtons(m_pEventHandler, m_pSettingsStore, this, 4, 2, false,13, _("Compass Mode"))));
  RegisterFactory(new CWrapperFactory(m_pEventHandler, m_pSettingsStore, new CStylusFilter(m_pEventHandler, m_pSettingsStore, this, m_pDasherModel,15, _("Stylus Control"))));

}

void CDasherInterfaceBase::GetPermittedValues(int iParameter, std::vector<std::string> &vList) {
  // TODO: Deprecate direct calls to these functions
  switch (iParameter) {
  case SP_ALPHABET_ID:
    if(m_AlphIO)
      m_AlphIO->GetAlphabets(&vList);
    break;
  case SP_COLOUR_ID:
    if(m_ColourIO)
      m_ColourIO->GetColours(&vList);
    break;
  case SP_INPUT_FILTER:
    m_oModuleManager.ListModules(1, vList);
    break;
  case SP_INPUT_DEVICE:
    m_oModuleManager.ListModules(0, vList);
    break;
  }
}

void CDasherInterfaceBase::StartShutdown() {
  ChangeState(TR_SHUTDOWN);
}

bool CDasherInterfaceBase::GetModuleSettings(const std::string &strName, SModuleSettings **pSettings, int *iCount) {
  return GetModuleByName(strName)->GetSettings(pSettings, iCount);
}

void CDasherInterfaceBase::SetupActionButtons() {
  m_vLeftButtons.push_back(new CActionButton(this, "Exit", true));
  m_vLeftButtons.push_back(new CActionButton(this, "Preferences", false));
  m_vLeftButtons.push_back(new CActionButton(this, "Help", false));
  m_vLeftButtons.push_back(new CActionButton(this, "About", false));
}

void CDasherInterfaceBase::DestroyActionButtons() {
  // TODO: implement and call this
}

void CDasherInterfaceBase::PositionActionButtons() {
  if(!m_DasherScreen)
    return;

  int iCurrentOffset(16);

  for(std::vector<CActionButton *>::iterator it(m_vLeftButtons.begin()); it != m_vLeftButtons.end(); ++it) {
    (*it)->SetPosition(16, iCurrentOffset, 32, 32);
    iCurrentOffset += 48;
  }

  iCurrentOffset = 16;

  for(std::vector<CActionButton *>::iterator it(m_vRightButtons.begin()); it != m_vRightButtons.end(); ++it) {
    (*it)->SetPosition(m_DasherScreen->GetWidth() - 144, iCurrentOffset, 128, 32);
    iCurrentOffset += 48;
  }
}

bool CDasherInterfaceBase::DrawActionButtons() {
  if(!m_DasherScreen)
    return false;

  bool bVisible(GetBoolParameter(BP_DASHER_PAUSED));

  bool bRV(bVisible != m_bOldVisible);
  m_bOldVisible = bVisible;

  for(std::vector<CActionButton *>::iterator it(m_vLeftButtons.begin()); it != m_vLeftButtons.end(); ++it)
    (*it)->Draw(m_DasherScreen, bVisible);

  for(std::vector<CActionButton *>::iterator it(m_vRightButtons.begin()); it != m_vRightButtons.end(); ++it)
    (*it)->Draw(m_DasherScreen, bVisible);

  return bRV;
}


void CDasherInterfaceBase::HandleClickUp(int iTime, int iX, int iY) {
#ifdef EXPERIMENTAL_FEATURES
  bool bVisible(GetBoolParameter(BP_DASHER_PAUSED));

  for(std::vector<CActionButton *>::iterator it(m_vLeftButtons.begin()); it != m_vLeftButtons.end(); ++it) {
    if((*it)->HandleClickUp(iTime, iX, iY, bVisible))
      return;
  }

  for(std::vector<CActionButton *>::iterator it(m_vRightButtons.begin()); it != m_vRightButtons.end(); ++it) {
    if((*it)->HandleClickUp(iTime, iX, iY, bVisible))
      return;
  }
#endif

  KeyUp(iTime, 100);
}

void CDasherInterfaceBase::HandleClickDown(int iTime, int iX, int iY) {
#ifdef EXPERIMENTAL_FEATURES
  bool bVisible(GetBoolParameter(BP_DASHER_PAUSED));

  for(std::vector<CActionButton *>::iterator it(m_vLeftButtons.begin()); it != m_vLeftButtons.end(); ++it) {
    if((*it)->HandleClickDown(iTime, iX, iY, bVisible))
      return;
  }

  for(std::vector<CActionButton *>::iterator it(m_vRightButtons.begin()); it != m_vRightButtons.end(); ++it) {
    if((*it)->HandleClickDown(iTime, iX, iY, bVisible))
      return;
  }
#endif

  KeyDown(iTime, 100);
}


void CDasherInterfaceBase::ExecuteCommand(const std::string &strName) {
  // TODO: Pointless - just insert event directly

  CCommandEvent *pEvent = new CCommandEvent(strName);
  m_pEventHandler->InsertEvent(pEvent);
  delete pEvent;
}

double CDasherInterfaceBase::GetFramerate() {
  if(m_pDasherModel)
    return(m_pDasherModel->Framerate());
  else
    return 0.0;
}

int CDasherInterfaceBase::GetRenderCount() {
  if(m_pDasherView)
    return(m_pDasherView->GetRenderCount());
  else
    return 0;
}

void CDasherInterfaceBase::AddActionButton(const std::string &strName) {
  m_vRightButtons.push_back(new CActionButton(this, strName, false));
}


void CDasherInterfaceBase::OnUIRealised() {
  StartTimer();
  ChangeState(TR_UI_INIT);
}


void CDasherInterfaceBase::ChangeState(ETransition iTransition) {
  static EState iTransitionTable[ST_NUM][TR_NUM] = {
    {ST_MODEL, ST_UI, ST_FORBIDDEN, ST_FORBIDDEN, ST_FORBIDDEN},
    {ST_FORBIDDEN, ST_NORMAL, ST_FORBIDDEN, ST_FORBIDDEN, ST_FORBIDDEN},
    {ST_NORMAL, ST_FORBIDDEN, ST_FORBIDDEN, ST_FORBIDDEN, ST_FORBIDDEN},
    {ST_FORBIDDEN, ST_FORBIDDEN, ST_LOCKED, ST_FORBIDDEN, ST_SHUTDOWN},
    {ST_FORBIDDEN, ST_FORBIDDEN, ST_FORBIDDEN, ST_NORMAL, ST_FORBIDDEN},
    {ST_FORBIDDEN, ST_FORBIDDEN, ST_FORBIDDEN, ST_FORBIDDEN, ST_FORBIDDEN}
  };

  EState iNewState(iTransitionTable[m_iCurrentState][iTransition]);

  if(iNewState != ST_FORBIDDEN) {
    LeaveState(m_iCurrentState);
    EnterState(iNewState);

    m_iCurrentState = iNewState;
  }
}

void CDasherInterfaceBase::LeaveState(EState iState) {

}

void CDasherInterfaceBase::EnterState(EState iState) {
  switch(iState) {
  case ST_SHUTDOWN:
    ShutdownTimer();  
    WriteTrainFileFull();
    break;
  default:
    // Not handled
    break;
  }
}

void CDasherInterfaceBase::AddLock(int iLockFlags) {
  if(m_iLockCount == 0)
    ChangeState(TR_LOCK);

  ++m_iLockCount;
}

void CDasherInterfaceBase::ReleaseLock(int iLockFlags) {
  if(m_iLockCount > 0)
    --m_iLockCount;

  if(m_iLockCount == 0)
    ChangeState(TR_UNLOCK);
}

void CDasherInterfaceBase::SetBuffer(int iOffset) { 
  CreateModel(iOffset);
}

void CDasherInterfaceBase::UnsetBuffer() {
  // TODO: Write training file?
  if(m_pDasherModel)
    delete m_pDasherModel;

  m_pDasherModel = 0;
}

void CDasherInterfaceBase::SetOffset(int iOffset) {
  if(m_pDasherModel)
    m_pDasherModel->SetOffset(iOffset, m_pDasherView);
}

void CDasherInterfaceBase::SetControlOffset(int iOffset) {
  if(m_pDasherModel)
    m_pDasherModel->SetControlOffset(iOffset);
}