#pragma once

#include "../DasherCore/Win32/DasherInterface.h"
#include "Common/WinHelper.h"
#include "DasherWindow.h"
#include "Widgets/Canvas.h"
#include "Widgets/Slidebar.h"
#include "EditWrapper.h"

namespace Dasher {
  class CDasher;
} 

class Dasher::CDasher : public CDasherInterface {
public:
  CDasher(HWND Parent);
  ~CDasher(void);

  // The following functions will not be part of the final interface

  CCanvas *GetCanvas() {
    return m_pCanvas;
  }

  CSlidebar *GetSlidebar() {
    return m_pSlidebar;
  }

  void SetEdit(CDashEditbox * pEdit) {
    m_pEditWrapper->SetEventHandler(pEdit);
  }

  void HandleParameterNotification(int iParameter);

private:

  void AddFiles(Tstring Alphabets, Tstring Colours, CDasherInterface * Interface);

  CCanvas *m_pCanvas;
  CSlidebar *m_pSlidebar;
  CEditWrapper *m_pEditWrapper;

  HANDLE m_workerThread;        // Handle to our worker thread that periodically checks on user's activities
  bool m_bWorkerShutdown;       // Set to true when the worker should terminate

  static DWORD WINAPI WorkerThread(LPVOID lpParam);     // Spins around and sends WM_DASHER_TIMER message
  void ShutdownWorkerThread();  // Called when we want the worker thread to stop
  void OnTimer();               // Does the periodic work

  HWND m_hParent;

};